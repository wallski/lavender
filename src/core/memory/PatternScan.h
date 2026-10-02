#pragma once
#include <Windows.h>
#include <string>
#include <vector>

namespace Memory {
    uintptr_t FindPattern(const std::string& moduleName, const std::string& pattern);


    uintptr_t ResolveRelativeAddress(uintptr_t instruction, int offset, int instructionSize);

    std::vector<uintptr_t> FindAllPatterns(const std::string& moduleName, const std::string& pattern);
}