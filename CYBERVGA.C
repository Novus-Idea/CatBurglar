#include "cybervga.h"

// Set screen to VGA mode 13h
void cv_set_vga_mode(void)
{
	union REGS regs;
	regs.h.ah = 0x00;
	regs.h.al = 0x13;
	int86(0x10, &regs, &regs);
}
// Set screen back to text mode
void cv_set_text_mode(void)
{
	union REGS regs;
	regs.h.ah = 0x00;
	regs.h.al = 0x03;
	int86(0x10, &regs, &regs);
}

// Sets a single palette entry (r,g,b: 0-63)
void cv_set_palette_entry(int idx, Byte r, Byte g, Byte b)
{
	outp(0x3C8, idx);
	outp(0x3C9, r);
	outp(0x3C9, g);
	outp(0x3C9, b);
}
void cv_set_palette(Byte* pal[3], int num_entries)
{
	int i;
	outp(0x3C8, 0);		// Start at color 0
	for(i=0; i<num_entries; ++i)
	{
		outp(0x3C9, pal[i][0]);		// Red property
		outp(0x3C9, pal[i][1]);		// Green property
		outp(0x3C9, pal[i][2]);		// Blue property
	}
}
void cv_make_default_palette(void)
{
	int i;
	outp(0x3C8, 0);		// Start at color 0
	for(i=0; i<256; ++i)
	{
		outp(0x3C9, 0);		// Red property
		outp(0x3C9, i/4);	// Green property
		outp(0x3C9, i/2);	// Blue property
	}
}