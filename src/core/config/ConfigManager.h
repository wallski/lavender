#pragma once
#include <string>
#include <vector>

namespace ConfigManager {
    bool Initialize();
    void Shutdown();

    bool Save(const std::string& name);
    bool Load(const std::string& name);
    bool Delete(const std::string& name);
    bool Exists(const std::string& name);

    std::vector<std::string> List();

    const std::string& GetCurrentName();
    const std::string& GetDirectory();
}