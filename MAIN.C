/* main.c DPMI mode-13h mini game (Cat Burglar): Title + GameOver + Tiny Font */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "cv_midi.h"

#define FRAME_US	14286UL

typedef signed long s32;

#ifdef __BORLANDC__
#pragma option -vi-
#pragma option -5
#endif

#ifdef __cplusplus
extern "C"
{
#endif
	/* DPMI bios mapping functions: CORE */
	void dpmi_rm_int(u16 intno,
					 u32 *eax, u32 *ebx, u32 *ecx, u32 *edx,
					 u32 *esi, u32 *edi, u16 *ds, u16 *es, u16 *flags);

	/* DPMI bios mapping functions: VGA */
	void vga_blit(const void *src, u32 nbytes);
	void vga_set_palette256(const unsigned char *dac3x256);
	void vsync_wait(void);

	/* DPMI bios mapping functions: KEYBOARD */
	int kbd_get_byte(unsigned char *outByte);	   /* kept but unused in gameplay */
	int kbd_bda_pop_word(unsigned short *outWord); /* BIOS ring buffer */
#ifdef __cplusplus
}
#endif

/* ============================================================
   BIOS helpers
   ============================================================ */
static void set_mode13(void)
{
	u32 ax = 0x00000013, bx = 0, cx = 0, dx = 0, si = 0, di = 0;
	u16 ds = 0, es = 0, fl = 0;
	dpmi_rm_int(0x10, &ax, &bx, &cx, &dx, &si, &di, &ds, &es, &fl);
}
static void set_textmode(void)
{
	u32 ax = 0x00000003, bx = 0, cx = 0, dx = 0, si = 0, di = 0;
	u16 ds = 0, es = 0, fl = 0;
	dpmi_rm_int(0x10, &ax, &bx, &cx, &dx, &si, &di, &ds, &es, &fl);
}

/* ============================================================
   Palette (file -> DAC via BIOS block call, works in your setup)
   ============================================================ */
static int hex2nib(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return 10 + (c - 'A');
	if (c >= 'a' && c <= 'f')
		return 10 + (c - 'a');
	return 0;
}
static int parse_hex8(const char *s)
{
	int hi = hex2nib(s[0]), lo = hex2nib(s[1]);
	return (hi << 4) | lo;
}
static u8 to_dac6(int v8)
{
	int v = (v8 * 63 + 127) / 255;
	if (v < 0)
		v = 0;
	if (v > 63)
		v = 63;
	return (u8)v;
}

static int load_palette_and_apply(const char *path)
{
	static u8 dac[256 * 3];
	FILE *f = fopen(path, "rb");
	char line[128];
	int count = 0;
	int lastR = 0, lastG = 0, lastB = 0;

	if (!f)
		return 0;

	while (count < 256 && fgets(line, sizeof(line), f))
	{
		int r = -1, g = -1, b = -1;
		/* trim leading spaces */
		char *s = line;
		while (*s == ' ' || *s == '\t' || *s == '\r')
			++s;

		if (*s == '#')
		{
			/* #RRGGBB */
			if (s[1] && s[2] && s[3] && s[4] && s[5] && s[6])
			{
				int R = (hex2nib(s[1]) << 4) | hex2nib(s[2]);
				int G = (hex2nib(s[3]) << 4) | hex2nib(s[4]);
				int B = (hex2nib(s[5]) << 4) | hex2nib(s[6]);
				r = R;
				g = G;
				b = B;
			}
		}
		else
		{
			/* allow: R G B (0..255) */
			int R = 0, G = 0, B = 0;
			if (3 == sscanf(s, "%d %d %d", &R, &G, &B))
			{
				r = R;
				g = G;
				b = B;
			}
		}

		if (r >= 0)
		{
			dac[count * 3 + 0] = to_dac6(r);
			dac[count * 3 + 1] = to_dac6(g);
			dac[count * 3 + 2] = to_dac6(b);
			lastR = r;
			lastG = g;
			lastB = b;
			++count;
		}
	}
	fclose(f);

	/* If fewer than 256 lines, pad with the last valid color (or black if none) */
	if (count == 0)
		return 0;
	while (count < 256)
	{
		dac[count * 3 + 0] = to_dac6(lastR);
		dac[count * 3 + 1] = to_dac6(lastG);
		dac[count * 3 + 2] = to_dac6(lastB);
		++count;
	}

	vga_set_palette256(dac);
	return 1;
}
static void set_test_palette_bios(void)
{
	static u8 dac[256 * 3];
	int i, j = 0;
	for (i = 0; i < 256; i++)
	{
		u8 r = (u8)(((i >> 5) & 7) * 9), g = (u8)(((i >> 2) & 7) * 9), b = (u8)((i & 3) * 21);
		dac[j++] = r;
		dac[j++] = g;
		dac[j++] = b;
	}
	{
		u32 ax = 0x00101200, bx = 0, cx = 256, dx = 0, si = (u32)dac, di = 0;
		u16 ds = 0, es = 0, fl = 0;
		dpmi_rm_int(0x10, &ax, &bx, &cx, &dx, &si, &di, &ds, &es, &fl);
	}
}
/* ============================================================
   RAW loader (8bpp). Accepts exact size or 2x (16bpp pairs).
   ============================================================ */
