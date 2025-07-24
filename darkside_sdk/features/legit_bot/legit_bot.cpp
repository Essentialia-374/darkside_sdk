#include <intrin.h>
#include "legit_bot.hpp"
#include "../../darkside.hpp"


constexpr std::uint32_t k_trace_mask_shot_player = 0x1C300B;           // MASK_SHOT | CONTENTS_HITBOX | extra misc bits

void c_legit_bot::triggerbot()
{
    //  Fast‑fail sanity checks
    if (!g_interfaces->m_engine->is_in_game())
        return;

    c_user_cmd* const user_cmd = g_ctx->m_user_cmd;
    c_cs_player_pawn* const self = g_ctx->m_local_pawn;

    if (!user_cmd || !self || !self->is_alive())
        return;

    c_base_player_weapon* const gun = self->get_active_weapon();
    if (!gun)
        return;

    c_cs_weapon_base_v_data* const wid = gun->get_weapon_data();
    if (!wid)
        return;

    const auto wtype = wid->m_weapon_type();
    if (wtype == WEAPONTYPE_KNIFE || wtype == WEAPONTYPE_GRENADE)
        return;

    // Gun ready?
    if (gun->m_clip1() <= 0 || gun->m_next_primary_attack() > g_ctx->m_local_controller->m_tick_base())
        return;

    // Build a rays from current view‑angles
    vec3_t view{
        user_cmd->pb.mutable_base()->viewangles().x(),
        user_cmd->pb.mutable_base()->viewangles().y(),
        user_cmd->pb.mutable_base()->viewangles().z()
    };

    vec3_t dir; g_math->angle_vectors(view, dir);
    dir.normalize_in_place();

    const vec3_t start = self->get_eye_pos();
    const vec3_t end = start + dir * wid->m_range();
    
    // Prepare trace filter & ray
    trace_filter_t filter(k_trace_mask_shot_player, self, nullptr, /*collision group*/3);

    ray_t ray;          // Source 2 uses a *shape* ray – we must initialise it!
    g_interfaces->m_trace->init_trace(filter, self, k_trace_mask_shot_player, 0x3, 0x7);

    game_trace_t tr;
    g_interfaces->m_trace->trace_shape(
        &ray,                 // ray definition
        start, end,           // same endpoints – trace_shape() still needs them
        &filter,              // what to hit / skip
        &tr);                 // out‑result

    // If we hit a living enemy, set IN_ATTACK 
    if (tr.m_hit_entity && tr.m_hit_entity->is_player_pawn())
    {
        auto* const enemy = static_cast<c_cs_player_pawn*>(tr.m_hit_entity);
        if (enemy->is_alive() && enemy->m_team_num() != self->m_team_num())
            user_cmd->m_button_state.m_button_state |= IN_ATTACK;
    }
}

void c_legit_bot::on_create_move()
{
    triggerbot();
}
