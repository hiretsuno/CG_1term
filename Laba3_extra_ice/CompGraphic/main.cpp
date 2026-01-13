#include <vector>
#include <cmath>
#include <cstring> 
#include <limits>  
#include <iostream>
#include <algorithm>
#include "tgaimage.h"
#include "model.h"
#include "geometry.h"
#include "camera.h"

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red = TGAColor(255, 0, 0, 255);
const TGAColor green = TGAColor(0, 255, 0, 255);
const TGAColor ice_color = TGAColor(180, 240, 255, 100);
const TGAColor sphere_outline = TGAColor(150, 200, 255, 200);

Model* model = NULL;
const int width = 800;
const int height = 800;

TGAColor blend_colors(const TGAColor& bg, const TGAColor& fg) {
    float alpha = fg.a / 255.0f;

    unsigned char r = static_cast<unsigned char>(bg.r * (1.0f - alpha) + fg.r * alpha);
    unsigned char g = static_cast<unsigned char>(bg.g * (1.0f - alpha) + fg.g * alpha);
    unsigned char b = static_cast<unsigned char>(bg.b * (1.0f - alpha) + fg.b * alpha);

    return TGAColor(r, g, b, 255);
}

//Line Sweeping
void triangle(Vec3i t0, Vec3i t1, Vec3i t2, Vec2i uv0, Vec2i uv1, Vec2i uv2,
    TGAImage& image, float intensity, float* zbuffer,
    bool is_transparent = false, TGAColor transparent_color = TGAColor(255, 255, 255, 255),
    Model* model = nullptr) {

    if (t0.y < 0 && t1.y < 0 && t2.y < 0) return;
    if (t0.y >= height && t1.y >= height && t2.y >= height) return;
    if (t0.x < 0 && t1.x < 0 && t2.x < 0) return;
    if (t0.x >= width && t1.x >= width && t2.x >= width) return;

    if (t0.y == t1.y && t0.y == t2.y) return;

    if (t0.y > t1.y) { std::swap(t0, t1); std::swap(uv0, uv1); }
    if (t0.y > t2.y) { std::swap(t0, t2); std::swap(uv0, uv2); }
    if (t1.y > t2.y) { std::swap(t1, t2); std::swap(uv1, uv2); }

    int total_height = t2.y - t0.y;

    for (int y = t0.y; y <= t2.y; y++) {
        if (y < 0 || y >= height) continue;

        bool second_half = y > t1.y || t1.y == t0.y;
        int segment_height = second_half ? t2.y - t1.y : t1.y - t0.y;
        if (segment_height == 0) segment_height = 1;

        float alpha = (float)(y - t0.y) / total_height;
        float beta = second_half ? (float)(y - t1.y) / segment_height : (float)(y - t0.y) / segment_height;

        int xA = t0.x + (t2.x - t0.x) * alpha;
        int xB = second_half ? t1.x + (t2.x - t1.x) * beta : t0.x + (t1.x - t0.x) * beta;

        float zA = t0.z + (t2.z - t0.z) * alpha;
        float zB = second_half ? t1.z + (t2.z - t1.z) * beta : t0.z + (t1.z - t0.z) * beta;

        Vec2i uvA = uv0 + (uv2 - uv0) * alpha;
        Vec2i uvB = second_half ? uv1 + (uv2 - uv1) * beta : uv0 + (uv1 - uv0) * beta;

        if (xA > xB) {
            std::swap(xA, xB);
            std::swap(zA, zB);
            std::swap(uvA, uvB);
        }

        for (int x = xA; x <= xB; x++) {
            if (x < 0 || x >= width) continue;

            float phi = (xA == xB) ? 1.0f : (float)(x - xA) / (float)(xB - xA);

            float z = zA + (zB - zA) * phi;
            Vec2i uv = uvA + (uvB - uvA) * phi;

            int idx = x + y * width;

            if (is_transparent) {
                if (zbuffer[idx] < z) {
                    zbuffer[idx] = z;

                    TGAColor color_with_intensity = transparent_color;
                    color_with_intensity.r = (unsigned char)(transparent_color.r * intensity);
                    color_with_intensity.g = (unsigned char)(transparent_color.g * intensity);
                    color_with_intensity.b = (unsigned char)(transparent_color.b * intensity);

                    TGAColor current_color = image.get(x, y);
                    TGAColor blended = blend_colors(current_color, color_with_intensity);
                    image.set(x, y, blended);
                }
            }
            else if (model) {
                if (zbuffer[idx] < z) {
                    zbuffer[idx] = z;

                    TGAColor color = model->diffuse(uv);
                    color.r = (unsigned char)(color.r * intensity);
                    color.g = (unsigned char)(color.g * intensity);
                    color.b = (unsigned char)(color.b * intensity);

                    image.set(x, y, color);
                }
            }
            else {
                if (zbuffer[idx] < z) {
                    zbuffer[idx] = z;

                    TGAColor color = transparent_color;
                    color.r = (unsigned char)(transparent_color.r * intensity);
                    color.g = (unsigned char)(transparent_color.g * intensity);
                    color.b = (unsigned char)(transparent_color.b * intensity);

                    image.set(x, y, color);
                }
            }
        }
    }
}

