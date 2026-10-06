#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace Skybox {

    struct Entry {
        std::string name;
        std::string path;
    };

    bool Initialize();
    void Shutdown();

    bool IsEnabled();
    void SetEnabled(bool enabled);

    int GetSelectedIndex();
    void SetSelectedIndex(int index);

    const std::vector<Entry>& GetEntries();

    void OnSkyboxDraw(void* mesh_array, uint32_t count);
}