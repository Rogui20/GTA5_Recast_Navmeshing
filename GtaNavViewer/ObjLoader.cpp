#include "ObjLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>

struct TempVertex {
    int v = 0;
    int vt = 0;
    int vn = 0;
};

static TempVertex ParseFaceElement(const std::string& token)
{
    TempVertex tv;

    // Formatos possíveis:
    // v
    // v/vt
    // v//vn
    // v/vt/vn

    sscanf(token.c_str(), "%d/%d/%d", &tv.v, &tv.vt, &tv.vn);

    // Caso "v//vn", sscanf deixará vt=0; detectamos quando tem "//"
    if (token.find("//") != std::string::npos)
    {
        sscanf(token.c_str(), "%d//%d", &tv.v, &tv.vn);
        tv.vt = 0;
    }

    return tv;
}

Mesh* ObjLoader::LoadObj(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cout << "OBJ: failed to open " << path << "\n";
        return nullptr;
    }

    Mesh* mesh = new Mesh();

    std::string line;
    std::vector<glm::vec3> tmp;

    glm::vec3 minB(FLT_MAX);
    glm::vec3 maxB(-FLT_MAX);

    while (std::getline(file, line))
    {
        std::stringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v")
        {
            float x, y, z;
            ss >> x >> y >> z;
            glm::vec3 v(x, y, z);

            tmp.push_back(v);

            minB = glm::min(minB, v);
            maxB = glm::max(maxB, v);
        }
        else if (type == "f")
        {
            unsigned int a, b, c;
            ss >> a >> b >> c;
            mesh->indices.push_back(a - 1);
            mesh->indices.push_back(b - 1);
            mesh->indices.push_back(c - 1);
        }
    }

    // ============================
    // 🔵 RECENTRALIZAR A MESH
    // ============================
    glm::vec3 center = (minB + maxB) * 0.5f;

    for (auto& v : tmp)
        v -= center;

    // recalcular bounds após recentralizar
    glm::vec3 newMin(FLT_MAX);
    glm::vec3 newMax(-FLT_MAX);
    for (auto& v : tmp)
    {
        newMin = glm::min(newMin, v);
        newMax = glm::max(newMax, v);
    }

    mesh->vertices = tmp;
    mesh->minBounds = newMin;
    mesh->maxBounds = newMax;

    // enviar GPU
    mesh->UploadToGPU();

    std::cout
        << "OBJ loaded: "
        << mesh->vertices.size() << " verts, "
        << mesh->indices.size() / 3 << " tris\n"
        << "Bounds (recentered):\n"
        << "   Min = " << newMin.x << ", " << newMin.y << ", " << newMin.z << "\n"
        << "   Max = " << newMax.x << ", " << newMax.y << ", " << newMax.z << "\n"
        << "   Center was = " << center.x << ", " << center.y << ", " << center.z << "\n";

    return mesh;
}
