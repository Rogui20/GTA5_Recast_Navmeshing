# GtaNavRuntime/CMakeLists.txt

set(GTANAV_SRC
    GtaNav.cpp
    GtaNavAPI.cpp
    GtaNavContext.cpp
    GtaNavGeometry.cpp
    GtaNavProps.cpp
    GtaNavTiles.cpp
)

add_library(GtaNavRuntime STATIC ${GTANAV_SRC})

target_include_directories(GtaNavRuntime PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/glm
    ${PROJECT_SOURCE_DIR}/Recast
    ${PROJECT_SOURCE_DIR}/Detour
    ${PROJECT_SOURCE_DIR}/DebugUtils
)

target_link_libraries(GtaNavRuntime PUBLIC
    Recast
    Detour
    DetourTileCache
    DetourCrowd
    DebugUtils
)
