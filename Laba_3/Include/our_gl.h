#ifndef __OUR_GL_H__
#define __OUR_GL_H__

#include "tgaimage.h"
#include "../Include/math.h"

extern Matrix ModelView;
extern Matrix Viewport;
extern Matrix Projection;

void viewport(int x, int y, int w, int h);
void projection(float coeff=0.f);
void lookat(Vec3f eye, Vec3f center, Vec3f up);

struct IShader{
    float alpha = 0.0f;
    virtual ~IShader();
    virtual Vec4f vertex(int iface, int nthvert) = 0;
    virtual bool fragment(Vec3f bar, TGAColor &color) = 0;
};

void triangle(Vec4f *pts, IShader &shader, TGAImage &image, TGAImage &zbuffer);

#endif //__OUR_GL_H__
