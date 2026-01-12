#include "tgaimage.h"
#include "shaders.h"

const TGAColor Background = TGAColor(45, 45, 45, 255);

const float PI = 3.14159265358979323846f;

void triangle(Vec3* screen_pts, const Vec3& world_v0, const Vec3& world_v1,
    const Vec3& world_v2, Vec2* tex_coords, Vec3& face_normal,
    const ShaderUniform& uniform, int width, int height,
    float* zbuffer, TGAImage& image) {

    Vec2 bboxmin(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    Vec2 bboxmax(-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max());

    for (int i = 0; i < 3; i++) {
        bboxmin.x = std::max(0.f, std::min(bboxmin.x, screen_pts[i].x));
        bboxmin.y = std::max(0.f, std::min(bboxmin.y, screen_pts[i].y));
        bboxmax.x = std::min(width - 1.f, std::max(bboxmax.x, screen_pts[i].x));
        bboxmax.y = std::min(height - 1.f, std::max(bboxmax.y, screen_pts[i].y));
    }

    for (int x = (int)bboxmin.x; x <= (int)bboxmax.x; x++) {
        for (int y = (int)bboxmin.y; y <= (int)bboxmax.y; y++) {
            Vec3 bc = barycentric(screen_pts, Vec2(x, y));

            if (bc.x < 0 || bc.y < 0 || bc.z < 0) continue;

            float z = screen_pts[0].z * bc.x + screen_pts[1].z * bc.y + screen_pts[2].z * bc.z;
            Vec3 world_pos = world_v0 * bc.x + world_v1 * bc.y + world_v2 * bc.z;
            Vec2 texcoord = tex_coords[0] * bc.x + tex_coords[1] * bc.y + tex_coords[2] * bc.z;

            int idx = x + y * width;
            if (z < zbuffer[idx]) {
                zbuffer[idx] = z;

                TGAColor color = phongLight(world_pos, face_normal, texcoord, uniform);
                image.set(x, y, color);
            }
        }
    }
}

class Camera {
private:
    Vec3 position;     
    Vec3 target;         
    Vec3 up;        

    float field_of_view;   
    float near_distance;  
    float far_distance; 

    Matrix4 view_matrix; 
    Matrix4 projection_matrix;

public:
    Camera(const Vec3& camera_position = Vec3(0, 0, 3),
        const Vec3& look_at_target = Vec3(0, 0, 0),
        float fov_degrees = 45.0f,
        float near_plane = 0.1f,
        float far_plane = 100.0f)
        : position(camera_position),
        target(look_at_target),
        up(0, 1, 0),
        field_of_view(fov_degrees),
        near_distance(near_plane),
        far_distance(far_plane) {

        recalculateMatrices();
    }

    void recalculateMatrices() {
        updateViewMatrix();
        updateProjectionMatrix();
    }

    Vec3 getPosition() {
        return position;
    }

    void updateViewMatrix() {
        Vec3 forward_direction = (target - position).normalize();

        Vec3 right_direction = cross(forward_direction, up).normalize();

        Vec3 corrected_up = cross(right_direction, forward_direction);

        view_matrix = Matrix4::lookAt(position, target, corrected_up);
    }

    void updateProjectionMatrix() {
        float fov_radians = field_of_view * PI / 180.0f;
        float aspect_ratio = 1.0f; 
        projection_matrix = Matrix4::perspective(fov_radians, aspect_ratio, near_distance, far_distance);
    }

    void move(const Vec3& movement_offset) {
        position = position + movement_offset;
        target = target + movement_offset;
        updateViewMatrix();
    }

    void setPosition(const Vec3& new_position) {
        Vec3 look_direction = target - position;
        position = new_position;
        target = position + look_direction;
        updateViewMatrix();
    }

    void lookAt(const Vec3& new_target) {
        target = new_target;
        updateViewMatrix();
    }

    void setFieldOfView(float new_fov_degrees) {
        field_of_view = new_fov_degrees;
        updateProjectionMatrix();
    }

    Matrix4 getViewMatrix() const { return view_matrix; }
    Matrix4 getProjectionMatrix() const { return projection_matrix; }
};

void drawModel(const Model& model, const ShaderUniform& uniform, TGAImage& image, float* zbuffer) {
    for (int i = 0; i < model.faces.size(); i++) {
        const Face& face = model.faces[i];
        if (face.vertexId.size() >= 3 && face.textureId.size() >= 3) {
            for (int j = 1; j < face.vertexId.size() - 1; j++) {
                int idx0 = 0, idx1 = j, idx2 = j + 1;

                const Vertex& v0 = model.vertices[face.vertexId[idx0]];
                const Vertex& v1 = model.vertices[face.vertexId[idx1]];
                const Vertex& v2 = model.vertices[face.vertexId[idx2]];
                const TextureCoord& tc0 = model.texCoords[face.textureId[idx0]];
                const TextureCoord& tc1 = model.texCoords[face.textureId[idx1]];
                const TextureCoord& tc2 = model.texCoords[face.textureId[idx2]];

                Vec3 normal;
                if (face.normalId.size() >= 3) {
                    const Normal& n0 = model.normals[face.normalId[idx0]];
                    const Normal& n1 = model.normals[face.normalId[idx1]];
                    const Normal& n2 = model.normals[face.normalId[idx2]];
                    normal = Vec3(n0.x, n0.y, n0.z);
                }
                else {
                    Vec3 world_v0(v0.x, v0.y, v0.z);
                    Vec3 world_v1(v1.x, v1.y, v1.z);
                    Vec3 world_v2(v2.x, v2.y, v2.z);
                    Vec3 edge1 = world_v1 - world_v0;
                    Vec3 edge2 = world_v2 - world_v0;
                    normal = cross(edge1, edge2).normalize();
                }

                Vec3 world_v0(v0.x, v0.y, v0.z);
                Vec3 world_v1(v1.x, v1.y, v1.z);
                Vec3 world_v2(v2.x, v2.y, v2.z);

                Vec3 screen_v0 = uniform.projection_matrix.transformPoint(
                    uniform.view_matrix.transformPoint(world_v0));
                Vec3 screen_v1 = uniform.projection_matrix.transformPoint(
                    uniform.view_matrix.transformPoint(world_v1));
                Vec3 screen_v2 = uniform.projection_matrix.transformPoint(
                    uniform.view_matrix.transformPoint(world_v2));

                Vec3 screen_coords[3] = {
                    Vec3((screen_v0.x + 1.0f) * uniform.width / 2.0f,
                         (screen_v0.y + 1.0f) * uniform.height / 2.0f,
                         screen_v0.z),
                    Vec3((screen_v1.x + 1.0f) * uniform.width / 2.0f,
                         (screen_v1.y + 1.0f) * uniform.height / 2.0f,
                         screen_v1.z),
                    Vec3((screen_v2.x + 1.0f) * uniform.width / 2.0f,
                         (screen_v2.y + 1.0f) * uniform.height / 2.0f,
                         screen_v2.z)
                };
                Vec2 texture_coords[3] = {
                    Vec2(tc0.u, tc0.v),
                    Vec2(tc1.u, tc1.v),
                    Vec2(tc2.u, tc2.v)
                };

                triangle(screen_coords, world_v0, world_v1, world_v2,
                    texture_coords, normal, uniform, uniform.width, uniform.height, zbuffer, image);
            }
        }
    }
}

int main() {
	int width = 500;
	int height = 500;
	TGAImage image(width, height, TGAImage::RGB);

	for (int x = 0; x < width; x++) {
		for (int y = 0; y < height; y++) {
			image.set(x, y, Background);
		}
	}

    Model obj;
    obj = parseOBJ("obj/african_head.obj");
    Texture texture("obj/african_head_SSS.jpg");

    std::vector<float> zBuffer(width * height, std::numeric_limits<float>::max());

    Camera camera(Vec3(0, 0, 5), //камера xyz
        Vec3(0, 0, 0),  // моделька xyz
        40.0f,          // FOV
        0.1f,           // мин
        50.0f);         // макс

    Material material;
    material.ambient = Vec3(0.4f, 0.4f, 0.4f);
    material.diffuse = Vec3(0.8f, 0.8f, 0.8f);
    material.specular = Vec3(0.5f, 0.5f, 0.5f);
    material.shininess = 32.0f;
    material.texture = &texture;

    Light light;
    light.position = Vec3(4, 10, 30);
    light.ambient = Vec3(0.2f, 0.2f, 0.2f);
    light.diffuse = Vec3(0.8f, 0.8f, 0.8f);
    light.specular = Vec3(0.5f, 0.5f, 0.5f);

    ShaderUniform uniform;
    uniform.view_matrix = camera.getViewMatrix();
    uniform.projection_matrix = camera.getProjectionMatrix();
    uniform.camera_pos = camera.getPosition();
    uniform.width = width;
    uniform.height = height;
    uniform.material = material;
    uniform.light = light;

    drawModel(obj, uniform, image, zBuffer.data());

    std::cout << "Done";

	image.flip_vertically();
	image.write_tga_file("output.tga"); 

}