// Генерация вершин сферы (икосаэдра для простоты, можно использовать более детализированную сферу)
std::vector<Vec3f> generate_sphere_vertices(int subdivisions = 2, float radius = 1.4f) {
    std::vector<Vec3f> vertices;

    // Начинаем с икосаэдра
    const float t = (1.0f + sqrt(5.0f)) / 2.0f;

    vertices = {
        Vec3f(-1,  t,  0), Vec3f(1,  t,  0), Vec3f(-1, -t,  0), Vec3f(1, -t,  0),
        Vec3f(0, -1,  t), Vec3f(0,  1,  t), Vec3f(0, -1, -t), Vec3f(0,  1, -t),
        Vec3f(t,  0, -1), Vec3f(t,  0,  1), Vec3f(-t,  0, -1), Vec3f(-t,  0,  1)
    };

    // Нормализуем и масштабируем
    for (auto& v : vertices) {
        v.normalize();
        v = v * radius;
    }

    return vertices;
}

// Структура для грани сферы
struct SphereFace {
    std::vector<int> indices;
    bool is_front;
    Vec3f normal;
    Vec3f center;
};

// Расчет нормали грани
Vec3f calculate_face_normal(const std::vector<Vec3f>& vertices, const std::vector<int>& indices) {
    if (indices.size() < 3) return Vec3f(0, 0, 1);

    Vec3f v0 = vertices[indices[0]];
    Vec3f v1 = vertices[indices[1]];
    Vec3f v2 = vertices[indices[2]];

    Vec3f normal = (v2 - v0) ^ (v1 - v0);
    normal.normalize();
    return normal;
}

// Получение граней сферы с определением видимости
std::vector<SphereFace> get_sphere_faces(const Camera& camera, const std::vector<Vec3f>& sphere_vertices) {
    std::vector<SphereFace> faces;

    // Базовые грани икосаэдра (20 граней)
    std::vector<std::vector<int>> base_faces = {
        {0, 11, 5}, {0, 5, 1}, {0, 1, 7}, {0, 7, 10}, {0, 10, 11},
        {1, 5, 9}, {5, 11, 4}, {11, 10, 2}, {10, 7, 6}, {7, 1, 8},
        {3, 9, 4}, {3, 4, 2}, {3, 2, 6}, {3, 6, 8}, {3, 8, 9},
        {4, 9, 5}, {2, 4, 11}, {6, 2, 10}, {8, 6, 7}, {9, 8, 1}
    };

    Vec3f camera_pos = camera.getEye();

    for (const auto& face_indices : base_faces) {
        SphereFace sphere_face;
        sphere_face.indices = face_indices;

        // Расчет нормали грани
        sphere_face.normal = calculate_face_normal(sphere_vertices, face_indices);

        // Расчет центра грани
        sphere_face.center = Vec3f(0, 0, 0);
        for (int idx : face_indices) {
            sphere_face.center = sphere_face.center + sphere_vertices[idx];
        }
        sphere_face.center = sphere_face.center * (1.0f / face_indices.size());

        // Определение видимости (лицевые грани)
        Vec3f to_camera = camera_pos - sphere_face.center;
        to_camera.normalize();

        float dot_product = sphere_face.normal * to_camera;
        sphere_face.is_front = (dot_product > 0.0f); // Все грани с положительным скалярным произведением видимы

        faces.push_back(sphere_face);
    }

    return faces;
}

