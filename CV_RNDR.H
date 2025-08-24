#ifndef CV_RNDR_H
#define CV_RNDR_H

#include "cybervga.h"
#include "cv_math.h"

#define CV_TEX_WIDTH 64
#define CV_TEX_HEIGHT 64

typedef struct
{
	Byte pixels[CV_TEX_WIDTH*CV_TEX_HEIGHT];
	int width, height;
}CV_Texture;

void cv_put_pixel(int x, int y, Byte color, Byte far *buffer);
void cv_put_pixel_z(int x, int y, float z, Byte color, Byte far *buffer, float far* depthBuffer);
void cv_draw_line(int x0, int y0, int x1, int y1,
				  Byte color, Byte far *buffer);
void cv_draw_triangle_wire(const CV_Vec2 *v0, const CV_Vec2 *v1,
						   const CV_Vec2 *v2, Byte color, Byte far *buffer);
void cv_draw_triangles_wire(const CV_Vec2 *vertices, const int *indices,
							int numTris, Byte color, Byte far *buffer);
void cv_draw_triangle_fill(const CV_Vec2 *v0, const CV_Vec2 *v1,
						   const CV_Vec2 *v2, Byte color, Byte far *buffer);
void cv_draw_char(int x, int y, char c, Byte color, Byte far* byffer);
void cv_draw_text(int x, int y, char* str, Byte color, Byte far* byffer);

// Texturing
int cv_load_texture(CV_Texture* tex, const char* file);

void cv_draw_texture(const CV_Texture* tex, int x, int y, Byte far* buffer);

void cv_draw_triangle_tex(const CV_Vec2* v0,const CV_Vec2* v1,const CV_Vec2* v2,
						  const CV_Vec2* t0,const CV_Vec2* t1,const CV_Vec2* t2,
						  const CV_Texture* tex, Byte far* buffer);
#endif