static int load_raw8(const char *path, u8 *dst, u32 w, u32 h)
{
    FILE *f = fopen(path, "rb");
    long sz;
    u8 *tmp;
    u32 need = w * h, i;
    if (!f)
        return 0;
	fseek(f, 0, SEEK_END);
    sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    tmp = (u8 *)malloc(sz > 0 ? (u32)sz : 1);
    if (!tmp)
    {
        fclose(f);
        return 0;
    }
    if (fread(tmp, 1, (u32)sz, f) != (u32)sz)
    {
        free(tmp);
		fclose(f);
        return 0;
	}
    fclose(f);
    if ((u32)sz == need)
    {
        memcpy(dst, tmp, need);
        free(tmp);
        return 1;
    }
    if ((u32)sz == need * 2u)
    {
		for (i = 0; i < need; ++i)
            dst[i] = tmp[i * 2u];
        free(tmp);
        return 1;
    }
    free(tmp);
    return 0;
}
/* ============================================================
   Sprites (index 0 transparent) + simple rects
   ============================================================ */
typedef struct
{
	u32 w, h;
	u8 *pix;
} Sprite;
static void sprite_free(Sprite *s)
{
	if (s && s->pix)
	{
		free(s->pix);
		s->pix = 0;
	}
}
static int sprite_load(Sprite *s, const char *raw, u32 w, u32 h)
{
	s->w = w;
	s->h = h;
	s->pix = (u8 *)malloc(w * h);
	if (!s->pix)
		return 0;
	if (!load_raw8(raw, s->pix, w, h))
	{
		free(s->pix);
		s->pix = 0;
		return 0;
	}
	return 1;
}
static void draw_sprite(u8 *buf, u32 bw, u32 bh, s32 x, s32 y, const Sprite *s)
{
	u32 sw, sh;
	s32 sx, sy, dstX, dstY, srcX0, srcY0, srcX1, srcY1;
	if (!s || !s->pix)
		return;
	sw = s->w;
	sh = s->h;
	if (x >= (s32)bw || y >= (s32)bh)
		return;
	if (x + (s32)sw <= 0 || y + (s32)sh <= 0)
		return;
	srcX0 = 0;
	srcY0 = 0;
	srcX1 = (s32)sw;
	srcY1 = (s32)sh;
	if (x < 0)
		srcX0 = -x;
	if (y < 0)
		srcY0 = -y;
	if (x + (s32)sw > (s32)bw)
		srcX1 = (s32)bw - x;
	if (y + (s32)sh > (s32)bh)
		srcY1 = (s32)bh - y;
	for (sy = srcY0; sy < srcY1; ++sy)
	{
		dstY = y + sy;
		dstX = x + srcX0;
		sx = srcX0;
		{
			u8 *d = buf + (u32)dstY * bw + (u32)dstX;
			const u8 *srow = s->pix + (u32)sy * sw + (u32)sx;
			while (sx < srcX1)
			{
				u8 c = *srow++;
				if (c)
					*d = c;
				++d;
				++sx;
			}
		}
	}
}
static void fill_rect(u8 *buf, u32 bw, u32 bh, s32 x, s32 y, s32 w, s32 h, u8 c)
{
	s32 iy, ix, x0 = x, y0 = y, x1 = x + w, y1 = y + h;
	if (x0 < 0)
		x0 = 0;
	if (y0 < 0)
		y0 = 0;
	if (x1 > (s32)bw)
		x1 = (s32)bw;
	if (y1 > (s32)bh)
		y1 = (s32)bh;
	for (iy = y0; iy < y1; ++iy)
	{
		u8 *d = buf + (u32)iy * bw + (u32)x0;
		for (ix = x0; ix < x1; ++ix)
			*d++ = c;
	}
}

