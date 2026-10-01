#pragma once
#include <Windows.h>
#include <string>
#include <vector>

namespace Memory {
    // Scans a module for a specific byte pattern (IDA format).
    // Example: "48 8B 05 ? ? ? ? 48 85 C0 74 ?"
    uintptr_t FindPattern(const std::string& moduleName, const std::string& pattern);

    // Helper to resolve relative addresses (RIP-relative) used in x64.
    // Many functions are called via relative offsets, so we need to calculate the absolute address.
    uintptr_t ResolveRelativeAddress(uintptr_t instruction, int offset, int instructionSize);
}