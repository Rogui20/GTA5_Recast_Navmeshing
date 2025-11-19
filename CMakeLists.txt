cmake_minimum_required(VERSION 3.1...4.0)

project(RecastNavigation)

# ---------------------------------------------------------
# Configuração global
# ---------------------------------------------------------
SET(SOVERSION 1)
set(LIB_VERSION 1.6.0)
string(REPLACE "." "," LIB_VERSION_NUM "${LIB_VERSION}.0")

set_property(GLOBAL PROPERTY CXX_STANDARD 17)   # <--- atualizado
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(RECASTNAVIGATION_DEMO "Build demo" OFF)
option(RECASTNAVIGATION_TESTS "Build tests" ON)
option(RECASTNAVIGATION_EXAMPLES "Build examples" ON)
option(RECASTNAVIGATION_DT_POLYREF64 "Use 64bit polyrefs instead of 32bit for Detour" OFF)
option(RECASTNAVIGATION_DT_VIRTUAL_QUERYFILTER "Use dynamic dispatch for dtQueryFilter" OFF)
option(RECASTNAVIGATION_ENABLE_ASSERTS "Enable custom recastnavigation asserts" "$<IF:$<CONFIG:Debug>,ON,OFF>")

if(MSVC AND BUILD_SHARED_LIBS)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)

# ---------------------------------------------------------
# RecastNavigation core
# ---------------------------------------------------------
configure_file(
    "${RecastNavigation_SOURCE_DIR}/version.h.in"
    "${RecastNavigation_BINARY_DIR}/version.h"
)

install(
    FILES "${RecastNavigation_BINARY_DIR}/version.h"
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/recastnavigation
)

# pkgconfig
set(prefix ${CMAKE_INSTALL_PREFIX})
set(exec_prefix "\${prefix}")
set(libdir "${CMAKE_INSTALL_FULL_LIBDIR}")
set(bindir "${CMAKE_INSTALL_FULL_BINDIR}")
set(includedir "${CMAKE_INSTALL_FULL_INCLUDEDIR}")

set(PACKAGE_VERSION "${LIB_VERSION}")

configure_file(
    "${RecastNavigation_SOURCE_DIR}/recastnavigation.pc.in"
    "${RecastNavigation_BINARY_DIR}/recastnavigation.pc"
    @ONLY
)

install(
    FILES "${RecastNavigation_BINARY_DIR}/recastnavigation.pc"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig"
)

# ---------------------------------------------------------
# Subprojetos nativos RecastNavigation
# ---------------------------------------------------------
add_subdirectory(DebugUtils)
add_subdirectory(Detour)
add_subdirectory(DetourCrowd)
add_subdirectory(DetourTileCache)
add_subdirectory(Recast)

# ---------------------------------------------------------
# 🔥 NOVO: GtaNavRuntime
# ---------------------------------------------------------
# (toda a pasta que você me mandou)
add_subdirectory(GtaNavRuntime)
add_subdirectory(GtaNavViewer)


# ---------------------------------------------------------
# Instalação e exportação da lib principal
# ---------------------------------------------------------
configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/recastnavigation-config.cmake.in
    recastnavigation-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/recastnavigation
)

write_basic_package_version_file(
    recastnavigation-config-version.cmake
    VERSION ${LIB_VERSION}
    COMPATIBILITY AnyNewerVersion
)

export(
    EXPORT recastnavigation-targets
    NAMESPACE RecastNavigation::
    FILE recastnavigation-targets.cmake
)

install(
    EXPORT recastnavigation-targets
    NAMESPACE RecastNavigation::
    FILE recastnavigation-targets.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/recastnavigation
)

install(
    FILES
        ${PROJECT_BINARY_DIR}/recastnavigation-config.cmake
        ${PROJECT_BINARY_DIR}/recastnavigation-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/recastnavigation
)

# ---------------------------------------------------------
# RecastDemo original (opcional)
# ---------------------------------------------------------
if (RECASTNAVIGATION_DEMO)
    add_subdirectory(RecastDemo)
endif ()

# ---------------------------------------------------------
# Tests
# ---------------------------------------------------------
if (RECASTNAVIGATION_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif ()
