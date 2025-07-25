#pragma once

#include "../../darkside.hpp"

struct calc_spread_output_t {
    float x{};
    float y{};
};

struct no_spread_result {
    bool bFound;
    vec3_t angAdjusted;
    int iFoundAfter;
    int iSeed;
};

class c_no_spread {
public:
    uint32_t compute_random_seed(vec3_t* view_angles, std::uint32_t tick_count)
    {
        using fn_t = uint32_t(__fastcall*)(void*, vec3_t*, std::uint32_t);
        static fn_t fn = reinterpret_cast<fn_t>(g_opcodes->scan(g_modules->m_modules.client_dll.get_name(), xorstr_("48 89 5C 24 08 57 48 81 EC F0 00")));
        return fn(nullptr, view_angles, tick_count);
    }

    calc_spread_output_t calculate_spread(int random_seed, float in_accuracy, float spread)
    {
        using func_t = void(__fastcall*)(int16_t, int, int, std::uint32_t, float, float, float, float*, float*);
        static func_t fn = reinterpret_cast<func_t>(g_opcodes->scan(g_modules->m_modules.client_dll.get_name(), xorstr_("48 8B C4 48 89 58 ? 48 89 68 ? 48 89 70 ? 57 41 54 41 55 41 56 41 57 48 81 EC ? ? ? ? 4C 63 EA")));

        calc_spread_output_t out{};

        int16_t weapon_index = 0;
        if (g_ctx->m_local_pawn) {
            c_base_player_weapon* weapon = g_ctx->m_local_pawn->get_active_weapon();
            if (weapon) {
                c_attribute_container* attr = weapon->m_attribute_manager();
                if (attr) {
                    c_econ_item_view* item = attr->m_item();
                    if (item)
                        weapon_index = static_cast<int16_t>(item->m_defenition_index());
                }
            }
        }

        fn(weapon_index, 1, 0, static_cast<std::uint32_t>(random_seed), in_accuracy, spread, 0.f, &out.x, &out.y);
        return out;
    }
};

inline const auto g_no_spread = std::make_unique<c_no_spread>();
