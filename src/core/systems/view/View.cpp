#include "pch.h"
#include "View.h"
#include "../../../config/OffsetConfig.h"

namespace systems {

    static bool SafeReadMatrix(std::uintptr_t addr, math::matrix4x4& out) {
        __try {
            out = *reinterpret_cast<math::matrix4x4*>(addr);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
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

    static float SafeReadFloat(std::uintptr_t addr) {
        __try { return *reinterpret_cast<float*>(addr); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return view::k_invalid; }
        return view::k_invalid;
    }

    static void GetViewport(float& out_w, float& out_h) {
        out_w = 1920.0f;
        out_h = 1080.0f;

        HWND hwnd = GetForegroundWindow();
        if (!hwnd) return;

        RECT rect;
        if (!GetClientRect(hwnd, &rect)) return;

        out_w = static_cast<float>(rect.right - rect.left);
        out_h = static_cast<float>(rect.bottom - rect.top);
    }

    void view::update(std::uintptr_t view_ptr) {
        this->update_matrix();

        if (view_ptr) {
            this->m_origin = SafeReadVec3(view_ptr + 0x0);
            this->m_angles = SafeReadVec3(view_ptr + 0xc);
            this->m_fov = SafeReadFloat(view_ptr + 0x18);
        }
        else {
            this->m_origin = { k_invalid, k_invalid, k_invalid };
            this->m_angles = { k_invalid, k_invalid, k_invalid };
            this->m_fov = k_invalid;
        }
    }

    void view::update_matrix() {
        if (!OffsetConfig::globals.dwViewMatrix) {
            this->m_matrix_valid.store(false, std::memory_order_release);
            return;
        }

        math::matrix4x4 matrix{};
        if (!SafeReadMatrix(OffsetConfig::globals.dwViewMatrix, matrix)) {
            this->m_matrix_valid.store(false, std::memory_order_release);
            return;
        }

        auto valid = true;
        auto magnitude = 0.0f;

        for (auto row = 0; row < 4; ++row) {
            for (auto column = 0; column < 4; ++column) {
                const auto value = matrix[row][column];
                valid = valid && std::isfinite(value);
                magnitude += std::fabsf(value);
            }
        }

        valid = valid && magnitude > 0.001f;
        if (valid) {
            std::scoped_lock lock(this->m_matrix_mtx);
            this->m_matrix = matrix;
        }

        this->m_matrix_valid.store(valid, std::memory_order_release);
    }

    math::vector2 view::project(const math::vector3& world_pos) {
        if (!this->has_camera()) {
            return { k_invalid, k_invalid };
        }

        const auto m = this->matrix();

        const auto w = m[3][0] * world_pos.x + m[3][1] * world_pos.y + m[3][2] * world_pos.z + m[3][3];
        if (w < 0.001f) {
            return { k_invalid, k_invalid };
        }

        const auto x = m[0][0] * world_pos.x + m[0][1] * world_pos.y + m[0][2] * world_pos.z + m[0][3];
        const auto y = m[1][0] * world_pos.x + m[1][1] * world_pos.y + m[1][2] * world_pos.z + m[1][3];

        float sw = 0.0f, sh = 0.0f;
        GetViewport(sw, sh);

        const auto inv_w = 1.0f / w;

        return {
            sw * 0.5f * (1.0f + x * inv_w),
            sh * 0.5f * (1.0f - y * inv_w)
        };
    }

    view::projection view::project_full(const math::vector3& world_pos) const {
        if (!this->has_camera()) {
            return {};
        }

        const auto m = this->matrix();

        const auto w = m[3][0] * world_pos.x + m[3][1] * world_pos.y + m[3][2] * world_pos.z + m[3][3];
        if (w < 0.001f) {
            return {};
        }

        const auto x = m[0][0] * world_pos.x + m[0][1] * world_pos.y + m[0][2] * world_pos.z + m[0][3];
        const auto y = m[1][0] * world_pos.x + m[1][1] * world_pos.y + m[1][2] * world_pos.z + m[1][3];

        float sw = 0.0f, sh = 0.0f;
        GetViewport(sw, sh);

        const auto inv_w = 1.0f / w;

        const math::vector2 screen{
            sw * 0.5f * (1.0f + x * inv_w),
            sh * 0.5f * (1.0f - y * inv_w)
        };

        projection r{};
        r.screen = screen;
        r.w = w;
        r.on_screen = w > 0.01f && screen.x >= 0.0f && screen.x < sw && screen.y >= 0.0f && screen.y < sh;
        return r;
    }

    math::matrix4x4 view::matrix() const {
        std::scoped_lock lock(this->m_matrix_mtx);
        return this->m_matrix;
    }

}