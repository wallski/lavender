#include "pch.h"
#include "OffsetConfig.h"
#include "../core/memory/PatternScan.h"
#include <fstream>
#include <sstream>
#include <cctype>

namespace OffsetConfig {
    Globals globals;
    Fields  fields;
    bool    loaded = false;

    static std::string GetDllDirectory() {
        HMODULE hm = NULL;
        GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCSTR)&GetDllDirectory, &hm);
        char path[MAX_PATH] = { 0 };
        GetModuleFileNameA(hm, path, MAX_PATH);
        std::string s(path);
        size_t pos = s.find_last_of("\\/");
        return (pos == std::string::npos) ? s : s.substr(0, pos);
    }

    static std::string AutoDetectA2xFolder() {
        std::string dir = GetDllDirectory();
        for (int i = 0; i < 5; i++) {
            size_t pos = dir.find_last_of("\\/");
            if (pos == std::string::npos) break;
            dir = dir.substr(0, pos);
            std::string candidate = dir + "\\a2x\\output";
            std::ifstream test(candidate + "\\offsets.json");
            if (test.is_open()) return candidate;
        }
        return "C:\\lavender\\a2x";
    }

    static std::string ReadWholeFile(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) return "";
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }

    static bool ExtractJsonNumber(const std::string& json, const std::string& key, uint64_t& out) {
        std::string search = "\"" + key + "\"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return false;
        pos = json.find(':', pos + search.size());
        if (pos == std::string::npos) return false;
        pos++;
        while (pos < json.size() && std::isspace((unsigned char)json[pos])) pos++;
        size_t end = pos;
        while (end < json.size() && (std::isalnum((unsigned char)json[end]) || json[end] == 'x' || json[end] == 'X')) end++;
        if (end == pos) return false;
        std::string num = json.substr(pos, end - pos);
        try {
            int base = 10;
            if (num.size() > 2 && num[0] == '0' && (num[1] == 'x' || num[1] == 'X')) base = 16;
            out = std::stoull(num, nullptr, base);
            return true;
        }
        catch (...) { return false; }
    }

    static bool ExtractHppConstant(const std::string& hpp, const std::string& name, uint64_t& out) {
        size_t pos = 0;
        while ((pos = hpp.find(name, pos)) != std::string::npos) {
            bool leftOk = (pos == 0) || !std::isalnum((unsigned char)hpp[pos - 1]);
            size_t after = pos + name.size();
            bool rightOk = (after >= hpp.size()) || !std::isalnum((unsigned char)hpp[after]);
            if (leftOk && rightOk) {
                size_t eq = hpp.find('=', after);
                if (eq != std::string::npos && eq - after < 200) {
                    size_t vstart = eq + 1;
                    while (vstart < hpp.size() && std::isspace((unsigned char)hpp[vstart])) vstart++;
                    if (vstart + 2 < hpp.size() && hpp[vstart] == '0' && (hpp[vstart + 1] == 'x' || hpp[vstart + 1] == 'X')) {
                        size_t vend = vstart + 2;
                        while (vend < hpp.size() && std::isxdigit((unsigned char)hpp[vend])) vend++;
                        try {
                            out = std::stoull(hpp.substr(vstart, vend - vstart), nullptr, 16);
                            return true;
                        }
                        catch (...) {}
                    }
                }
            }
            pos += name.size();
        }
        return false;
    }

    bool Load() {
        return Load(AutoDetectA2xFolder());
    }

    bool Load(const std::string& a2xFolder) {
        std::string jsonPath = a2xFolder + "\\offsets.json";
        std::string hppPath = a2xFolder + "\\client_dll.hpp";

        printf("[OffsetConfig] Reading from: %s\n", a2xFolder.c_str()); fflush(stdout);

        std::string json = ReadWholeFile(jsonPath);
        if (json.empty()) {
            printf("[OffsetConfig] Cannot read %s\n", jsonPath.c_str()); fflush(stdout);
            return false;
        }

        uint64_t v = 0;
        if (ExtractJsonNumber(json, "dwEntityList", v)) globals.dwEntityList = (uintptr_t)v;
        if (ExtractJsonNumber(json, "dwLocalPlayerController", v)) globals.dwLocalPlayerController = (uintptr_t)v;
        if (ExtractJsonNumber(json, "dwLocalPlayerPawn", v)) globals.dwLocalPlayerPawn = (uintptr_t)v;
        if (ExtractJsonNumber(json, "dwViewMatrix", v)) globals.dwViewMatrix = (uintptr_t)v;
        if (ExtractJsonNumber(json, "dwGlobalVars", v)) globals.dwGlobalVars = (uintptr_t)v;
        if (ExtractJsonNumber(json, "dwGameRules", v)) globals.dwGameRules = (uintptr_t)v;
        if (ExtractJsonNumber(json, "dwInputSystem", v)) globals.dwInputSystem = (uintptr_t)v;

        std::string hpp = ReadWholeFile(hppPath);
        if (!hpp.empty()) {
            if (ExtractHppConstant(hpp, "m_iHealth", v)) fields.m_iHealth = (uint32_t)v;
            if (ExtractHppConstant(hpp, "m_iTeamNum", v)) fields.m_iTeamNum = (uint32_t)v;
            if (ExtractHppConstant(hpp, "m_lifeState", v)) fields.m_lifeState = (uint32_t)v;
            if (ExtractHppConstant(hpp, "m_vecOrigin", v)) fields.m_vecOrigin = (uint32_t)v;
            if (ExtractHppConstant(hpp, "m_hPlayerPawn", v)) fields.m_hPlayerPawn = (uint32_t)v;
            if (ExtractHppConstant(hpp, "m_bDormant", v)) fields.m_bDormant = (uint32_t)v;
        }
        else {
            printf("[OffsetConfig] WARN: cannot read %s\n", hppPath.c_str()); fflush(stdout);
        }

        loaded = true;
        return true;
    }

    static uintptr_t GetModuleBase(const char* name) {
        HMODULE h = GetModuleHandleA(name);
        return h ? reinterpret_cast<uintptr_t>(h) : 0;
    }

    static uintptr_t ResolveMovRip(const char* module, const char* pattern) {
        uintptr_t instr = Memory::FindPattern(module, pattern);
        if (!instr) return 0;
        int32_t rel = *reinterpret_cast<int32_t*>(instr + 3);
        return instr + 7 + rel;
    }

    void Resolve() {
        uintptr_t clientBase = GetModuleBase("client.dll");
        uintptr_t engineBase = GetModuleBase("engine2.dll");
        uintptr_t inputBase = GetModuleBase("inputsystem.dll");

        printf("[OffsetConfig] Resolving RVAs (client=0x%llX engine=0x%llX input=0x%llX)\n",
            (unsigned long long)clientBase, (unsigned long long)engineBase, (unsigned long long)inputBase); fflush(stdout);

        globals.dwEntityList += clientBase;
        globals.dwLocalPlayerController += clientBase;
        globals.dwLocalPlayerPawn += clientBase;
        globals.dwViewMatrix += clientBase;
        globals.dwGameRules += clientBase;
        globals.dwGlobalVars += engineBase;
        globals.dwInputSystem += inputBase;

        if (!globals.dwInputSystem) {
            globals.dwInputSystem = ResolveMovRip("inputsystem.dll", "48 8B 0D ? ? ? ? 48 8B 01 FF 50");
            if (globals.dwInputSystem) {
                printf("[OffsetConfig] dwInputSystem fallback resolved to 0x%llX\n", (unsigned long long)globals.dwInputSystem);
            }
        }

        printf("[OffsetConfig] Resolved absolute addresses:\n");
        printf("  dwEntityList            = 0x%llX\n", (unsigned long long)globals.dwEntityList);
        printf("  dwLocalPlayerController = 0x%llX\n", (unsigned long long)globals.dwLocalPlayerController);
        printf("  dwLocalPlayerPawn       = 0x%llX\n", (unsigned long long)globals.dwLocalPlayerPawn);
        printf("  dwViewMatrix            = 0x%llX\n", (unsigned long long)globals.dwViewMatrix);
        printf("  dwGameRules             = 0x%llX\n", (unsigned long long)globals.dwGameRules);
        printf("  dwGlobalVars            = 0x%llX\n", (unsigned long long)globals.dwGlobalVars);
        printf("  dwInputSystem           = 0x%llX\n", (unsigned long long)globals.dwInputSystem);
        fflush(stdout);
    }
}