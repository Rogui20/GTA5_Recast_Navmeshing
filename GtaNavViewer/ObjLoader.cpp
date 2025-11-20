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
            std::vector<TempVertex> faceVerts;
            std::string token;
            while (ss >> token)
            {
                faceVerts.push_back(ParseFaceElement(token));
            }

            if (faceVerts.size() >= 3)
            {
                // Triangulate a possible ngon as a fan around the first vertex
                for (size_t i = 1; i + 1 < faceVerts.size(); ++i)
                {
                    const TempVertex& v0 = faceVerts[0];
                    const TempVertex& v1 = faceVerts[i];
                    const TempVertex& v2 = faceVerts[i + 1];

                    // OBJ indices are 1-based; validate before storing
                    if (v0.v <= 0 || v1.v <= 0 || v2.v <= 0) continue;

                    mesh->indices.push_back(static_cast<unsigned int>(v0.v - 1));
                    mesh->indices.push_back(static_cast<unsigned int>(v1.v - 1));
                    mesh->indices.push_back(static_cast<unsigned int>(v2.v - 1));
                }
            }
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
