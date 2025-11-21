#include "GtaHandlerMenu.h"

#include <imgui.h>
#include <SDL.h>
#include <cstdio>
#include <fstream>

#include "GtaHandler.h"
#include "ViewerApp.h"

GtaHandlerMenu::GtaHandlerMenu()
    : instancesBrowser(ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_NoTitleBar)
    , meshDirectoryBrowser(ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_NoTitleBar)
{
    instancesBrowser.SetTitle("Selecionar navmesh_instances.json");
    instancesBrowser.SetTypeFilters({".json"});
    meshDirectoryBrowser.SetTitle("Selecionar pasta de meshes");

    LoadLastSelections();
}

void GtaHandlerMenu::Draw(GtaHandler& handler, ViewerApp& app)
{
    if (ImGui::Button("Selecionar navmesh_instances.json"))
    {
        instancesBrowser.SetPwd(instancesFile.empty() ? std::filesystem::current_path() : instancesFile.parent_path());
        instancesBrowser.Open();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(instancesFile.empty() ? "(nenhum arquivo selecionado)" : instancesFile.string().c_str());

    if (ImGui::Button("Selecionar pasta OBJ"))
    {
        meshDirectoryBrowser.SetPwd(meshDirectory.empty() ? std::filesystem::current_path() : meshDirectory);
        meshDirectoryBrowser.Open();
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(meshDirectory.empty() ? "(nenhuma pasta selecionada)" : meshDirectory.string().c_str());

    ImGui::BeginDisabled(instancesFile.empty());
    if (ImGui::Button("Carregar"))
    {
        if (handler.LoadInstances(instancesFile))
        {
            printf("[GtaHandlerMenu] navmesh_instances.json carregado de %s.\n", instancesFile.string().c_str());
        }
    }
    ImGui::EndDisabled();

    ImGui::SliderFloat("Scan Range", &scanRange, 0.0f, 10000.0f, "%.1f");

    if (ImGui::Checkbox("Procedural Test", &proceduralTest))
    {
        app.SetProceduralTestEnabled(proceduralTest);
    }

    bool canBuild = handler.HasCache() && !meshDirectory.empty();
    ImGui::BeginDisabled(!canBuild);
    if (ImGui::Button("Build Static Map"))
    {
        if (!handler.HasCache())
        {
            printf("[GtaHandlerMenu] Nenhum cache carregado.\n");
        }
        else if (meshDirectory.empty())
        {
            printf("[GtaHandlerMenu] Nenhuma pasta de meshes selecionada.\n");
        }
        else
        {
            app.BuildStaticMap(handler, meshDirectory, scanRange);
        }
    }
    ImGui::EndDisabled();

    instancesBrowser.Display();
    if (instancesBrowser.HasSelected())
    {
        instancesFile = instancesBrowser.GetSelected();
        instancesBrowser.ClearSelected();
        SaveLastSelections();
    }

    meshDirectoryBrowser.Display();
    if (meshDirectoryBrowser.HasSelected())
    {
        meshDirectory = meshDirectoryBrowser.GetSelected();
        meshDirectoryBrowser.ClearSelected();
        SaveLastSelections();
    }
}

std::filesystem::path GtaHandlerMenu::GetConfigFilePath() const
{
    char* basePath = SDL_GetBasePath();
    if (basePath)
    {
        std::filesystem::path result = std::filesystem::path(basePath) / "gta_handler_paths.txt";
        SDL_free(basePath);
        return result;
    }

    return "gta_handler_paths.txt";
}

void GtaHandlerMenu::LoadLastSelections()
{
    std::filesystem::path configPath = GetConfigFilePath();
    std::ifstream file(configPath);
    if (!file)
        return;

    std::string lastInstances;
    std::string lastMeshDir;

    std::getline(file, lastInstances);
    std::getline(file, lastMeshDir);

    if (!lastInstances.empty())
    {
        std::filesystem::path candidate = lastInstances;
        if (std::filesystem::exists(candidate))
            instancesFile = candidate;
    }

    if (!lastMeshDir.empty())
    {
        std::filesystem::path candidate = lastMeshDir;
        if (std::filesystem::exists(candidate) && std::filesystem::is_directory(candidate))
            meshDirectory = candidate;
    }
}

void GtaHandlerMenu::SaveLastSelections() const
{
    std::filesystem::path configPath = GetConfigFilePath();
    std::ofstream file(configPath, std::ios::trunc);
    if (!file)
        return;

    file << instancesFile.string() << "\n";
    file << meshDirectory.string() << "\n";
}
