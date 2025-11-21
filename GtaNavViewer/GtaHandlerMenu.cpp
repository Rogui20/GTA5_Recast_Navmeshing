#include "GtaHandlerMenu.h"

#include <imgui.h>
#include <cstdio>

#include "GtaHandler.h"
#include "ViewerApp.h"

GtaHandlerMenu::GtaHandlerMenu()
    : instancesBrowser(ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_NoTitleBar)
    , meshDirectoryBrowser(ImGuiFileBrowserFlags_SelectDirectory | ImGuiFileBrowserFlags_CloseOnEsc | ImGuiFileBrowserFlags_NoTitleBar)
{
    instancesBrowser.SetTitle("Selecionar navmesh_instances.json");
    instancesBrowser.SetTypeFilters({".json"});
    meshDirectoryBrowser.SetTitle("Selecionar pasta de meshes");
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
    }

    meshDirectoryBrowser.Display();
    if (meshDirectoryBrowser.HasSelected())
    {
        meshDirectory = meshDirectoryBrowser.GetSelected();
        meshDirectoryBrowser.ClearSelected();
    }
}
