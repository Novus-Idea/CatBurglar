#ifndef CV_SPRIT_H
#define CV_SPRIT_H

#include <stdio.h>
typedef struct
{
	unsigned char 	manufacturer;
	unsigned char 	version;
	unsigned char 	encoding;
	unsigned char 	bitsPerPixel;
	unsigned int    xmin, ymin;
	unsigned int    xmax, ymax;
	unsigned int    hDPI, vDPI;
	unsigned char   colormap[48];
	unsigned char	reserved;
	unsigned char   nPlanes;
	unsigned int    bytesPerLine;
	unsigned int	paletteInfo;
	unsigned int	hScreenSize, vScreenSize;
	unsigned char   filler[54];

}PCXHeader;

typedef struct
{
	int width;
	int height;
	unsigned char* pixels;
}Sprite;

static void cv_read_le16(const unsigned char* p, unsigned int* out);
static void cv_parse_pcx(const char raw[128], PCXHeader* h);
static int cv_pcx_read_palette(FILE* f, unsigned char outVga63[256*3]);
static int cv_pcx_rle_get(FILE* f, int* runCount,int* runValue);

int cv_load_pcx_sprite(const char* path,
						Sprite* outSprite,
						unsigned char vgaPalette63[256*3]);

#endif