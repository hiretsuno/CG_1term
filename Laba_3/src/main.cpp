#include <vector>
#include <iostream>

#include "../Include/tgaimage.h"
#include "../Include/mesh.h"
#include "../Include/math.h"
#include "../Include/our_gl.h"
#include "../Include/camera.h"

Model *model     = NULL;
const int width  = 800;
const int height = 800;

Vec3f light_dir(1,1,1);

Camera cam(
    /* eye    */ Vec3f(2, 2, 10),
    /* center */ Vec3f(0, 0, 0),
    /* up     */ Vec3f(0, 1, 0)
);

Vec3f cube_vertices_global[8] = {
    {-1, -1, -1}, {1, -1, -1},
    {1,  1, -1}, {-1, 1, -1},
    {-1, -1,  1}, {1, -1,  1},
    {1,  1,  1}, {-1, 1,  1}
};

int cube_faces[12][3] = {
    {0,1,2}, {0,2,3}, // задняя грань
    {4,5,6}, {4,6,7}, // передняя грань
    {0,1,5}, {0,5,4}, // нижняя грань
    {2,3,7}, {2,7,6}, // верхняя грань
    {1,2,6}, {1,6,5}, // правая грань
    {0,3,7}, {0,7,4}  // левая грань
};

struct Shader : public IShader {
    mat<3,3,float> varying_norm; // нормали по вершинам
    mat<3,3,float> varying_pos;  // позиции по вершинам
    mat<2,3,float> varying_uv;   // uv по вершинам


    virtual Vec4f vertex(int iface, int nthvert) {
        // UV
        varying_uv.set_col(nthvert, model->uv(iface, nthvert));
        // вершина из модели
        Vec4f gl_Vertex = embed<4>(model->vert(iface, nthvert));

        Vec4f pos_eye4 = ModelView * gl_Vertex;
        Vec3f pos_eye = proj<3>(pos_eye4);
        varying_pos.set_col(nthvert, pos_eye);

        Vec4f n4 = embed<4>(model->normal(iface, nthvert), 0.0f);
        Vec3f n_eye = proj<3>(ModelView * n4).normalize();
        varying_norm.set_col(nthvert, n_eye);

        return Viewport * Projection * ModelView * gl_Vertex;
    }

    // fragment: интерполируем нормаль/позицию/uv и считаем Phong
    virtual bool fragment(Vec3f bar, TGAColor &color) {
        // --- интерполяция ---
        Vec2f interp_uv = varying_uv * bar;
        Vec3f interp_pos = varying_pos * bar;
        Vec3f interp_norm = (varying_norm * bar).normalize();

        // нормализованный вектор света
        Vec3f light_dir_eye = proj<3>(ModelView * embed<4>(::light_dir, 0.0f)).normalize();

        // view vector
        Vec3f view_dir = (Vec3f(0,0,0) - interp_pos).normalize();

        const float ambient_strength = 0.1f;

        // diffuse
        float diff = std::max(0.f, interp_norm * light_dir_eye);

        // specular (Phong)
        // R = reflect(-L, N) = 2*(N·L)*N - L
        Vec3f R = interp_norm * (2.f * (interp_norm * light_dir_eye)) - light_dir_eye;
        float spec_angle = std::max(0.f, R * view_dir);

        float spec_map = model->specular(Vec2f(interp_uv[0], interp_uv[1]));

        if (spec_map < 1e-6f) spec_map = 16.f;
        float shininess = spec_map;
        float spec = std::pow(spec_angle, shininess);

        // получаем base diffuse color из текстуры
        Vec2f uv = Vec2f(interp_uv[0], interp_uv[1]);
        TGAColor tex = model->diffuse(uv);
        Vec3f tex_rgb = Vec3f((float)tex[2], (float)tex[1], (float)tex[0]);

        Vec3f ambient = tex_rgb * ambient_strength;
        Vec3f diffuse = tex_rgb * diff;
        Vec3f specular = Vec3f(255.f,255.f,255.f) * spec * 0.5f;

        Vec3f result = ambient + diffuse + specular;

        for (int i=0;i<3;i++) {
            if (result[i] > 255.f) result[i] = 255.f;
            if (result[i] < 0.f)   result[i] = 0.f;
        }
        color[0] = (unsigned char)result[2]; // B
        color[1] = (unsigned char)result[1]; // G
        color[2] = (unsigned char)result[0]; // R
        color[3] = 255; // A

        return false;
    }
};



struct CubeShader : public IShader {
    CubeShader(const TGAColor &c, float a) {
        base_color = c;
        alpha = a;
    }

    TGAColor base_color;

    virtual Vec4f vertex(int iface, int nthvert) {
        int idx = cube_faces[iface][nthvert];
        Vec4f v = embed<4>(cube_vertices_global[idx]);
        return Viewport * Projection * ModelView * v;
    }

    virtual bool fragment(Vec3f bar, TGAColor &color) {
        color = base_color;
        return false;
    }
};

int main(int argc, char** argv) {
    if (2==argc) {
        model = new Model(argv[1]);
    } else {
        model = new Model("../obj/african_head.obj");
    }

    cam.applyView();
    cam.applyProjection(width, height);
    viewport(width/8, height/8, width*3/4, height*3/4);
    light_dir.normalize();

    TGAImage image  (width, height, TGAImage::RGB);
    TGAImage zbuffer(width, height, TGAImage::GRAYSCALE);

    Shader shader;
    for (int i = 0; i < model->nfaces(); i++) {
        Vec4f screen_coords[3];
        for (int j = 0; j < 3; j++)
            screen_coords[j] = shader.vertex(i, j);
        triangle(screen_coords, shader, image, zbuffer);
    }

    CubeShader cubeshader(TGAColor(50,150,255,255), 0.3f); // alpha = 0.3
    for (int i = 0; i < 12; i++) {
        Vec4f screen_coords[3];
        for (int j = 0; j < 3; j++)
            screen_coords[j] = cubeshader.vertex(i, j);
        triangle(screen_coords, cubeshader, image, zbuffer);
    }


    image.  flip_vertically();
    zbuffer.flip_vertically();
    image.  write_tga_file("output.tga");
    zbuffer.write_tga_file("zbuffer.tga");

    delete model;
    return 0;
}