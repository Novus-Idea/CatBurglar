/* ----------- C89 SMF Type-0 MIDI loader/player over MPU-401 ----------- */
#ifndef CV_MIDI_H
#define CV_MIDI_H

/* stdint-ish */
typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned long  u32;

#ifdef __cplusplus
extern C
{
#endif
    /* DPMI bios mapping functions: MIDI */
	void mpu_init_uart(unsigned short base);
	void mpu_write_data(unsigned short base, unsigned char b);
	void mpu_write_cmd (unsigned short base, unsigned char cmd);
#ifdef __cplusplus
}
#endif
/* pick the right base for your card; DOSBox default is 0x330 */
#ifndef MPU_BASE
#define MPU_BASE 0x0330
#endif
/* tiny send helpers */
static void midi_send(u8 b);
static void midi_note_on (u8 ch,u8 n,u8 v);
static void midi_note_off(u8 ch,u8 n,u8 v);
static void midi_prog    (u8 ch,u8 p)     ;
static void midi_cc      (u8 ch,u8 cc,u8 val);

/* ----------- SMF player state ----------- */
typedef struct {
    u8*  data;
    u32  size;

    u32  trk_start;
    u32  trk_end;
    u32  pos;

    u16  division;              /* ticks per quarter (PPQ), only positive PPQ supported */
    u32  tempo_us_qn;           /* microseconds per quarter note (default 500000 = 120 BPM) */

    u32  next_delta_ticks;      /* ticks until next event */
    u8   running_status;

    u32  acc_us;                /* accumulated microseconds since last event */
    int  active;                /* 1=playing, 0=stopped (we loop on EOT) */
} Smf0Player;

/* read big-endian 16/32 */
static u16 be16(const u8* p);
static u32 be32(const u8* p);
static int read_vlq(const u8* base, u32 end, u32* pp, u32* out);
static int smf0_read_delta(Smf0Player* s);
static int skip_bytes(Smf0Player* s, u32 n);
static int smf0_emit_event(Smf0Player* s);
static u32 ticks_to_us(Smf0Player* s, u32 ticks);
static int smf0_load(const char* path, Smf0Player* s);
static void smf0_unload(Smf0Player* s);
static void smf0_start(Smf0Player* s);
static void smf0_tick(Smf0Player* s, u32 elapsed_us);
static void smf0_all_notes_off(void);
#endif	/* CV_MIDI_H */
