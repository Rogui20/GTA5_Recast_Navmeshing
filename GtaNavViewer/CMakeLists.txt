cmake_minimum_required(VERSION 3.15)

project(GtaNavViewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------
# Diretórios base
# ---------------------------------------
set(ROOT_DIR "${PROJECT_SOURCE_DIR}/..")
set(EXTERNAL_DIR "${ROOT_DIR}/external")
set(SDL2_INCLUDE "${EXTERNAL_DIR}/SDL2/include")
set(SDL2_LIB     "${EXTERNAL_DIR}/SDL2/lib/x64")
set(GLAD_INCLUDE "${EXTERNAL_DIR}/glad/include")
set(GLAD_SRC     "${EXTERNAL_DIR}/glad/src/glad.c")
set(GLM_INCLUDE  "${EXTERNAL_DIR}/glm")
set(RUNTIME_DIR  "${ROOT_DIR}/GtaNavRuntime")

# Detour/Recast includes
set(DETOUR_INCLUDE "${ROOT_DIR}/Detour/Include")
set(RECAST_INCLUDE "${ROOT_DIR}/Recast/Include")

# ---------------------------------------
# Includes principais
# ---------------------------------------
include_directories(
    ${PROJECT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/imgui
    ${PROJECT_SOURCE_DIR}/imgui/backends

    ${SDL2_INCLUDE}
    ${GLAD_INCLUDE}
    ${GLM_INCLUDE}

    ${RUNTIME_DIR}
    ${DETOUR_INCLUDE}
    ${RECAST_INCLUDE}
)

# ---------------------------------------
# IMGUI sources
# ---------------------------------------
file(GLOB IMGUI_SOURCES
    imgui/*.cpp
    imgui/backends/imgui_impl_sdl2.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

# ---------------------------------------
# Viewer executable
# ---------------------------------------
add_executable(GtaNavViewer
    main.cpp
    ViewerApp.cpp
    ViewerApp.h
    ViewerAppInput.cpp
    ViewerAppUtils.cpp
    NavmeshUpdate.cpp
    Pathfind.cpp
    MathUtils.cpp
    MathUtils.h
    ViewerCamera.cpp
    ViewerCamera.h
    RendererGL.cpp
    RendererGL.h
    ObjLoader.cpp
    ObjLoader.h
    Mesh.cpp
    Mesh.h
    NavMesh_Single.cpp
    NavMesh_Tiled.cpp
    NavMeshData.cpp
    NavMeshData.h
    NavMeshBuild.h
    GtaHandler.cpp
    GtaHandler.h
    MemoryHandler.cpp
    MemoryHandler.h
    GtaHandlerMenu.cpp
    GtaHandlerMenu.h
    json.hpp
    imfilebrowser.h
    ${GLAD_SRC}
    ${IMGUI_SOURCES}
)

# ---------------------------------------
# Link runtime que já existe
# ---------------------------------------
target_link_libraries(GtaNavViewer
    GtaNavRuntime   # vindo do /GtaNavRuntime/CMakeLists.txt
    ${SDL2_LIB}/SDL2.lib
    ${SDL2_LIB}/SDL2main.lib
    opengl32
)

# Copiar SDL2.dll automaticamente
add_custom_command(TARGET GtaNavViewer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SDL2_LIB}/SDL2.dll"
        $<TARGET_FILE_DIR:GtaNavViewer>
)
