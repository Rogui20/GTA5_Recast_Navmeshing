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

private:
    std::filesystem::path instancesFile;
    std::filesystem::path meshDirectory;
    float scanRange = 100.0f;

    ImGui::FileBrowser instancesBrowser;
    ImGui::FileBrowser meshDirectoryBrowser;
};
