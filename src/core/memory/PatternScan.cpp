#include "pch.h"
#include "PatternScan.h"
#include <Psapi.h>
#pragma comment(lib, "Psapi.lib")

namespace Memory {

 
    static bool ParsePattern(const std::string& pattern, std::vector<int>& bytes) {
        bytes.clear();
        std::istringstream iss(pattern);
        std::string byteStr;
        while (iss >> byteStr) {
            if (byteStr == "?" || byteStr == "??") {
                bytes.push_back(-1);
            }
            else {
                bytes.push_back(std::stoi(byteStr, nullptr, 16));
            }
        }
        return !bytes.empty();
    }

    uintptr_t FindPattern(const std::string& moduleName, const std::string& pattern) {
        std::vector<int> patternBytes;
        if (!ParsePattern(pattern, patternBytes)) return 0;

        HMODULE hModule = GetModuleHandleA(moduleName.c_str());
        if (!hModule) return 0;

        MODULEINFO modInfo;
        if (!GetModuleInformation(GetCurrentProcess(), hModule, &modInfo, sizeof(MODULEINFO))) return 0;

        uintptr_t moduleBase = reinterpret_cast<uintptr_t>(modInfo.lpBaseOfDll);
        size_t moduleSize = modInfo.SizeOfImage;

        for (uintptr_t i = 0; i < moduleSize - patternBytes.size(); i++) {
            bool found = true;
            for (size_t j = 0; j < patternBytes.size(); j++) {
                if (patternBytes[j] != -1 && *reinterpret_cast<uint8_t*>(moduleBase + i + j) != patternBytes[j]) {
                    found = false;
                    break;
                }
            }
            if (found) {
                return moduleBase + i;
            }
        }
        return 0;
    }

    uintptr_t ResolveRelativeAddress(uintptr_t instruction, int offset, int instructionSize) {

        int32_t relativeOffset = *reinterpret_cast<int32_t*>(instruction + offset);
        return instruction + instructionSize + relativeOffset;
    }
}