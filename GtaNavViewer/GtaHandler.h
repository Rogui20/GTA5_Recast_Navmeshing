#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <glm/vec3.hpp>

class GtaHandler
{
public:
    bool LoadInstances(const std::filesystem::path& jsonPath);
    std::vector<std::string> LookupSources(const glm::vec3& gtaPos, float range) const;
    bool HasCache() const { return !cache.empty(); }
    std::filesystem::path GetInstancesPath() const { return instancesPath; }
    size_t CachedInstanceCount() const { return cache.size(); }

private:
    struct CachedInstance
    {
        glm::vec3 center{0.0f};
        float radiusSq = 0.0f;
        std::vector<std::string> sources;
    };

    static std::string TrimYbnExtension(const std::string& sourceName);

    std::filesystem::path instancesPath;
    std::vector<CachedInstance> cache;
};