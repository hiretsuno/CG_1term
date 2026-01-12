#include "parserOBJ.h"

Model parseOBJ(const std::string& filepath) {
    std::ifstream file(filepath);
    Model model;

    if (!file.is_open()) {
        std::cout << "File not found: " << filepath << std::endl;
        return model;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            Vertex v;
            iss >> v.x >> v.y >> v.z;
            model.vertices.push_back(v);
        }
        else if (prefix == "vt") {
            TextureCoord tc;
            iss >> tc.u >> tc.v;
            model.texCoords.push_back(tc);
        }
        else if (prefix == "vn") {
            Normal n;
            iss >> n.x >> n.y >> n.z;
            model.normals.push_back(n);
        }
        else if (prefix == "f") {
            Face f;
            std::string faceData;
            while (iss >> faceData) {
                std::replace(faceData.begin(), faceData.end(), '/', ' ');
                std::istringstream faceStream(faceData);

                int vIdx, tIdx = -1, nIdx = -1;
                faceStream >> vIdx;
                if (!faceStream.eof()) faceStream >> tIdx;
                if (!faceStream.eof()) faceStream >> nIdx;

                f.vertexId.push_back(vIdx - 1);
                if (tIdx != -1) f.textureId.push_back(tIdx - 1);
                if (nIdx != -1) f.normalId.push_back(nIdx - 1);
            }
            model.faces.push_back(f);
        }
    }

    file.close();
    return model;
}

void printInfo(Model model) {
    std::cout << "Loaded OBJ data:\n";
    std::cout << "Vertices: " << model.vertices.size() << std::endl;
    std::cout << "Texture coordinates: " << model.texCoords.size() << std::endl;
    std::cout << "Normals: " << model.normals.size() << std::endl;
    std::cout << "Faces: " << model.faces.size() << std::endl;
}
