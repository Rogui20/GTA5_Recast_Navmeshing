#pragma once

#include <filesystem>
#include <string>

#include "imfilebrowser.h"

class GtaHandler;
class ViewerApp;

class GtaHandlerMenu
{
public:
    GtaHandlerMenu();
    void Draw(GtaHandler& handler, ViewerApp& app);

    float GetScanRange() const { return scanRange; }
    const std::filesystem::path& GetMeshDirectory() const { return meshDirectory; }
    bool IsProceduralTestEnabled() const { return proceduralTest; }

private:
    std::filesystem::path instancesFile;
    std::filesystem::path meshDirectory;
    float scanRange = 100.0f;
    bool proceduralTest = false;

    ImGui::FileBrowser instancesBrowser;
    ImGui::FileBrowser meshDirectoryBrowser;
};
