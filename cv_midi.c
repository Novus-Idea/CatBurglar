#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cv_midi.h"

/* tiny send helpers */
static void midi_send(u8 b)                         { mpu_write_data(MPU_BASE, b); }
static void midi_note_on (u8 ch,u8 n,u8 v)          { midi_send((u8)(0x90|(ch&0x0F))); midi_send(n); midi_send(v); }
static void midi_note_off(u8 ch,u8 n,u8 v)          { midi_send((u8)(0x80|(ch&0x0F))); midi_send(n); midi_send(v); }
static void midi_prog    (u8 ch,u8 p)               { midi_send((u8)(0xC0|(ch&0x0F))); midi_send(p); }
static void midi_cc      (u8 ch,u8 cc,u8 val)       { midi_send((u8)(0xB0|(ch&0x0F))); midi_send(cc); midi_send(val); }


/* read big-endian 16/32 */
static u16 be16(const u8* p){ return (u16)((p[0]<<8)|p[1]); }
static u32 be32(const u8* p){ return ((u32)p[0]<<24)|((u32)p[1]<<16)|((u32)p[2]<<8)|p[3]; }

/* read VLQ; returns value, advances *pp; bounds-safe */
static int read_vlq(const u8* base, u32 end, u32* pp, u32* out){
	u32 pos=*pp, v=0, i;
    for(i=0;i<4;i++){
		if(pos>=end) return 0;
		v = (v<<7) | (base[pos]&0x7F);
		if(!(base[pos++] & 0x80)) { *pp=pos; *out=v; return 1; }
	}
	/* 5th byte allowed by spec */
	if(pos>=end) return 0;
	v = (v<<7) | (base[pos++]&0x7F);
	*pp=pos; *out=v; return 1;
}

/* parse next delta into player->next_delta_ticks, return 1 ok, 0 error/eot */
static int smf0_read_delta(Smf0Player* s){
	u32 d;
	if(!read_vlq(s->data, s->trk_end, &s->pos, &d)) return 0;
	s->next_delta_ticks = d;
	return 1;
}

/* skip N bytes safely */
static int skip_bytes(Smf0Player* s, u32 n){
	if(s->pos + n > s->trk_end) return 0;
	s->pos += n; return 1;
}

/* emit one MIDI/meta/sysEx event at s->pos (delta already consumed).
   returns 1 if an event was processed, 0 if end-of-track reached or error. */
