#include <cmath>
#include <limits>
#include <cstdlib>
#include "../Include/our_gl.h"

Matrix ModelView;
Matrix Viewport;
Matrix Projection;

IShader::~IShader() {}

void viewport(int x, int y, int w, int h) {
    Viewport = Matrix::identity();
    Viewport[0][3] = x+w/2.f;
    Viewport[1][3] = y+h/2.f;
    Viewport[2][3] = 255.f/2.f;
    Viewport[0][0] = w/2.f;
    Viewport[1][1] = h/2.f;
    Viewport[2][2] = 255.f/2.f;
}

void projection(float coeff) {
    Projection = Matrix::identity();
    Projection[3][2] = coeff;
}

void lookat(Vec3f eye, Vec3f center, Vec3f up) {
    Vec3f z = (eye-center).normalize();
    Vec3f x = cross(up,z).normalize();
    Vec3f y = cross(z,x).normalize();
    ModelView = Matrix::identity();
    for (int i=0; i<3; i++) {
        ModelView[0][i] = x[i];
        ModelView[1][i] = y[i];
        ModelView[2][i] = z[i];
        ModelView[i][3] = -center[i];
    }
}

Vec3f barycentric(Vec2f A, Vec2f B, Vec2f C, Vec2f P) {
    Vec3f s[2];
    for (int i=2; i--; ) {
        s[i][0] = C[i]-A[i];
        s[i][1] = B[i]-A[i];
        s[i][2] = A[i]-P[i];
    }
    Vec3f u = cross(s[0], s[1]);
    if (std::abs(u[2])>1e-2)
        return Vec3f(1.f-(u.x+u.y)/u.z, u.y/u.z, u.x/u.z);
    return Vec3f(-1,1,1);
}
void alphaBlendPixel(TGAImage &image, int x, int y, const TGAColor &src, float alpha) {
    TGAColor dst = image.get(x,y);
    TGAColor out = dst;
    for(int i=0;i<3;i++) {
        out[i] = (unsigned char)(dst[i]*(1-alpha) + src[i]*alpha);
    }
    out[3]=255;
    image.set(x,y,out);
}
void triangle(Vec4f *pts, IShader &shader, TGAImage &image, TGAImage &zbuffer) {
    Vec2f bboxmin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    Vec2f bboxmax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());

    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 2; j++) {
            float v = pts[i][j] / pts[i][3];
            bboxmin[j] = std::min(bboxmin[j], v);
            bboxmax[j] = std::max(bboxmax[j], v);
        }

    Vec2i P;
    TGAColor color;

    for (P.x = bboxmin.x; P.x <= bboxmax.x; P.x++) {
        for (P.y = bboxmin.y; P.y <= bboxmax.y; P.y++) {
            Vec3f bc = barycentric(
                proj<2>(pts[0]/pts[0][3]),
                proj<2>(pts[1]/pts[1][3]),
                proj<2>(pts[2]/pts[2][3]),
                proj<2>(P)
            );

            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;

            float z = pts[0][2]*bc.x + pts[1][2]*bc.y + pts[2][2]*bc.z;
            float w = pts[0][3]*bc.x + pts[1][3]*bc.y + pts[2][3]*bc.z;
            int frag_depth = std::max(0, std::min(255, int(z/w + 0.5f)));

            bool discard = shader.fragment(bc, color);
            if (discard) continue;

            // прозрачный объект (куб)
            if (shader.alpha > 0.0f) {
                // НЕ проверяем z-buffer, чтобы видеть модель внутри
                alphaBlendPixel(image, P.x, P.y, color, shader.alpha);
            } else {
                // непрозрачный объект — обычная запись и проверка глубины
                if (zbuffer.get(P.x, P.y)[0] > frag_depth) continue;
                zbuffer.set(P.x, P.y, TGAColor(frag_depth));
                image.set(P.x, P.y, color);
            }
        }
    }
}