// Рендеринг задних граней сферы
void render_sphere_with_layers(Camera& camera, TGAImage& image, float* zbuffer, Vec3f light_dir) {
    std::vector<Vec3f> sphere_vertices = generate_sphere_vertices();
    std::vector<SphereFace> faces = get_sphere_faces(camera, sphere_vertices);

    for (const auto& face : faces) {
        if (!face.is_front) { // Рендерим только невидимые (задние) грани
            Vec3i screen_coords[3];
            Vec3f world_coords[3];

            for (int j = 0; j < 3; j++) {
                int idx = face.indices[j];
                Vec3f v = sphere_vertices[idx];
                world_coords[j] = v;

                Matrix viewProj = camera.getViewProjectionMatrix();
                Vec3f transformed = viewProj * v;

                screen_coords[j] = Vec3i(
                    (int)((transformed.x + 1.0f) * width / 2.0f + 0.5f),
                    (int)((transformed.y + 1.0f) * height / 2.0f + 0.5f),
                    (int)(transformed.z * 1000.0f)
                );
            }

            // Освещение для грани сферы
            float intensity = 0.6f + 0.2f * std::abs(face.normal * light_dir);
            intensity = std::min(0.8f, std::max(0.5f, intensity));

            triangle(screen_coords[0], screen_coords[1], screen_coords[2],
                Vec2i(0, 0), Vec2i(0, 0), Vec2i(0, 0),
                image, intensity, zbuffer, false, ice_color, nullptr);
        }
    }
}

// Рендеринг передних (прозрачных) граней сферы
void render_front_sphere_faces(Camera& camera, TGAImage& image, float* zbuffer, Vec3f light_dir) {
    std::vector<Vec3f> sphere_vertices = generate_sphere_vertices();
    std::vector<SphereFace> faces = get_sphere_faces(camera, sphere_vertices);

    for (const auto& face : faces) {
        if (face.is_front) { // Рендерим только видимые (передние) грани
            Vec3i screen_coords[3];
            Vec3f world_coords[3];

            for (int j = 0; j < 3; j++) {
                int idx = face.indices[j];
                Vec3f v = sphere_vertices[idx];
                world_coords[j] = v;

                Matrix viewProj = camera.getViewProjectionMatrix();
                Vec3f transformed = viewProj * v;

                screen_coords[j] = Vec3i(
                    (int)((transformed.x + 1.0f) * width / 2.0f + 0.5f),
                    (int)((transformed.y + 1.0f) * height / 2.0f + 0.5f),
                    (int)(transformed.z * 1000.0f)
                );
            }

            // Освещение для передней грани сферы
            float intensity = 0.5f + 0.3f * std::abs(face.normal * light_dir);
            intensity = std::min(0.7f, std::max(0.4f, intensity));

            // Рендерим как прозрачную грань
            triangle(screen_coords[0], screen_coords[1], screen_coords[2],
                Vec2i(0, 0), Vec2i(0, 0), Vec2i(0, 0),
                image, intensity, zbuffer, true, ice_color, nullptr);
        }
    }
}

