#include "ObjLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cfloat>

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

Mesh* ObjLoader::LoadObj(const std::string& path, bool centerMesh)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cout << "OBJ: failed to open " << path << "\n";
        return nullptr;
    }

    Mesh* mesh = new Mesh();

    std::string line;
    std::vector<glm::vec3> originalVertices;

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

            originalVertices.push_back(v);

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

    std::vector<glm::vec3> renderVertices = originalVertices;

    glm::vec3 center(0.0f);
    if (centerMesh)
    {
        // ============================
        // 🔵 RECENTRALIZAR A MESH
        // ============================
        center = (minB + maxB) * 0.5f;

        for (auto& v : renderVertices)
            v -= center;

        // recalcular bounds após recentralizar
        glm::vec3 newMin(FLT_MAX);
        glm::vec3 newMax(-FLT_MAX);
        for (auto& v : renderVertices)
        {
            newMin = glm::min(newMin, v);
            newMax = glm::max(newMax, v);
        }

        minB = newMin;
        maxB = newMax;
    }

    mesh->renderVertices = renderVertices;
    mesh->navmeshVertices = originalVertices;
    mesh->minBounds = minB;
    mesh->maxBounds = maxB;

    // enviar GPU
    mesh->UploadToGPU();

    std::cout
        << "OBJ loaded: "
        << mesh->renderVertices.size() << " verts, "
        << mesh->indices.size() / 3 << " tris\n"
        << "Bounds" << (centerMesh ? " (recentered)" : "") << ":\n"
        << "   Min = " << minB.x << ", " << minB.y << ", " << minB.z << "\n"
        << "   Max = " << maxB.x << ", " << maxB.y << ", " << maxB.z << "\n";

    if (centerMesh)
    {
        std::cout << "   Center was = " << center.x << ", " << center.y << ", " << center.z << "\n";
    }

    return mesh;
}
