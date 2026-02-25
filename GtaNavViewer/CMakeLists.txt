cmake_minimum_required(VERSION 3.15)

project(GtaNavViewer)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GTANAVVIEWER_BUILD_SHARED "Build GtaNavViewer as a shared library (DLL)" OFF)

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
set(UWEBSOCKETS_DIR "${EXTERNAL_DIR}/uWebSockets")

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

set(GTANAVVIEWER_CORE_SOURCES
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
    NavMesh_TileCacheDB.cpp
    NavMesh_TileCacheDB.h
    NavMesh_Tiled.cpp
    NavMeshData.cpp
    NavMeshData.h
    NavMeshBuild.h
    GtaHandler.cpp
    GtaHandler.h
    MemoryHandler.cpp
    MemoryHandler.h
    WebSockets.cpp
    WebSockets.h
    GtaHandlerMenu.cpp
    GtaHandlerMenu.h
    ExternC.cpp
    ExternC.h
    json.hpp
    imfilebrowser.h
    ${GLAD_SRC}
    ${IMGUI_SOURCES}
)

if (GTANAVVIEWER_BUILD_SHARED)
    add_library(GtaNavViewer SHARED ${GTANAVVIEWER_CORE_SOURCES})
    target_compile_definitions(GtaNavViewer PRIVATE GTANAVVIEWER_BUILD_DLL)
else()
    add_executable(GtaNavViewer main.cpp ${GTANAVVIEWER_CORE_SOURCES})
endif()

set(UWEBSOCKETS_AVAILABLE OFF)
if (EXISTS "${UWEBSOCKETS_DIR}/src/App.h")
    # uWebSockets usa libuv por padrão; só habilitamos se uv.h for encontrado.
    find_path(LIBUV_INCLUDE_DIR uv.h
        HINTS
            "${UWEBSOCKETS_DIR}/libuv/include"
            "${EXTERNAL_DIR}/libuv/include"
    )

    if (NOT LIBUV_INCLUDE_DIR)
        message(WARNING "uWebSockets encontrado, mas libuv (uv.h) não foi localizado; suporte desabilitado.")
    endif()

    file(GLOB_RECURSE UWEBSOCKETS_SOURCES
        "${UWEBSOCKETS_DIR}/src/*.c"
        "${UWEBSOCKETS_DIR}/src/*.cpp"
        "${UWEBSOCKETS_DIR}/uSockets/src/*.c"
        "${UWEBSOCKETS_DIR}/uSockets/src/*.cpp"
    )

    if (UWEBSOCKETS_SOURCES AND LIBUV_INCLUDE_DIR)
        add_library(uwebsockets STATIC ${UWEBSOCKETS_SOURCES})
        target_include_directories(uwebsockets PUBLIC
            ${UWEBSOCKETS_DIR}/src
            ${UWEBSOCKETS_DIR}/uSockets/src
            ${LIBUV_INCLUDE_DIR}
        )
        target_compile_definitions(uwebsockets PUBLIC LIBUS_NO_SSL)

        if (WIN32)
            target_link_libraries(uwebsockets PUBLIC ws2_32 userenv advapi32)
        elseif (UNIX)
            target_link_libraries(uwebsockets PUBLIC pthread)
        endif()

        set(UWEBSOCKETS_AVAILABLE ON)
    else()
        message(WARNING "uWebSockets encontrado, mas sem fontes C/C++ ou libuv; suporte desabilitado.")
    endif()
else()
    message(WARNING "uWebSockets não encontrado em ${UWEBSOCKETS_DIR}; suporte a WebSockets ficará desabilitado.")
endif()

# ---------------------------------------
# Link runtime que já existe
# ---------------------------------------
target_link_libraries(GtaNavViewer PRIVATE
    GtaNavRuntime   # vindo do /GtaNavRuntime/CMakeLists.txt
    ${SDL2_LIB}/SDL2.lib
    opengl32
)
if (NOT GTANAVVIEWER_BUILD_SHARED)
    target_link_libraries(GtaNavViewer PRIVATE ${SDL2_LIB}/SDL2main.lib)
endif()

if (UWEBSOCKETS_AVAILABLE)
    target_link_libraries(GtaNavViewer PRIVATE uwebsockets)
    target_compile_definitions(GtaNavViewer PRIVATE HAVE_UWEBSOCKETS)
endif()

# Copiar SDL2.dll automaticamente
if (NOT GTANAVVIEWER_BUILD_SHARED)
    add_custom_command(TARGET GtaNavViewer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${SDL2_LIB}/SDL2.dll"
            $<TARGET_FILE_DIR:GtaNavViewer>
    )
endif()