static void font_init_fill(void)
{
	/* We fill the real table at runtime to keep code short & readable in BC45 */
}
static int font_index(char ch)
{
	if (ch >= '0' && ch <= '9')
		return ch - '0';
	if (ch >= 'A' && ch <= 'Z')
		return 10 + (ch - 'A');
	return -1;
}
/* 3x5 font: digits 0–9 then A–Z (same shapes you had) */
static const unsigned char FONT_SRC[36][5] = {
	/*0*/ {7, 5, 5, 5, 7}, /*1*/ {2, 6, 2, 2, 7}, /*2*/ {7, 1, 7, 4, 7}, /*3*/ {7, 1, 7, 1, 7}, /*4*/ {5, 5, 7, 1, 1},
	/*5*/ {7, 4, 7, 1, 7}, /*6*/ {7, 4, 7, 5, 7}, /*7*/ {7, 1, 1, 1, 1}, /*8*/ {7, 5, 7, 5, 7}, /*9*/ {7, 5, 7, 1, 7},
	/*A*/ {7, 5, 7, 5, 5}, /*B*/ {6, 5, 6, 5, 6}, /*C*/ {7, 4, 4, 4, 7}, /*D*/ {6, 5, 5, 5, 6}, /*E*/ {7, 4, 6, 4, 7},
	/*F*/ {7, 4, 6, 4, 4}, /*G*/ {7, 4, 5, 5, 7}, /*H*/ {5, 5, 7, 5, 5}, /*I*/ {7, 2, 2, 2, 7}, /*J*/ {1, 1, 1, 5, 7},
	/*K*/ {5, 5, 6, 5, 5}, /*L*/ {4, 4, 4, 4, 7}, /*M*/ {5, 7, 7, 5, 5}, /*N*/ {5, 7, 7, 7, 5}, /*O*/ {7, 5, 5, 5, 7},
	/*P*/ {7, 5, 7, 4, 4}, /*Q*/ {7, 5, 5, 7, 3}, /*R*/ {7, 5, 7, 6, 5}, /*S*/ {7, 4, 7, 1, 7}, /*T*/ {7, 2, 2, 2, 2},
	/*U*/ {5, 5, 5, 5, 7}, /*V*/ {5, 5, 5, 5, 2}, /*W*/ {5, 5, 7, 7, 5}, /*X*/ {5, 5, 2, 5, 5}, /*Y*/ {5, 5, 2, 2, 2},
	/*Z*/ {7, 1, 2, 4, 7}};

static void draw_char3x5(u8 *buf, u32 bw, u32 bh, int x, int y, char ch, u8 col, int scale)
{
	int idx = font_index(ch), row, cx, cy;
	unsigned char m;
	if (idx < 0)
		return;
	for (row = 0; row < 5; ++row)
	{
		m = FONT_SRC[idx][row];
		for (cx = 0; cx < 3; ++cx)
		{
			if ((m >> (2 - cx)) & 1)
			{
				for (cy = 0; cy < scale; ++cy)
					fill_rect(buf, bw, bh, x + cx * scale, y + row * scale, scale, scale, col);
			}
		}
	}
}
/* draws one glyph directly (no recursion, no side effects) */
static void draw_glyph3x5(u8 *buf, u32 bw, u32 bh, int x, int y, u8 col, int scale, const unsigned char glyph[5])
{
	int row, colb, xx, yy;
	for (row = 0; row < 5; ++row)
	{
		unsigned char m = glyph[row];
		for (colb = 0; colb < 3; ++colb)
		{
			if ((m >> (2 - colb)) & 1)
			{ /* leftmost bit = column 0 */
				for (yy = 0; yy < scale; ++yy)
				{
					u8 *d = buf + (u32)(y + row * scale + yy) * bw + (u32)(x + colb * scale);
					for (xx = 0; xx < scale; ++xx)
						*d++ = col;
				}
			}
		}
	}
}
/* safe text draw: iterates once over the string; advances x deterministically */
static void draw_text3x5(u8 *buf, u32 bw, u32 bh, int x, int y, const char *s, u8 col, int scale, int spacing)
{
	const char *p=s;
	const int advance = 3 * scale + spacing;

	for (; *p; ++p)
	{
		char ch = *p;
		int idx;
		if (ch == ' ')
		{
			x += advance;
			continue;
		}
		if (ch >= 'a' && ch <= 'z')
			ch = (char)(ch - 32);
		idx = font_index(ch);
		if (idx >= 0)
		{
			draw_glyph3x5(buf, bw, bh, x, y, col, scale, FONT_SRC[idx]);
		}
		x += advance;
	}
}