// Дополнительная функция для рендеринга контура сферы
void render_sphere_outline(Camera& camera, TGAImage& image, float* zbuffer) {
    std::vector<Vec3f> sphere_vertices = generate_sphere_vertices();

    // Рисуем рёбра сферы (контур)
    std::vector<std::pair<int, int>> edges = {
        {0, 11}, {11, 5}, {5, 0}, {0, 5}, {5, 1}, {1, 0},
        {0, 1}, {1, 7}, {7, 0}, {0, 7}, {7, 10}, {10, 0},
        {0, 10}, {10, 11}, {11, 0},
        {1, 5}, {5, 9}, {9, 1},
        {5, 11}, {11, 4}, {4, 5},
        {11, 10}, {10, 2}, {2, 11},
        {10, 7}, {7, 6}, {6, 10},
        {7, 1}, {1, 8}, {8, 7},
        {3, 9}, {9, 4}, {4, 3},
        {3, 4}, {4, 2}, {2, 3},
        {3, 2}, {2, 6}, {6, 3},
        {3, 6}, {6, 8}, {8, 3},
        {3, 8}, {8, 9}, {9, 3}
    };

    for (const auto& edge : edges) {
        Vec3f v1 = sphere_vertices[edge.first];
        Vec3f v2 = sphere_vertices[edge.second];

        Matrix viewProj = camera.getViewProjectionMatrix();
        Vec3f p1 = viewProj * v1;
        Vec3f p2 = viewProj * v2;

        int x1 = (int)((p1.x + 1.0f) * width / 2.0f);
        int y1 = (int)((p1.y + 1.0f) * height / 2.0f);
        int x2 = (int)((p2.x + 1.0f) * width / 2.0f);
        int y2 = (int)((p2.y + 1.0f) * height / 2.0f);

        // Простая линия Брезенхема для контура
        bool steep = false;
        if (std::abs(x1 - x2) < std::abs(y1 - y2)) {
            std::swap(x1, y1);
            std::swap(x2, y2);
            steep = true;
        }
        if (x1 > x2) {
            std::swap(x1, x2);
            std::swap(y1, y2);
        }

        int dx = x2 - x1;
        int dy = y2 - y1;
        int derror2 = std::abs(dy) * 2;
        int error2 = 0;
        int y = y1;

        for (int x = x1; x <= x2; x++) {
            if (steep) {
                if (x >= 0 && x < height && y >= 0 && y < width) {
                    image.set(y, x, sphere_outline);
                }
            }
            else {
                if (x >= 0 && x < width && y >= 0 && y < height) {
                    image.set(x, y, sphere_outline);
                }
            }
            error2 += derror2;
            if (error2 > dx) {
                y += (y2 > y1 ? 1 : -1);
                error2 -= dx * 2;
            }
        }
    }
}

