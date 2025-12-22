#pragma once
#include <string>
#include "Mesh.h"

class ObjLoader
{
public:
    static Mesh* LoadMesh(const std::string& path, bool centerMesh = true, bool tryLoadBin = true);
    static Mesh* LoadMeshFromBin(const std::string& path, bool centerMesh = true);
    static Mesh* LoadObj(const std::string& path, bool centerMesh = true, bool tryLoadBin = false);
};