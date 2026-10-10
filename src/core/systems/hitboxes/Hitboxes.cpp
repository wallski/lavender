#include "pch.h"
#include "Hitboxes.h"

namespace systems {

    static std::uintptr_t SafeReadPtr(std::uintptr_t addr) {
        __try { return *reinterpret_cast<std::uintptr_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
    }

    static std::uint16_t SafeReadU16(std::uintptr_t addr) {
        __try { return *reinterpret_cast<std::uint16_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return 0;
    }

    static std::int16_t SafeReadI16(std::uintptr_t addr) {
        __try { return *reinterpret_cast<std::int16_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return 0;
    }

    static std::uint32_t SafeReadU32(std::uintptr_t addr) {
        __try { return *reinterpret_cast<std::uint32_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return 0;
    }

    static int SafeReadInt(std::uintptr_t addr) {
        __try { return *reinterpret_cast<int*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return 0;
    }

    static float SafeReadFloat(std::uintptr_t addr) {
        __try { return *reinterpret_cast<float*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0.0f; }
        return 0.0f;
    }

    static std::uint8_t SafeReadU8(std::uintptr_t addr) {
        __try { return *reinterpret_cast<std::uint8_t*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        return 0;
    }

    static math::vector3 SafeReadVec3(std::uintptr_t addr) {
        math::vector3 v{};
        __try {
            v.x = *reinterpret_cast<float*>(addr + 0);
            v.y = *reinterpret_cast<float*>(addr + 4);
            v.z = *reinterpret_cast<float*>(addr + 8);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return v;
    }

    hitboxes::set hitboxes::query(std::uintptr_t game_scene_node, bool seh) {
        (void)seh;
        set result{};

        if (game_scene_node < 0x10000) return result;

        auto model_handle = SafeReadPtr(game_scene_node + 0x1E0);
        if (!model_handle) {
            model_handle = SafeReadPtr(game_scene_node + 0x210);
        }
        if (!model_handle) return result;

        const auto cmodel = SafeReadPtr(model_handle);
        if (!cmodel) return result;

        const auto render_mesh_array = SafeReadPtr(cmodel + 0x78);
        if (!render_mesh_array) return result;

        const auto render_meshes = SafeReadPtr(render_mesh_array);
        if (!render_meshes) return result;

        std::uintptr_t hitbox_set = 0;

        const auto set_count = SafeReadInt(render_meshes + 0x174);
        if (set_count > 0 && set_count <= 32) {
            const auto storage_flags = SafeReadU32(render_meshes + 0x164);
            hitbox_set = (storage_flags & 0x7fffffff)
                ? SafeReadPtr(render_meshes + 0x168)
                : render_meshes + 0x168;
        }

        if (!hitbox_set) {
            hitbox_set = SafeReadPtr(render_meshes + 0x150);
        }
        if (!hitbox_set) return result;

        const auto count = SafeReadInt(hitbox_set + 0x28);
        if (count <= 0 || count > 20) return result;

        const auto array_ptr = SafeReadPtr(hitbox_set + 0x30);
        if (!array_ptr) return result;

        const auto remap_count = SafeReadInt(cmodel + 0x220);
        const auto remap_table = SafeReadPtr(cmodel + 0x228);
        const auto mesh_a = SafeReadPtr(cmodel + 0x240);
        const auto mesh_b = SafeReadPtr(cmodel + 0x2F0);

        constexpr auto k_hitbox_stride = 0x70;

        for (auto i = 0; i < count; ++i) {
            const auto base = array_ptr + static_cast<std::size_t>(i) * k_hitbox_stride;

            auto bone = -1;

            if (remap_table && mesh_a && mesh_b && remap_count > 0) {
                const auto hb_idx = SafeReadU16(base + 0x48);
                const auto ofs_a = SafeReadU16(mesh_a);
                const auto ofs_b = SafeReadU16(mesh_b);
                const auto slot = static_cast<std::size_t>(hb_idx + ofs_a + ofs_b);

                if (slot < static_cast<std::size_t>(remap_count)) {
                    bone = SafeReadI16(remap_table + 2 * slot);
                }
            }

            if (bone < 0) continue;

            const auto radius = SafeReadFloat(base + 0x30);
            if (radius < 0.0f || radius > 100.0f) continue;

            auto& hb = result.entries[result.count++];
            hb.index = i;
            hb.bone = bone;
            hb.mins = SafeReadVec3(base + 0x18);
            hb.maxs = SafeReadVec3(base + 0x24);
            hb.radius = radius;
            hb.shape_type = SafeReadU8(base + 0x3C);
            hb.translation_only = SafeReadU8(base + 0x3D) != 0;
        }

        return result;
    }

    int hitboxes::hitgroup_from_hitbox(int hitbox) {
        switch (hitbox) {
        case 0:  return 1;
        case 1:  return 8;
        case 2:  return 3;
        case 3:  return 3;
        case 4:  return 2;
        case 5:  return 2;
        case 6:  return 2;
        case 7:  return 7;
        case 8:  return 6;
        case 9:  return 7;
        case 10: return 6;
        case 11: return 7;
        case 12: return 6;
        case 13: return 5;
        case 14: return 4;
        case 15: return 5;
        case 16: return 5;
        case 17: return 4;
        case 18: return 4;
        default: return 0;
        }
    }

    const char* hitboxes::hitgroup_to_name(int hitgroup) {
        constexpr const char* k_names[]{
            "body", "head", "chest", "stomach",
            "left arm", "right arm", "left leg", "right leg", "neck"
        };
        return hitgroup < 9 ? k_names[hitgroup] : "body";
    }

}