/* ============================================================
   Keyboard via BIOS buffer
   ============================================================ */
typedef struct
{
	int dummy;
} KbdState; /* no prefixes needed with BIOS words */

static void kbd_init(KbdState *k) { (void)k; }

static int kbd_poll_bios(int *dx, int *dy)
{
	unsigned short w;
	int esc = 0;
	*dx = 0;
	*dy = 0;
	while (kbd_bda_pop_word(&w))
	{
		unsigned char ascii = (unsigned char)(w & 0xFFu);
		unsigned char scan = (unsigned char)((w >> 8) & 0xFFu);
		if (ascii == 27u || scan == 1u)
			esc = 1;
		else
		{
			switch (scan)
			{
			case 0x48:
				*dy -= 1;
				break;
			case 0x50:
				*dy += 1;
				break;
			case 0x4B:
				*dx -= 1;
				break;
			case 0x4D:
				*dx += 1;
				break;
			default:
				break;
			}
		}
	}
	return esc;
}
static int any_key_pressed(void)
{
	unsigned short w;
	int got = 0;
	while (kbd_bda_pop_word(&w))
		got = 1;
	return got;
}

/* ============================================================
   Tiny game + states
   ============================================================ */
typedef struct
{
	s32 x, y;
	s32 w, h;
} AABB;
static int aabb_overlap(const AABB *a, const AABB *b)
{
	if (a->x >= b->x + b->w)
		return 0;
	if (b->x >= a->x + a->w)
		return 0;
	if (a->y >= b->y + b->h)
		return 0;
	if (b->y >= a->y + a->h)
		return 0;
	return 1;
}

enum
{
	ST_TITLE = 0,
	ST_PLAY = 1,
	ST_GAMEOVER = 2
};

