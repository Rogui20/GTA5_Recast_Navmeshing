#pragma once
#include <string>
#include "Mesh.h"

class ObjLoader
{
public:
    static Mesh* LoadObj(const std::string& path, bool centerMesh = true);
};
