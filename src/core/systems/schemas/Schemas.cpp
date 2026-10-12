#include "pch.h"
#include "Schemas.h"
#include "../../interfaces/InterfaceManager.h"
#include "../../math/hash.hpp"

namespace systems {
    namespace schemas {

        static void* g_schemaSystem = nullptr;

        bool Initialize() {
            g_schemaSystem = InterfaceManager::Get<void>("schemasystem.dll", "SchemaSystem_001");
            if (!g_schemaSystem) {
                printf("[schemas] SchemaSystem_001 not found\n");
                fflush(stdout);
                return false;
            }

            printf("[schemas] SchemaSystem_001 = 0x%llX\n",
                (unsigned long long)g_schemaSystem);
            fflush(stdout);

            const auto test = lookup("CCSPlayerController", "m_hPlayerPawn"_hash);
            printf("[schemas] self-test: CCSPlayerController::m_hPlayerPawn = 0x%X\n", test);
            fflush(stdout);

            const auto gsn = lookup("C_BaseEntity", "m_pGameSceneNode"_hash);
            const auto simTime = lookup("C_BaseEntity", "m_flSimulationTime"_hash);
            const auto modelState = lookup("CSkeletonInstance", "m_modelState"_hash);
            const auto absOrigin = lookup("CGameSceneNode", "m_vecAbsOrigin"_hash);
            const auto absRot = lookup("CGameSceneNode", "m_angAbsRotation"_hash);
            const auto tickBase = lookup("CBasePlayerController", "m_nTickBase"_hash);

            printf("[schemas] diag: gsn=0x%X simTime=0x%X modelState=0x%X absOrigin=0x%X absRot=0x%X tickBase=0x%X\n",
                gsn, simTime, modelState, absOrigin, absRot, tickBase);
            fflush(stdout);

            return true;
        }

        void Shutdown() {
            g_schemaSystem = nullptr;
        }

        std::uint32_t lookup(const char* class_name, std::uint32_t field_hash) {
            if (!g_schemaSystem || !class_name) return 0;

            void* type_scope = nullptr;
            __try {
                void** vtable = *reinterpret_cast<void***>(g_schemaSystem);
                using GetScopeFn = void* (*)(void*, const char*, void*);
                auto fn = reinterpret_cast<GetScopeFn>(vtable[13]);
                type_scope = fn(g_schemaSystem, "client.dll", nullptr);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
            if (!type_scope) return 0;

            uintptr_t class_info = 0;
            __try {
                void** vtable = *reinterpret_cast<void***>(type_scope);
                using FindClassFn = void(*)(void*, uintptr_t*, const char*);
                auto fn = reinterpret_cast<FindClassFn>(vtable[2]);
                fn(type_scope, &class_info, class_name);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
            if (!class_info) return 0;

            uintptr_t fields_ptr = 0;
            uint16_t field_count = 0;
            __try {
                fields_ptr = *reinterpret_cast<uintptr_t*>(class_info + 0x30);
                field_count = *reinterpret_cast<uint16_t*>(class_info + 0x24);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
            if (!fields_ptr || !field_count) return 0;

            for (uint16_t i = 0; i < field_count; ++i) {
                const uintptr_t field_addr = fields_ptr + static_cast<size_t>(i) * 0x20;

                const char* name_ptr = nullptr;
                __try {
                    name_ptr = *reinterpret_cast<const char**>(field_addr);
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {
                    continue;
                }
                if (!name_ptr) continue;

                if (fnv1a::runtime_hash(name_ptr) == field_hash) {
                    uint32_t offset = 0;
                    __try {
                        offset = *reinterpret_cast<uint32_t*>(field_addr + 0x10);
                    }
                    __except (EXCEPTION_EXECUTE_HANDLER) {
                        return 0;
                    }
                    return offset;
                }
            }

            return 0;
        }

    }
}