int main(int argc, char** argv) {
    std::cout << "=== 3D Renderer with Object INSIDE Transparent Sphere ===" << std::endl;

    if (argc == 2) {
        model = new Model(argv[1]);
    }
    else {
        model = new Model("object.obj");
    }

    if (model->nverts() == 0) {
        std::cout << "ERROR: Failed to load model!" << std::endl;
        return 1;
    }

    std::cout << "Model loaded: " << model->nverts() << " vertices, "
        << model->nfaces() << " faces" << std::endl;

    Vec3f light_dir(0.2f, 0.4f, -1.0f);
    light_dir.normalize();

    float material_specular = 0.4f;
    float shininess = 32.0f;

    const char* view_names[] = { "front", "side", "top", "three_quarter" };

    struct ViewConfig {
        Vec3f eye;
        Vec3f target;
        Vec3f up;
        float fov;
    };

    ViewConfig view_configs[] = {
        {Vec3f(0, 0, 5), Vec3f(0, 0, 0), Vec3f(0, 1, 0), 45.0f},
        {Vec3f(5, 0, 0), Vec3f(0, 0, 0), Vec3f(0, 1, 0), 45.0f},
        {Vec3f(0, 5, 0), Vec3f(0, 0, 0), Vec3f(0, 0, -1), 45.0f},
        {Vec3f(3, 2, 4), Vec3f(0, 0, 0), Vec3f(0, 1, 0), 50.0f}
    };

    for (int view = 0; view < 4; view++) {
        std::cout << "\n=== Rendering " << view_names[view] << " view... ===" << std::endl;

        ViewConfig config = view_configs[view];
        Camera camera(config.eye, config.target, config.up,
            config.fov, (float)width / height, 0.1f, 100.0f);

        TGAImage image(width, height, TGAImage::RGB);
        float* zbuffer = new float[width * height];
        for (int i = 0; i < width * height; i++) {
            zbuffer[i] = -std::numeric_limits<float>::max();
        }

        std::cout << "1. Rendering back faces of sphere... ";
        render_sphere_with_layers(camera, image, zbuffer, light_dir);
        std::cout << "Done" << std::endl;

        std::cout << "2. Rendering object inside sphere... ";

        int rendered_faces = 0;
        int total_faces = model->nfaces();

        // Рендерим объект (голову)
        for (int i = 0; i < total_faces; i++) {
            if (i % (total_faces / 50) == 0) {
                std::cout << ".";
                std::cout.flush();
            }

            std::vector<int> face = model->face(i);
            if (face.size() < 3) continue;

            Vec3i screen_coords[3];
            Vec3f world_coords[3];
            Vec2i uv_coords[3];

            for (int j = 0; j < 3; j++) {
                int vert_idx = face[j];
                if (vert_idx < 0 || vert_idx >= model->nverts()) {
                    screen_coords[j] = Vec3i(0, 0, 0);
                    continue;
                }

                Vec3f v = model->vert(vert_idx);
                world_coords[j] = v;

                Matrix viewProj = camera.getViewProjectionMatrix();
                Vec3f transformed = viewProj * v;

                screen_coords[j] = Vec3i(
                    (int)((transformed.x + 1.0f) * width / 2.0f + 0.5f),
                    (int)((transformed.y + 1.0f) * height / 2.0f + 0.5f),
                    (int)(transformed.z * 1000.0f)
                );

                uv_coords[j] = model->uv(i, j);
            }

            bool outside = true;
            for (int j = 0; j < 3; j++) {
                if (screen_coords[j].x >= -100 && screen_coords[j].x < width + 100 &&
                    screen_coords[j].y >= -100 && screen_coords[j].y < height + 100) {
                    outside = false;
                    break;
                }
            }

            if (outside) continue;

            Vec3f n = (world_coords[2] - world_coords[0]) ^ (world_coords[1] - world_coords[0]);
            float norm = n.norm();
            if (norm > 0) {
                n.normalize();

                Vec3f view_dir = (camera.getEye() - world_coords[0]);
                view_dir.normalize();

                Vec3f light_dir_neg = light_dir * (-1.0f);
                Vec3f reflect_dir = light_dir_neg.reflect(n);
                reflect_dir.normalize();

                float ambient = 0.25f;
                float diffuse = std::abs(n * light_dir);
                float specular = material_specular * std::pow(std::max(0.0f, view_dir * reflect_dir), shininess);

                float intensity = ambient + diffuse + specular;
                intensity = std::min(1.0f, std::max(0.0f, intensity));

                if (intensity > 0.0f) {
                    rendered_faces++;
                    triangle(screen_coords[0], screen_coords[1], screen_coords[2],
                        uv_coords[0], uv_coords[1], uv_coords[2],
                        image, intensity, zbuffer, false, white, model);
                }
            }
        }

        std::cout << " Done" << std::endl;

        std::cout << "3. Rendering front (transparent) faces of sphere... ";
        render_front_sphere_faces(camera, image, zbuffer, light_dir);
        std::cout << "Done" << std::endl;

        std::cout << "4. Rendering sphere outline... ";
        render_sphere_outline(camera, image, zbuffer);
        std::cout << "Done" << std::endl;

        std::cout << "Faces rendered: " << rendered_faces << "/" << total_faces << std::endl;

        std::string filename = std::string("output_") + view_names[view] + "_layered_sphere.tga";
        if (image.write_tga_file(filename.c_str())) {
            std::cout << "Saved: " << filename << std::endl;
        }
        else {
            std::cout << "ERROR saving: " << filename << std::endl;
        }

        delete[] zbuffer;
    }

    delete model;
    std::cout << "\n=== All 4 views rendered with Object INSIDE Layered Sphere! ===" << std::endl;

    return 0;
}