#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <limits>

struct Vertex {
    float x, y, z;
};

struct TextureCoord {
    float u, v;
    TextureCoord(float u = 0, float v = 0) : u(u), v(v) {}
};

struct Normal {
    float x, y, z;
};

struct Face {
    std::vector<int> vertexId;
    std::vector<int> textureId;
    std::vector<int> normalId;
};

struct Model {
    std::vector<Vertex> vertices;
    std::vector<TextureCoord> texCoords;
    std::vector<Normal> normals;
    std::vector<Face> faces;
};

Model parseOBJ(const std::string& filepath);
void printInfo(Model model);

