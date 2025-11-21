#include "RendererGL.h"
#include "ViewerCamera.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <cstdio>
#include <vector>

static const char* VERT_SRC = R"(
#version 330 core
layout (location = 0) in vec3 aPos;

uniform mat4 uProj;
uniform mat4 uView;
uniform mat4 uModel;

out vec3 vWorldPos;

void main()
{
    vec4 world = uModel * vec4(aPos,1);
    vWorldPos = world.xyz;
    gl_Position = uProj * uView * world;
}

)";


static const char* FRAG_SRC = R"(
#version 330 core

in vec3 vWorldPos;

uniform int uRenderMode;
uniform vec3 uSolidColor;
uniform float uSolidAlpha;

out vec4 FragColor;

void main()
{
    // ---- CALCULO DE NORMAL POR FACE (preciso e simples) ----
    vec3 N = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));

    if (uRenderMode == 0) {
        // Blender-like SOLID shading (lambert + subtle fill light)
        vec3 baseColor = vec3(0.72, 0.76, 0.82);

        // Key light similar to Blender's default solid lighting
        vec3 L1 = normalize(vec3(0.45, 0.80, 0.35));
        // Fill light to avoid overly dark backsides
        vec3 L2 = normalize(vec3(-0.30, -0.60, -0.25));

        float diff1 = max(dot(N, L1), 0.0);
        float diff2 = max(dot(N, L2), 0.0) * 0.35;

        float ambient = 0.25;
        float shading = ambient + diff1 * 0.9 + diff2;
        shading = clamp(shading, 0.0, 1.0);

        vec3 col = baseColor * shading;
        FragColor = vec4(col,1);
    }
    else if (uRenderMode == 1) {
        FragColor = vec4(0,0,0,1);
    }
    else if (uRenderMode == 2) {
        FragColor = vec4(N*0.5+0.5,1);
    }
    else if (uRenderMode == 3) {
        float depth = gl_FragCoord.z;
        FragColor = vec4(depth,depth,depth,1);
    }
    else if (uRenderMode == 4) {
        // ---- LIT estilo Blender SOLID ----
        vec3 L = normalize(vec3(0.4,1.0,0.2));
        float diff = max(dot(N,L), 0.0);
        vec3 base = vec3(0.8, 0.8, 0.9);
        vec3 col  = base*(0.15 + diff*0.85);
        FragColor = vec4(col,1);
    }
    else if (uRenderMode == 99) {
        FragColor = vec4(uSolidColor,uSolidAlpha);
    }
}
)";


RendererGL::RendererGL()
{
    shader = LoadShader(VERT_SRC, FRAG_SRC);

    glGenVertexArrays(1, &vaoCube);
    glGenBuffers(1, &vboCube);

    float cubeVerts[] = {
        -1,-1,-1,  1,-1,-1,  1, 1,-1,
        -1,-1,-1,  1, 1,-1, -1, 1,-1,
        -1,-1, 1,  1,-1, 1,  1, 1, 1,
        -1,-1, 1,  1, 1, 1, -1, 1, 1,
        -1,-1,-1, -1, 1,-1, -1, 1, 1,
        -1,-1,-1, -1, 1, 1, -1,-1, 1,
         1,-1,-1,  1, 1,-1,  1, 1, 1,
         1,-1,-1,  1, 1, 1,  1,-1, 1,
        -1,-1,-1,  1,-1,-1,  1,-1, 1,
        -1,-1,-1,  1,-1, 1, -1,-1, 1,
        -1, 1,-1,  1, 1,-1,  1, 1, 1,
        -1, 1,-1,  1, 1, 1, -1, 1, 1
    };

    glBindVertexArray(vaoCube);
    glBindBuffer(GL_ARRAY_BUFFER, vboCube);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVerts), cubeVerts, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    float axisVerts[] = {
        // X
        0,0,0,  1,0,0,
        // Y
        0,0,0,  0,1,0,
        // Z
        0,0,0,  0,0,1,
    };

    glGenVertexArrays(1, &vaoAxis);
    glGenBuffers(1, &vboAxis);

    glBindVertexArray(vaoAxis);
    glBindBuffer(GL_ARRAY_BUFFER, vboAxis);
    glBufferData(GL_ARRAY_BUFFER, sizeof(axisVerts), axisVerts, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    std::vector<float> grid;
    const int N = 50;

    for (int i=-N; i<=N; i++)
    {
        // Linhas paralelas ao X
        grid.push_back(i); grid.push_back(0); grid.push_back(-N);
        grid.push_back(i); grid.push_back(0); grid.push_back(N);

        // Linhas paralelas ao Z
        grid.push_back(-N); grid.push_back(0); grid.push_back(i);
        grid.push_back(N);  grid.push_back(0); grid.push_back(i);
    }

    gridVertexCount = grid.size() / 3;

    glGenVertexArrays(1, &vaoGrid);
    glGenBuffers(1, &vboGrid);

    glBindVertexArray(vaoGrid);
    glBindBuffer(GL_ARRAY_BUFFER, vboGrid);
    glBufferData(GL_ARRAY_BUFFER, grid.size()*sizeof(float), grid.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

    // --- Navmesh Debug Lines ---

    glGenVertexArrays(1, &navVao);
    glGenBuffers(1, &navVbo);

    glBindVertexArray(navVao);
    glBindBuffer(GL_ARRAY_BUFFER, navVbo);

    // buffer inicialmente vazio
    glBufferData(GL_ARRAY_BUFFER, 1, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float)*3, (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);

}

RendererGL::~RendererGL()
{
    glDeleteProgram(shader);
    glDeleteVertexArrays(1, &vaoCube);
    glDeleteBuffers(1, &vboCube);
    glDeleteVertexArrays(1, &vaoAxis);
    glDeleteBuffers(1, &vboAxis);
    glDeleteVertexArrays(1, &vaoGrid);
    glDeleteBuffers(1, &vboGrid);
    glDeleteVertexArrays(1, &navVao);
    glDeleteBuffers(1, &navVbo);
}

GLuint RendererGL::LoadShader(const char* vs, const char* fs)
{
    GLuint v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(v, 1, &vs, NULL);
    glCompileShader(v);

    GLint ok;
    glGetShaderiv(v, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetShaderInfoLog(v, 512, NULL, log);
        printf("[Shader] Vertex error: %s\n", log);
    }

    GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(f, 1, &fs, NULL);
    glCompileShader(f);

    glGetShaderiv(f, GL_COMPILE_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetShaderInfoLog(f, 512, NULL, log);
        printf("[Shader] Fragment error: %s\n", log);
    }

    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);

    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok)
    {
        char log[512];
        glGetProgramInfoLog(p, 512, NULL, log);
        printf("[Shader] Link error: %s\n", log);
    }

    glDeleteShader(v);
    glDeleteShader(f);

    return p;
}