int main(void)
{
	const u32 W = 320, H = 200, BYTES = 320UL * 200UL;
	u8 *buf0 = 0, *buf1 = 0, *cur = 0;
	float mul=1.0f;
	Sprite sprPlayer, sprLoot, sprGuard;
	int havePlayer = 0, haveLoot = 0, haveGuard = 0, havePal = 0;
	static Smf0Player music;
	KbdState kb;
	s32 px, py, gx, gy, dir, lx, ly, gspd=1;
	int score, hiscore = 0;
	int running = 1, gameover = 0;
	u32 frame = 0;
	int state = ST_TITLE;

	printf("Cat Burglar for Brackey's Jam!\nStarting...\n");
	set_mode13();
	havePal = load_palette_and_apply("game.pal");
	if (!havePal)
	{
		printf("Error loading palette.");
		goto CV_QUIT;
	}

	buf0 = (u8 *)malloc(BYTES);
	buf1 = (u8 *)malloc(BYTES);
	if (!buf0 || !buf1)
	{
		if (buf0)
			free(buf0);
		if (buf1)
			free(buf1);
		set_textmode();
		printf("Error allocating buffers.");
		goto CV_QUIT;
	}
	memset(buf0, 0, BYTES);
	memset(buf1, 0, BYTES);
	cur = buf0;

	memset(&sprPlayer, 0, sizeof(sprPlayer));
	memset(&sprLoot, 0, sizeof(sprLoot));
	memset(&sprGuard, 0, sizeof(sprGuard));
	havePlayer = sprite_load(&sprPlayer, "player.raw", 32, 32);
	haveLoot = sprite_load(&sprLoot, "loot.raw", 8, 8);
	haveGuard = sprite_load(&sprGuard, "guard.raw", 32, 32);
	if (!havePlayer || !haveLoot || !haveGuard)
	{
		set_textmode();
		printf("Error loading sprites.");
		goto CV_QUIT;
	}

	/* Init Keyboard */
	kbd_init(&kb);

	/* Init MPU for MIDI controller */
//	mpu_init_uart(0x330);
	//if(smf0_load("BGM.mid", &music)) smf0_start(&music);

	while (running)
	{
		int dx = 0, dy = 0;

		/* RENDER base clear */
		memset(cur, 0, BYTES);

		/* Border */
		fill_rect(cur, W, H, 0, 0, W, 10, 31);
		fill_rect(cur, W, H, 0, H - 10, W, 10, 31);
		fill_rect(cur, W, H, 0, 0, 10, H, 31);
		fill_rect(cur, W, H, W - 10, 0, 10, H, 31);

		if (state == ST_TITLE)
		{
				/* reset into a function-like block */
		{
			px = 140;
			py = 150;
			gx = 40;
			gy = 80;
			dir = 1;
			gspd = 1;
			lx = 260;
			ly = 140;
			score = 0;
			gameover = 0;
		}

			/* Title visuals */
			fill_rect(cur, W, H, 40, 40, 240, 60, 41);
			draw_text3x5(cur, W, H, 50, 50, "CAT BURGLAR", 24, 5, 1);
			draw_text3x5(cur, W, H, 58, 80, "GAME BY BURAK YAZAR", 28, 3, 1);
			draw_text3x5(cur, W, H, 30, 110, "USE ARROW KEYS TO CONTROL, ESC TO QUIT", 254, 2, 1);
			draw_text3x5(cur, W, H, 48, 125, "PRESS ANY KEY", 245, 1, 1);
			if (hiscore > 0)
			{
				draw_text3x5(cur, W, H, 90, 140, "HISCORE", 254, 2, 1);
				{
					char num[16];
					int n = hiscore, pos = 0, i;
					char tmp[12];
					int tlen = 0;
					if (n == 0)
					{
						tmp[tlen++] = '0';
					}
					while (n > 0)
					{
						tmp[tlen++] = (char)('0' + (n % 10));
						n /= 10;
					}
					for (i = tlen - 1; i >= 0; --i)
						num[pos++] = tmp[i];
					num[pos] = 0;
					draw_text3x5(cur, W, H, 160, 140, num, 254, 2, 1);
				}
			}
			if (any_key_pressed())
			{
				px = 140;
				py = 150;
				gx = 40;
				gy = 80;
				dir = 1;
                gspd = 1;
				lx = 260;
				ly = 140;
				score = 0;
				gameover = 0;
				state = ST_PLAY;
			}
		}
		else if (state == ST_PLAY)
		{
			/* INPUT */
			if (kbd_poll_bios(&dx, &dy))
			{
				running = 0;
			} /* ESC quits */
			px += dx * 2;
			py += dy * 2;
			if (px < 0)
				px = 0;
			if (py < 0)
				py = 0;
			if (px > (s32)(W - 16))
				px = (s32)(W - 16);
			if (py > (s32)(H - 16))
				py = (s32)(H - 16);

			/* UPDATE guard patrol */
			gx += (dir * gspd);
			gy += (dir * gspd);
			if (gx < 16)
			{
				gx = 16;
				dir = 1;
			}
			if (gy < 16)
			{
				gy = 16;
			}
			if (gx > (s32)(W - 32))
			{
				gx = (s32)(W - 32);
				dir = -1;
			}
			if(gy>(s32)(H-32))
			{
				gy =(s32)(H-32);
			}

			/* draw loot/guard/player */
			if (haveLoot)
				draw_sprite(cur, W, H, lx, ly, &sprLoot);
			else
				fill_rect(cur, W, H, lx, ly, 16, 16, 14);
			if (haveGuard)
				draw_sprite(cur, W, H, gx, gy, &sprGuard);
			else
				fill_rect(cur, W, H, gx, gy, 16, 32, 9);
			if (havePlayer)
				draw_sprite(cur, W, H, px, py, &sprPlayer);
			else
				fill_rect(cur, W, H, px, py, 16, 16, 28);

			/* HUD */
				/* Border */
			fill_rect(cur, W, H, 0, 0, W, 10, 31);
			fill_rect(cur, W, H, 0, H - 10, W, 10, 31);
			fill_rect(cur, W, H, 0, 0, 10, H, 31);
			fill_rect(cur, W, H, W - 10, 0, 10, H, 31);

			fill_rect(cur, W, H, 10, 10, 300, 10, 90);
			{
				int bar = score;
				if (bar > 298)
					bar = 0;
				fill_rect(cur, W, H, 11, 11, bar, 8, 152);
			}
			draw_text3x5(cur, W, H, 14, 22, "SCORE", 254, 2, 1);
			{
				char num[16];
				int n = score, pos = 0, i;
				char tmp[12];
				int tlen = 0;
				if (n == 0)
				{
					tmp[tlen++] = '0';
				}
				while (n > 0)
				{
					tmp[tlen++] = (char)('0' + (n % 10));
					n /= 10;
				}
				for (i = tlen - 1; i >= 0; --i)
					num[pos++] = tmp[i];
				num[pos] = 0;
				draw_text3x5(cur, W, H, 60, 22, num, 254, 2, 1);
			}
				draw_text3x5(cur, W, H, 100, 22, "MULTIPLIER", 254, 2, 1);
			{
				char num[16];
				int n, pos = 0, i;
				char tmp[12];
				int tlen = 0;

				n = (int)mul;
				if (n == 0)
				{
					tmp[tlen++] = '0';
				}
				while (n > 0)
				{
					tmp[tlen++] = (char)('0' + (n % 10));
					n /= 10;
				}
				for (i = tlen - 1; i >= 0; --i)
					num[pos++] = tmp[i];
				num[pos] = 0;
				draw_text3x5(cur, W, H, 180, 22, num, 254, 2, 1);
			}

			/* COLLISIONS */
			{
				AABB aP, aG, aL;

				aP.x = px;
				aP.y = py;
				aP.w = 16;
				aP.h = 32;
				aG.x = gx;
				aG.y = gy;
				aG.w = 16;
				aG.h = 32;
				aL.x = lx;
				aL.y = ly;
				aL.w = 16;
				aL.h = 16;
				if (havePlayer)
				{
					aP.w = (s32)sprPlayer.w;
					aP.h = (s32)sprPlayer.h;
				}
				if (haveGuard)
				{
					aG.w = (s32)sprGuard.w;
					aG.h = (s32)sprGuard.h;
				}
				if (haveLoot)
				{
					aL.w = (s32)sprLoot.w;
					aL.h = (s32)sprLoot.h;
				}
				if (aabb_overlap(&aP, &aL))
				{
					lx = 30 + (frame * 7) % 260;
					ly = 40 + (frame * 5) % 130;
					mul =  pow((float)abs(gx-px),2.0f) + pow((float)abs(gy-py),2.0f);
					mul = sqrt(mul)/20.0f;
					score = (int)((float)score + mul+1.0f);
					gspd++;
				}
				if (aabb_overlap(&aP, &aG))
				{
					gameover = 1;
					if (score > hiscore)
						hiscore = score;
					state = ST_GAMEOVER;
				}
			}
		}
		else
		{ /* ST_GAMEOVER */
			fill_rect(cur, W, H, 60, 70, 200, 50, 20);
			draw_text3x5(cur, W, H, 90, 84, "GAME OVER", 254, 3, 1);
			draw_text3x5(cur, W, H, 80, 130, "PRESS ANY KEY", 254, 2, 1);
			if (any_key_pressed())
			{
				state = ST_TITLE;
			}
		}

		/* VSYNC -> BLIT -> SWAP */
		vsync_wait();
		vga_blit(cur, BYTES);
		cur = (cur == buf0) ? buf1 : buf0;

		++frame;
	}

CV_QUIT:
	sprite_free(&sprPlayer);
	sprite_free(&sprLoot);
	sprite_free(&sprGuard);
	free(buf0);
	free(buf1);
	set_textmode();
	printf("Thank you for playing Cat Burglar, have a good one. :)");
	return 0;
}
