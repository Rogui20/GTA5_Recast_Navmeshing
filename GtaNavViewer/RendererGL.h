#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "RenderMode.h"
#include <vector>

class ViewerCamera;

struct DebugLine
{
    float x0, y0, z0;
    float x1, y1, z1;
};


class RendererGL
{
public:
    RendererGL();
    ~RendererGL();

    void Begin(ViewerCamera* cam, RenderMode mode);
    void End();

    void DebugCube(glm::vec3 pos, float scale);

    GLuint GetShader() const { return shader; }
    void DrawGrid();
    void DrawAxisGizmo();
    void DrawAxisGizmoScreen(const ViewerCamera* cam, int screenW, int screenH);

    void DrawTriangleHighlight(glm::vec3 a, glm::vec3 b, glm::vec3 c);
    void drawNavmeshLines(const std::vector<DebugLine>& lines);

    bool Load(const char* path);

    // NOVO: constrói navmesh direto da malha
    bool BuildFromMesh(const std::vector<glm::vec3>& verts,
                       const std::vector<unsigned int>& indices);

    void ExtractDebugMesh(std::vector<glm::vec3>& outVerts,
                          std::vector<glm::vec3>& outLines);

private:
    GLuint LoadShader(const char* vs, const char* fs);

    GLuint shader;
    GLuint vaoCube;
    GLuint vboCube;
    GLuint vaoAxis, vboAxis;
    GLuint vaoGrid, vboGrid;
    int gridVertexCount = 0;
    GLuint navVao = 0;
    GLuint navVbo = 0;
};