static int smf0_emit_event(Smf0Player* s){
	u8 st, b1, b2;
	u32 len, pos;

	if(s->pos >= s->trk_end) return 0;

	st = s->data[s->pos++];

	if(st < 0x80){
		/* running status: st is actually first data byte */
		if(s->running_status < 0x80) return 0;
		/* re-use previous status */
		b1 = st;
		st = s->running_status;
		/* figure expected data length */
		if((st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0){
			/* one data byte total; b1 already read */
			midi_send(st); midi_send(b1);
			s->running_status = st;
			return 1;
		} else {
			/* two data bytes total: need one more */
			if(s->pos >= s->trk_end) return 0;
			b2 = s->data[s->pos++];
			/* treat NoteOn v=0 as NoteOff for compatibility */
			if((st & 0xF0)==0x90 && b2==0) { midi_send((u8)(0x80|(st&0x0F))); midi_send(b1); midi_send(0); }
			else                            { midi_send(st); midi_send(b1); midi_send(b2); }
			s->running_status = st;
			return 1;
		}
	}

	if(st >= 0x80 && st <= 0xEF){
		s->running_status = st;
		if((st & 0xF0) == 0xC0 || (st & 0xF0) == 0xD0){
			if(s->pos >= s->trk_end) return 0;
			b1 = s->data[s->pos++];
			midi_send(st); midi_send(b1);
			return 1;
		} else {
			if(s->pos+1 >= s->trk_end) return 0;
			b1 = s->data[s->pos++];
			b2 = s->data[s->pos++];
			if((st & 0xF0)==0x90 && b2==0) { midi_send((u8)(0x80|(st&0x0F))); midi_send(b1); midi_send(0); }
			else                            { midi_send(st); midi_send(b1); midi_send(b2); }
			return 1;
		}
	}

	/* System / Meta */
	s->running_status = 0;

	if(st == 0xFF){
		/* Meta: type, length VLQ, data */
		if(s->pos >= s->trk_end) return 0;
		b1 = s->data[s->pos++]; /* meta type */
		if(!read_vlq(s->data, s->trk_end, &s->pos, &len)) return 0;

		/* Handle tempo and end-of-track; skip others */
		if(b1 == 0x51 && len == 3){
			/* tempo: microseconds per quarter note */
			if(s->pos + 3 > s->trk_end) return 0;
			s->tempo_us_qn = ((u32)s->data[s->pos]<<16) | ((u32)s->data[s->pos+1]<<8) | (u32)s->data[s->pos+2];
		} else if(b1 == 0x2F){
			/* End of track: loop */
			/* length should be 0, but skip whatever present safely */
			/* reset to start of track for looping */
		}
		if(!skip_bytes(s, len)) return 0;

		if(b1 == 0x2F){
			/* loop: rewind to track start and read next delta */
			s->pos = s->trk_start;
			s->running_status = 0;
			if(!smf0_read_delta(s)) return 0;
		}
		return 1;
	}
	else if(st == 0xF0 || st == 0xF7){
		/* SysEx: length VLQ + data (just skip) */
		if(!read_vlq(s->data, s->trk_end, &s->pos, &len)) return 0;
		if(!skip_bytes(s, len)) return 0;
		return 1;
	}

	/* Unknown */
	return 0;
}

/* convert ticks to microseconds with current tempo/division */
static u32 ticks_to_us(Smf0Player* s, u32 ticks){
	/* only PPQ supported (positive division) */
	if(s->division == 0) return 0;
	/* us = ticks * tempo_us_qn / division */
	return (u32)((((u32)ticks) * (u32)s->tempo_us_qn) / (u32)s->division);
}

/* public API ----------------------------------------------- */

/* returns 1 on success, 0 on failure */
static int smf0_load(const char* path, Smf0Player* s){
	FILE* f;
	u8 hdr[14];
	u32 len, trklen;

	memset(s, 0, sizeof(*s));
	s->tempo_us_qn = 500000UL; /* default 120 BPM */

	f = fopen(path, "rb");
	if(!f) return 0;
	if(fread(hdr,1,14,f) != 14){ fclose(f); return 0; }
	if(memcmp(hdr, "MThd", 4)!=0){ fclose(f); return 0; }
	len = be32(hdr+4);
	if(len != 6){ fclose(f); return 0; }
	if(be16(hdr+8) != 0){ fclose(f); return 0; }       /* format must be 0 */
	if(be16(hdr+10) != 1){ fclose(f); return 0; }      /* one track */
	s->division = be16(hdr+12);
	if((s->division & 0x8000) != 0){ fclose(f); return 0; } /* SMPTE not supported */

	/* read MTrk header */
	if(fread(hdr,1,8,f) != 8){ fclose(f); return 0; }
	if(memcmp(hdr, "MTrk", 4)!=0){ fclose(f); return 0; }
	trklen = be32(hdr+4);

	/* read whole track chunk into memory */
	s->size = trklen;
	s->data = (u8*)malloc(s->size);
	if(!s->data){ fclose(f); return 0; }
	if(fread(s->data,1,s->size,f) != s->size){ free(s->data); s->data=0; fclose(f); return 0; }
	fclose(f);

	s->trk_start = 0;
	s->trk_end   = trklen;
	s->pos       = s->trk_start;
	s->running_status = 0;
	s->acc_us    = 0;
	s->active    = 1;

	if(!smf0_read_delta(s)){ free(s->data); s->data=0; return 0; }
	return 1;
}

static void smf0_unload(Smf0Player* s){
	if(s->data){ free(s->data); s->data=0; }
	s->active=0;
}

/* call once before playback */
static void smf0_start(Smf0Player* s){
	/* soft reset synth state if you like */
	/* example: set all channels volume/pan reasonable */
	{
		int ch;
		for(ch=0; ch<16; ++ch){
			midi_cc((u8)ch, 7, 100);
			midi_cc((u8)ch,10, 64);
		}
	}
}

/* call every frame: pass elapsed microseconds since last frame (˜14286 for VGA) */
static void smf0_tick(Smf0Player* s, u32 elapsed_us){
	u32 need_us;
	if(!s || !s->active) return;

	s->acc_us += elapsed_us;

	for(;;){
		need_us = ticks_to_us(s, s->next_delta_ticks);
		if(s->acc_us < need_us) break;

		/* consume delta and emit one event */
		s->acc_us -= need_us;

		/* events with zero delta can bunch up; emit until a nonzero delta appears */
		do {
			if(!smf0_emit_event(s)){ s->active=0; return; }
			if(!smf0_read_delta(s)){ s->active=0; return; }
		} while(s->next_delta_ticks == 0);
	}
}

/* optional: send All Notes Off on quit */
static void smf0_all_notes_off(void){
	int ch;
	for(ch=0; ch<16; ++ch) midi_cc((u8)ch, 123, 0);
}
