#pragma once
#include <Windows.h>
#include <string>

class InterfaceManager {
public:

    template <typename T>
    static T* Get(const std::string& moduleName, const std::string& interfaceName) {

        HMODULE hModule = GetModuleHandleA(moduleName.c_str());
        if (!hModule) return nullptr;


        using CreateInterfaceFn = void* (*)(const char*, int*);


        CreateInterfaceFn createInterface = reinterpret_cast<CreateInterfaceFn>(
            GetProcAddress(hModule, "CreateInterface")
            );
        if (!createInterface) return nullptr;


        int returnCode = 0;
        void* interfacePtr = createInterface(interfaceName.c_str(), &returnCode);


        return static_cast<T*>(interfacePtr);
    }
};