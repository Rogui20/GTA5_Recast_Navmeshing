#include "GtaHandler.h"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <unordered_set>
#include <glm/geometric.hpp>

namespace
{
    bool IsArrayOfSize(const nlohmann::json& j, size_t expected)
    {
        return j.is_array() && j.size() >= expected;
    }
}

std::string GtaHandler::TrimYbnExtension(const std::string& sourceName)
{
    const std::string suffix = ".ybn.xml";
    if (sourceName.size() >= suffix.size())
    {
        auto tail = sourceName.substr(sourceName.size() - suffix.size());
        std::string lowerTail;
        lowerTail.resize(tail.size());
        std::transform(tail.begin(), tail.end(), lowerTail.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
        if (lowerTail == suffix)
        {
            return sourceName.substr(0, sourceName.size() - suffix.size());
        }
    }
    return sourceName;
}

bool GtaHandler::LoadInstances(const std::filesystem::path& jsonPath)
{
    instancesPath = jsonPath;
    cache.clear();

    if (!std::filesystem::exists(jsonPath))
    {
        printf("[GtaHandler] Arquivo %s não encontrado.\n", jsonPath.string().c_str());
        return false;
    }

    std::ifstream file(jsonPath);
    if (!file)
    {
        printf("[GtaHandler] Falha ao abrir %s.\n", jsonPath.string().c_str());
        return false;
    }

    nlohmann::json data;
    try
    {
        file >> data;
    }
    catch (const std::exception& e)
    {
        printf("[GtaHandler] Erro ao parsear JSON: %s\n", e.what());
        return false;
    }

    if (!data.is_array())
    {
        printf("[GtaHandler] JSON não é um array.\n");
        return false;
    }

    cache.reserve(data.size());

    for (size_t i = 0; i < data.size(); ++i)
    {
        const auto& inst = data[i];
        CachedInstance entry;

        // center calculation
        if (inst.contains("aabb") && inst["aabb"].is_object())
        {
            const auto& aabb = inst["aabb"];
            if (IsArrayOfSize(aabb.value("min", nlohmann::json::array()), 3) &&
                IsArrayOfSize(aabb.value("max", nlohmann::json::array()), 3))
            {
                glm::vec3 min = glm::vec3(
                    aabb["min"][0].get<float>(),
                    aabb["min"][1].get<float>(),
                    aabb["min"][2].get<float>());
                glm::vec3 max = glm::vec3(
                    aabb["max"][0].get<float>(),
                    aabb["max"][1].get<float>(),
                    aabb["max"][2].get<float>());
                entry.center = (min + max) * 0.5f;
            }
        }

        if (entry.center == glm::vec3(0.0f) && IsArrayOfSize(inst.value("pos", nlohmann::json::array()), 3))
        {
            entry.center = glm::vec3(
                inst["pos"][0].get<float>(),
                inst["pos"][1].get<float>(),
                inst["pos"][2].get<float>());
        }

        entry.radiusSq = 0.0f;
        if (inst.contains("radius"))
        {
            float r = 0.0f;
            try
            {
                r = inst["radius"].get<float>();
            }
            catch (...)
            {
                r = 0.0f;
            }
            entry.radiusSq = r * r;
        }

        std::string parentBase = TrimYbnExtension(inst.value("source", std::string{}));
        if (!parentBase.empty())
        {
            entry.sources.push_back(parentBase + ".obj");
        }

        if (inst.contains("subCollisions") && inst["subCollisions"].is_array())
        {
            const auto& subs = inst["subCollisions"];
            for (size_t subIdx = 0; subIdx < subs.size(); ++subIdx)
            {
                if (parentBase.empty())
                    break;
                std::string subName = parentBase + "_sub" + std::to_string(subIdx + 1) + ".obj";
                entry.sources.push_back(std::move(subName));
            }
        }

        cache.push_back(std::move(entry));
    }

    printf("[GtaHandler] Cache carregado: %zu instâncias.\n", cache.size());
    return true;
}

std::vector<std::string> GtaHandler::LookupSources(const glm::vec3& gtaPos, float range) const
{
    std::vector<std::string> results;
    if (cache.empty())
        return results;

    const float rangeSq = range * range;
    std::unordered_set<std::string> dedup;

    for (const auto& item : cache)
    {
        glm::vec3 delta = gtaPos - item.center;
        float distSq = glm::dot(delta, delta);
        if (distSq > rangeSq + item.radiusSq)
            continue;

        for (const auto& src : item.sources)
        {
            if (dedup.insert(src).second)
            {
                results.push_back(src);
            }
        }
    }

    return results;
}