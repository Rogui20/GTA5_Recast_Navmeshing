#include "ObjLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cfloat>
#include <filesystem>

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

namespace
{
    Mesh* FinalizeMesh(const std::vector<glm::vec3>& originalVertices,
                       const std::vector<unsigned int>& indices,
                       const glm::vec3& navMinB,
                       const glm::vec3& navMaxB,
                       bool centerMesh,
                       const char* sourceLabel)
    {
        if (originalVertices.empty())
        {
            std::cout << sourceLabel << ": mesh sem vertices" << "\n";
            return nullptr;
        }

        Mesh* mesh = new Mesh();

        std::vector<glm::vec3> renderVertices = originalVertices;

        glm::vec3 renderOffset(0.0f);
        glm::vec3 renderMin(FLT_MAX);
        glm::vec3 renderMax(-FLT_MAX);
        if (centerMesh)
        {
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
        mesh->indices = indices;
        mesh->renderOffset = renderOffset;
        mesh->renderMinBounds = renderMin;
        mesh->renderMaxBounds = renderMax;
        mesh->navmeshMinBounds = navMinB;
        mesh->navmeshMaxBounds = navMaxB;

        mesh->UploadToGPU();

        std::cout
            << sourceLabel << " loaded: "
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
}

static bool SaveMeshToBin(const std::string& path,
                          const std::vector<glm::vec3>& originalVertices,
                          const std::vector<unsigned int>& indices,
                          const glm::vec3& navMinB,
                          const glm::vec3& navMaxB)
{
    std::ofstream out(path, std::ios::binary);
    if (!out.is_open())
    {
        std::cout << "BIN: failed to open for write " << path << "\n";
        return false;
    }

    uint32_t version = 1;
    uint64_t vertexCount = originalVertices.size();
    uint64_t indexCount = indices.size();

    out.write(reinterpret_cast<const char*>(&version), sizeof(version));
    out.write(reinterpret_cast<const char*>(&vertexCount), sizeof(vertexCount));
    out.write(reinterpret_cast<const char*>(&indexCount), sizeof(indexCount));
    out.write(reinterpret_cast<const char*>(&navMinB), sizeof(navMinB));
    out.write(reinterpret_cast<const char*>(&navMaxB), sizeof(navMaxB));

    out.write(reinterpret_cast<const char*>(originalVertices.data()), sizeof(glm::vec3) * vertexCount);
    out.write(reinterpret_cast<const char*>(indices.data()), sizeof(unsigned int) * indexCount);

    return true;
}

Mesh* ObjLoader::LoadObj(const std::string& path, bool centerMesh, bool tryLoadBin)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        std::cout << "OBJ: failed to open " << path << "\n";
        return nullptr;
    }

    std::string line;
    std::vector<glm::vec3> originalVertices;
    std::vector<unsigned int> indices;

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

                    indices.push_back(static_cast<unsigned int>(v0.v - 1));
                    indices.push_back(static_cast<unsigned int>(v1.v - 1));
                    indices.push_back(static_cast<unsigned int>(v2.v - 1));
                }
            }
        }
    }

    Mesh* mesh = FinalizeMesh(originalVertices, indices, navMinB, navMaxB, centerMesh, "OBJ");

    if (mesh && tryLoadBin)
    {
        std::filesystem::path binPath(path);
        binPath.replace_extension(".bin");
        if (SaveMeshToBin(binPath.string(), originalVertices, indices, navMinB, navMaxB))
        {
            std::cout << "BIN salvo em " << binPath << "\n";
        }
    }

    return mesh;
}

Mesh* ObjLoader::LoadMeshFromBin(const std::string& path, bool centerMesh)
{
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open())
    {
        std::cout << "BIN: failed to open " << path << "\n";
        return nullptr;
    }

    uint32_t version = 0;
    uint64_t vertexCount = 0;
    uint64_t indexCount = 0;
    glm::vec3 navMinB(FLT_MAX);
    glm::vec3 navMaxB(-FLT_MAX);

    in.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 1)
    {
        std::cout << "BIN: unsupported version in " << path << "\n";
        return nullptr;
    }

    in.read(reinterpret_cast<char*>(&vertexCount), sizeof(vertexCount));
    in.read(reinterpret_cast<char*>(&indexCount), sizeof(indexCount));
    in.read(reinterpret_cast<char*>(&navMinB), sizeof(navMinB));
    in.read(reinterpret_cast<char*>(&navMaxB), sizeof(navMaxB));

    if (!in.good())
    {
        std::cout << "BIN: corrupted header in " << path << "\n";
        return nullptr;
    }

    std::vector<glm::vec3> originalVertices(vertexCount);
    std::vector<unsigned int> indices(indexCount);

    in.read(reinterpret_cast<char*>(originalVertices.data()), sizeof(glm::vec3) * vertexCount);
    in.read(reinterpret_cast<char*>(indices.data()), sizeof(unsigned int) * indexCount);

    if (!in.good())
    {
        std::cout << "BIN: corrupted data in " << path << "\n";
        return nullptr;
    }

    return FinalizeMesh(originalVertices, indices, navMinB, navMaxB, centerMesh, "BIN");
}

Mesh* ObjLoader::LoadMesh(const std::string& path, bool centerMesh, bool tryLoadBin)
{
    std::filesystem::path objPath(path);
    std::filesystem::path binPath = objPath;
    binPath.replace_extension(".bin");

    if (tryLoadBin && std::filesystem::exists(binPath))
    {
        if (auto* mesh = LoadMeshFromBin(binPath.string(), centerMesh))
        {
            return mesh;
        }

        std::cout << "BIN falhou, tentando OBJ...\n";
    }

    return LoadObj(path, centerMesh, tryLoadBin);
}
