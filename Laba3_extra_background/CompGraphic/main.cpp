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

// Загрузим библиотеку STB для загрузки JPEG
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const TGAColor white = TGAColor(255, 255, 255, 255);
const TGAColor red = TGAColor(255, 0, 0, 255);
const TGAColor green = TGAColor(0, 255, 0, 255);

Model* model = NULL;
const int width = 800;
const int height = 800;

// Структура для хранения фонового изображения
struct BackgroundImage {
    unsigned char* data = nullptr;
    int bg_width = 0;
    int bg_height = 0;
    int channels = 0;

    ~BackgroundImage() {
        if (data) {
            stbi_image_free(data);
        }
    }

    bool load(const char* filename) {
        data = stbi_load(filename, &bg_width, &bg_height, &channels, 3);
        return data != nullptr;
    }

    TGAColor get_pixel(int x, int y) const {
        if (!data || x < 0 || x >= bg_width || y < 0 || y >= bg_height) {
            return TGAColor(0, 0, 0, 255);
        }

        int index = (y * bg_width + x) * 3;
        return TGAColor(data[index], data[index + 1], data[index + 2], 255);
    }

    TGAColor get_pixel_scaled(int x, int y, int target_width, int target_height) const {
        if (!data || bg_width == 0 || bg_height == 0) {
            return TGAColor(0, 0, 0, 255);
        }

        // Масштабируем координаты
        float u = (float)x / target_width;
        float v = (float)y / target_height;

        int bg_x = (int)(u * bg_width);
        int bg_y = (int)(v * bg_height);

        // Ограничиваем координаты
        bg_x = std::max(0, std::min(bg_width - 1, bg_x));
        bg_y = std::max(0, std::min(bg_height - 1, bg_y));

        return get_pixel(bg_x, bg_y);
    }
};

BackgroundImage background;

void triangle(Vec3i t0, Vec3i t1, Vec3i t2, Vec2i uv0, Vec2i uv1, Vec2i uv2,
    TGAImage& image, float intensity, float* zbuffer, Model* model,
    bool* pixel_covered = nullptr) {

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
            if (zbuffer[idx] < z) {
                zbuffer[idx] = z;

                TGAColor color = model->diffuse(uv);
                color.r = (unsigned char)(color.r * intensity);
                color.g = (unsigned char)(color.g * intensity);
                color.b = (unsigned char)(color.b * intensity);

                image.set(x, y, color);

                // Отмечаем пиксель как покрытый моделью
                if (pixel_covered) {
                    pixel_covered[idx] = true;
                }
            }
        }
    }
}

// Функция для рендеринга фона
void render_background(TGAImage& image, const BackgroundImage& bg) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            TGAColor bg_color = bg.get_pixel_scaled(x, y, width, height);
            image.set(x, y, bg_color);
        }
    }
}

// Функция для смешивания цвета модели с фоном
TGAColor blend_with_background(const TGAColor& model_color, const TGAColor& bg_color) {
    // Простое альфа-смешивание (можно улучшить)
    float alpha = 1.0f; // Полная непрозрачность модели

    unsigned char r = static_cast<unsigned char>(
        bg_color.r * (1.0f - alpha) + model_color.r * alpha
        );
    unsigned char g = static_cast<unsigned char>(
        bg_color.g * (1.0f - alpha) + model_color.g * alpha
        );
    unsigned char b = static_cast<unsigned char>(
        bg_color.b * (1.0f - alpha) + model_color.b * alpha
        );

    return TGAColor(r, g, b, 255);
}

int main(int argc, char** argv) {
    std::cout << "=== 3D Renderer with Camera, Phong Lighting and Background ===" << std::endl;

    // Загружаем фоновое изображение
    std::cout << "Loading background image... ";
    if (!background.load("dolphines.jpeg")) {
        std::cout << "ERROR: Failed to load background image 'dolphines.jpeg'!" << std::endl;
        std::cout << "Trying alternative names..." << std::endl;

        // Пробуем альтернативные имена
        const char* alt_names[] = { "dolphines.jpg", "dolphins.jpeg", "dolphins.jpg", "background.jpeg", "background.jpg" };
        bool loaded = false;

        for (const char* name : alt_names) {
            if (background.load(name)) {
                std::cout << "Loaded alternative: " << name << std::endl;
                loaded = true;
                break;
            }
        }

        if (!loaded) {
            std::cout << "ERROR: Could not load any background image!" << std::endl;
            std::cout << "Rendering without background..." << std::endl;
        }
    }
    else {
        std::cout << "Success! Size: " << background.bg_width << "x" << background.bg_height << std::endl;
    }

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

    float material_specular = 0.5f;
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

        // Массив для отслеживания покрытых пикселей
        bool* pixel_covered = new bool[width * height];

        for (int i = 0; i < width * height; i++) {
            zbuffer[i] = -std::numeric_limits<float>::max();
            pixel_covered[i] = false;
        }

        // Шаг 1: Рендерим фон
        std::cout << "Rendering background... ";
        render_background(image, background);
        std::cout << "Done" << std::endl;

        // Шаг 2: Рендерим модель поверх фона
        std::cout << "Rendering model... [";

        int rendered_faces = 0;
        int total_faces = model->nfaces();

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

                Vec3f center_point = (world_coords[0] + world_coords[1] + world_coords[2]);
                center_point = center_point * (1.0f / 3.0f);

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
                        image, intensity, zbuffer, model, pixel_covered);
                }
            }
        }

        std::cout << "]" << std::endl;
        std::cout << "Faces rendered: " << rendered_faces << "/" << total_faces << std::endl;

        // Опционально: можно добавить дополнительную обработку границ модели
        // для более плавного смешивания с фоном
        std::cout << "Finalizing image... ";

        // Простая постобработка: антиалиасинг по краям
        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                int idx = x + y * width;
                if (!pixel_covered[idx]) continue;

                // Проверяем соседей
                int covered_neighbors = 0;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nx = x + dx;
                        int ny = y + dy;
                        if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                            if (!pixel_covered[nx + ny * width]) {
                                covered_neighbors++;
                            }
                        }
                    }
                }

                // Если пиксель на границе модели, немного смешиваем с фоном
                if (covered_neighbors > 0) {
                    TGAColor model_color = image.get(x, y);
                    TGAColor bg_color = background.get_pixel_scaled(x, y, width, height);

                    float blend_factor = 0.3f; // Насколько сильно смешивать границы
                    unsigned char r = static_cast<unsigned char>(
                        model_color.r * (1.0f - blend_factor) + bg_color.r * blend_factor
                        );
                    unsigned char g = static_cast<unsigned char>(
                        model_color.g * (1.0f - blend_factor) + bg_color.g * blend_factor
                        );
                    unsigned char b = static_cast<unsigned char>(
                        model_color.b * (1.0f - blend_factor) + bg_color.b * blend_factor
                        );

                    image.set(x, y, TGAColor(r, g, b, 255));
                }
            }
        }

        std::cout << "Done" << std::endl;

        std::string filename = std::string("output_") + view_names[view] + "_with_background.tga";
        if (image.write_tga_file(filename.c_str())) {
            std::cout << "Saved: " << filename << std::endl;
        }
        else {
            std::cout << "ERROR saving: " << filename << std::endl;
        }

        delete[] zbuffer;
        delete[] pixel_covered;
    }

    delete model;
    std::cout << "\n=== All 4 views rendered with Phong Lighting and Background! ===" << std::endl;

    return 0;
}