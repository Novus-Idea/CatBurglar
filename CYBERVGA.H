#ifndef CYBERVGA_H
#define CYBERVGA_H

#include <dos.h>
#include <alloc.h>

#define CV_WIDTH 320
#define CV_HEIGHT 200
#define CV_SCREENRES 64000UL
typedef unsigned char Byte;

void cv_set_vga_mode(void);
void cv_set_text_mode(void);
void cv_set_palette_entry(int idx, Byte r, Byte g, Byte b);
void cv_set_palette(Byte *pal[3], int num_entries);
void cv_make_default_palette(void);

#endif