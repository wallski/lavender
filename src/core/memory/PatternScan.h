#pragma once
#include <Windows.h>
#include <string>
#include <vector>
#include <cstdint>

namespace Memory {
    uintptr_t FindPattern(const std::string& moduleName, const std::string& pattern);
    uintptr_t ResolveRelativeAddress(uintptr_t instruction, int offset, int instructionSize);
    std::vector<uintptr_t> FindAllPatterns(const std::string& moduleName, const std::string& pattern);
    uintptr_t GetExport(const std::string& moduleName, const std::string& exportName);

    template <typename Ret = void, typename... Args>
    Ret Call(uintptr_t address, Args... args) {
        using Fn = Ret(*)(Args...);
        return reinterpret_cast<Fn>(address)(args...);
    }
}