void RendererGL::Begin(ViewerCamera* cam, RenderMode mode)
{
    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "uRenderMode"), (int)mode);
    glUniform1f(glGetUniformLocation(shader, "uSolidAlpha"), 1.0f);


    // Matriz de projeção e view sempre iguais
    glm::mat4 proj = cam->GetProjMatrix();
    glm::mat4 view = cam->GetViewMatrix();

    glUniformMatrix4fv(glGetUniformLocation(shader, "uProj"), 1, GL_FALSE, &proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uView"), 1, GL_FALSE, &view[0][0]);

    // Configuração por modo
    switch (mode)
    {
    case RenderMode::Solid:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_FILL);
        break;

    case RenderMode::Wireframe:
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDisable(GL_POLYGON_OFFSET_FILL);
        break;

    case RenderMode::Normals:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDisable(GL_POLYGON_OFFSET_FILL);
        break;

    case RenderMode::Depth:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f); // evita z-fighting
        break;

    case RenderMode::None:
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glUniform1i(glGetUniformLocation(shader, "uRenderMode"), 99);
        glUniform3f(glGetUniformLocation(shader, "uSolidColor"), 1,1,1);
        break;


    }
}


void RendererGL::End()
{
    glBindVertexArray(0);
}

void RendererGL::DebugCube(glm::vec3 pos, float scale)
{
    glUseProgram(shader);

    glm::mat4 model(1.0f);
    model = glm::translate(model, pos);
    model = glm::scale(model, glm::vec3(scale));

    glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"), 1, GL_FALSE, &model[0][0]);

    glBindVertexArray(vaoCube);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

void RendererGL::DrawAxisGizmo()
{
    glUseProgram(shader);

    glm::mat4 model(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"), 1, GL_FALSE, &model[0][0]);

    glBindVertexArray(vaoAxis);

    glLineWidth(3.0f);

    glUniform3f(glGetUniformLocation(shader, "uSolidColor"), 1,0,0); // vermelho
    glDrawArrays(GL_LINES, 0, 2);

    glUniform3f(glGetUniformLocation(shader, "uSolidColor"), 0,1,0); // verde
    glDrawArrays(GL_LINES, 2, 2);

    glUniform3f(glGetUniformLocation(shader, "uSolidColor"), 0,0,1); // azul
    glDrawArrays(GL_LINES, 4, 2);
    
}

void RendererGL::DrawGrid()
{
    glUseProgram(shader);

    glm::mat4 model(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"), 1, GL_FALSE, &model[0][0]);

    glUniform3f(glGetUniformLocation(shader, "uSolidColor"), 0.3f, 0.3f, 0.3f);

    glBindVertexArray(vaoGrid);
    glLineWidth(1.0f);
    glDrawArrays(GL_LINES, 0, gridVertexCount);
}

void RendererGL::DrawTriangleHighlight(glm::vec3 a, glm::vec3 b, glm::vec3 c)
{
    glUseProgram(shader);

    glm::mat4 model(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"),
                       1, GL_FALSE, &model[0][0]);

    glUniform3f(glGetUniformLocation(shader, "uSolidColor"), 1,1,0);


    float tri[] = {
        a.x,a.y,a.z,
        b.x,b.y,b.z,
        c.x,c.y,c.z
    };

    GLuint vao, vbo;
    glGenVertexArrays(1,&vao);
    glGenBuffers(1,&vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER,vbo);
    glBufferData(GL_ARRAY_BUFFER,sizeof(tri),tri,GL_STATIC_DRAW);

    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),(void*)0);
    glEnableVertexAttribArray(0);

    glLineWidth(4.0f);
    glDrawArrays(GL_LINE_LOOP,0,3);

    glDeleteBuffers(1,&vbo);
    glDeleteVertexArrays(1,&vao);
}

void RendererGL::DrawAxisGizmoScreen(const ViewerCamera* cam, int screenW, int screenH)
{
    glUseProgram(shader);

    // --- 1. Configurar pequena viewport 100x100 no canto superior direito ---
    int size = 100;

    glViewport(screenW - size - 10, 10, size, size);

    // --- 2. Projeção ortográfica (para overlay) ---
    glm::mat4 proj = glm::ortho(-1.f, 1.f, -1.f, 1.f, 0.1f, 100.f);

    // --- 3. View matrix baseada somente na rotação da câmera ― sem posição ---
    glm::mat4 view = glm::lookAt(
        glm::vec3(0,0,3),  // câmera fixa
        glm::vec3(0,0,0),
        glm::vec3(0,1,0)
    );

    // Rotação = mesma da câmera do jogo
    glm::mat4 rot =
        glm::yawPitchRoll(glm::radians(cam->yaw), glm::radians(cam->pitch), 0.0f);

    view = view * rot;

    glUniformMatrix4fv(glGetUniformLocation(shader,"uProj"),1,GL_FALSE,&proj[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(shader,"uView"),1,GL_FALSE,&view[0][0]);

    glm::mat4 model(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader,"uModel"),1,GL_FALSE,&model[0][0]);

    // --- 4. Renderizar eixos ---
    glBindVertexArray(vaoAxis);
    glLineWidth(3.0f);

    glUniform3f(glGetUniformLocation(shader,"uSolidColor"), 1,0,0);
    glDrawArrays(GL_LINES, 0, 2);

    glUniform3f(glGetUniformLocation(shader,"uSolidColor"), 0,1,0);
    glDrawArrays(GL_LINES, 2, 2);

    glUniform3f(glGetUniformLocation(shader,"uSolidColor"), 0,0,1);
    glDrawArrays(GL_LINES, 4, 2);

    // --- 5. Restaurar viewport principal (IMPORTANTÍSSIMO!) ---
    glViewport(0, 0, screenW, screenH);
}

void RendererGL::drawNavmeshTriangles(const std::vector<glm::vec3>& tris, const glm::vec3& color, float alpha)
{
    if (tris.empty())
        return;

    std::vector<float> verts;
    verts.reserve(tris.size() * 3);

    for (const auto& v : tris)
    {
        verts.push_back(v.x);
        verts.push_back(v.y);
        verts.push_back(v.z);
    }

    glUseProgram(shader);
    glUniform1i(glGetUniformLocation(shader, "uRenderMode"), 99);
    glUniform3f(glGetUniformLocation(shader, "uSolidColor"), color.x, color.y, color.z);
    glUniform1f(glGetUniformLocation(shader, "uSolidAlpha"), alpha);

    glm::mat4 model(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"),
                       1, GL_FALSE, &model[0][0]);

    glBindVertexArray(navVao);
    glBindBuffer(GL_ARRAY_BUFFER, navVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 verts.size() * sizeof(float),
                 verts.data(),
                 GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(verts.size() / 3));
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

    glBindVertexArray(0);
}

void RendererGL::drawNavmeshLines(const std::vector<DebugLine>& lines, const glm::vec3& color, float lineWidth)
{
    if (lines.empty())
        return;

    // Monta um buffer de pontos (duas posições por linha)
    std::vector<float> verts;
    verts.reserve(lines.size() * 2 * 3);

    for (const auto& l : lines)
    {
        verts.push_back(l.x0); verts.push_back(l.y0); verts.push_back(l.z0);
        verts.push_back(l.x1); verts.push_back(l.y1); verts.push_back(l.z1);
    }

    glUseProgram(shader);

    // Queremos um "modo linha colorida fixa"
    glUniform1i(glGetUniformLocation(shader, "uRenderMode"), 99);
    glUniform3f(glGetUniformLocation(shader, "uSolidColor"), color.x, color.y, color.z);
    glUniform1f(glGetUniformLocation(shader, "uSolidAlpha"), 1.0f);

    glm::mat4 model(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader, "uModel"),
                       1, GL_FALSE, &model[0][0]);

    glBindVertexArray(navVao);
    glBindBuffer(GL_ARRAY_BUFFER, navVbo);

    glBufferData(GL_ARRAY_BUFFER,
                 verts.size() * sizeof(float),
                 verts.data(),
                 GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glLineWidth(lineWidth);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(verts.size() / 3));

    glBindVertexArray(0);
}
