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

    glm::vec3 navMinB(FLT_MAX);
    glm::vec3 navMaxB(-FLT_MAX);

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

            navMinB = glm::min(navMinB, v);
            navMaxB = glm::max(navMaxB, v);
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

    glm::vec3 renderOffset(0.0f);
    glm::vec3 renderMin(FLT_MAX);
    glm::vec3 renderMax(-FLT_MAX);
    if (centerMesh)
    {
        // ============================
        // 🔵 RECENTRALIZAR A MESH
        // ============================
        renderOffset = (navMinB + navMaxB) * 0.5f;

        for (auto& v : renderVertices)
            v -= renderOffset;
    }

    for (auto& v : renderVertices)
    {
        renderMin = glm::min(renderMin, v);
        renderMax = glm::max(renderMax, v);
    }

    mesh->renderVertices = renderVertices;
    mesh->navmeshVertices = originalVertices;
    mesh->renderOffset = renderOffset;
    mesh->renderMinBounds = renderMin;
    mesh->renderMaxBounds = renderMax;
    mesh->navmeshMinBounds = navMinB;
    mesh->navmeshMaxBounds = navMaxB;

    // enviar GPU
    mesh->UploadToGPU();

    std::cout
        << "OBJ loaded: "
        << mesh->renderVertices.size() << " verts, "
        << mesh->indices.size() / 3 << " tris\n"
        << "Navmesh bounds:\n"
        << "   Min = " << navMinB.x << ", " << navMinB.y << ", " << navMinB.z << "\n"
        << "   Max = " << navMaxB.x << ", " << navMaxB.y << ", " << navMaxB.z << "\n"
        << "Render bounds" << (centerMesh ? " (recentered)" : "") << ":\n"
        << "   Min = " << renderMin.x << ", " << renderMin.y << ", " << renderMin.z << "\n"
        << "   Max = " << renderMax.x << ", " << renderMax.y << ", " << renderMax.z << "\n";

    if (centerMesh)
    {
        std::cout << "   Render offset = " << renderOffset.x << ", " << renderOffset.y << ", " << renderOffset.z << "\n";
    }

    return mesh;
}
