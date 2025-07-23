#include "menu.hpp"

#include "../darkside.hpp"
#include "../features/skins/skins.hpp"
#include "custom.hpp"

static void DrawWindowTitleCentered(const char* title,
    float bar_height = 24.0f,  
    ImU32  bg_color = 0,       
    ImU32  text_color = 0,       
    ImU32  border_color = 0)       
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (!window || window->SkipItems)
        return;

    const ImGuiStyle& style = ImGui::GetStyle();
    if (bg_color == 0)
        bg_color = ImGui::GetColorU32(ImGuiCol_TitleBg);
    
    if (text_color == 0) 
        text_color = ImGui::GetColorU32(ImGuiCol_Text);
    
    if (border_color == 0) 
        border_color = ImGui::GetColorU32(ImGuiCol_Border);

    ImDrawList* draw = ImGui::GetWindowDrawList();

    const ImVec2 bar_min = window->Pos;
    const ImVec2 bar_max = ImVec2(window->Pos.x + window->Size.x, window->Pos.y + bar_height);
    draw->AddRectFilled(bar_min, bar_max, bg_color);
    draw->AddLine(ImVec2(bar_min.x, bar_max.y), ImVec2(bar_max.x, bar_max.y), border_color);

    const ImVec2 text_size = ImGui::CalcTextSize(title);
    const ImVec2 text_pos{
        bar_min.x + (window->Size.x - text_size.x) * 0.5f,
        bar_min.y + (bar_height - text_size.y) * 0.5f
    };
    draw->AddText(text_pos, text_color, title);

    ImGui::Dummy(ImVec2(0.0f, bar_height - style.FramePadding.y * 2.0f));
}

void c_menu::draw() {
    if (!m_opened)
        return;

    ImGui::SetNextWindowSize(ImVec2(512.0f, 515.0f), ImGuiCond_Once);
    const auto window_flags = ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse;

    ImGui::Begin("##cs2_window", nullptr, window_flags);
    DrawWindowTitleCentered("Lambdahook");

    ImGui::BeginChild("tabs", ImVec2{ 150, 0 }, true);
    {
        static constexpr const char* tabs[]{ "Rage", "Anti-aim", "World esp", "Player esp", "Misc", "Skins", "Settings" };

        for (std::size_t i{ }; i < IM_ARRAYSIZE(tabs); ++i) {
            if (ImGui::Selectable(tabs[i], m_selected_tab == i)) {
                m_selected_tab = i;
            }
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    if (m_selected_tab == 0) {
        ImGui::BeginChild("ragebot", ImVec2{ 0, 0 }, true);
        {
            const char* hitboxes[] = { "head", "chest", "stomach", "arms", "legs", "feet" };
            const char* multi_points[] = { "head", "chest", "stomach", "arms", "legs", "feet" };

            ImGui::Checkbox("enabled", &g_cfg->rage_bot.m_enabled);
            ImGui::Checkbox("silent", &g_cfg->rage_bot.m_silent);
            ImGui::MultiCombo("hiboxes", g_cfg->rage_bot.m_hitboxes, hitboxes, IM_ARRAYSIZE(hitboxes));
            ImGui::MultiCombo("multi points", g_cfg->rage_bot.m_multi_points, multi_points, IM_ARRAYSIZE(multi_points));

            if (g_cfg->rage_bot.m_multi_points[0]
                || g_cfg->rage_bot.m_multi_points[1]
                || g_cfg->rage_bot.m_multi_points[2]
                || g_cfg->rage_bot.m_multi_points[3]
                || g_cfg->rage_bot.m_multi_points[4]
                || g_cfg->rage_bot.m_multi_points[5]) {
                ImGui::Checkbox("static scale", &g_cfg->rage_bot.m_static_scale);

                if (g_cfg->rage_bot.m_static_scale) {
                    // open the Point scale window without the default title bar
                    ImGui::Begin("##PointScaleWindow", nullptr,
                        ImGuiWindowFlags_AlwaysAutoResize |
                        ImGuiWindowFlags_NoCollapse |
                        ImGuiWindowFlags_NoTitleBar);

                    // draw our centered, custom title
                    DrawWindowTitleCentered("Point scale");

                    // now the individual hitbox‑scale sliders
                    for (int i = 0; i < IM_ARRAYSIZE(multi_points); i++) {
                        if (g_cfg->rage_bot.m_multi_points[i]) {
                            ImGui::SliderInt(
                                std::format("{} scale", multi_points[i]).c_str(),
                                &g_cfg->rage_bot.m_hitbox_scale[i],
                                1, 100
                            );
                        }
                    }

                    ImGui::End();
                }
            }

            ImGui::SliderInt("minimum damage", &g_cfg->rage_bot.m_minimum_damage, 1, 100);
            ImGui::SliderInt("damage override", &g_cfg->rage_bot.m_minimum_damage_override, 1, 100);
            ImGui::Keybind("damage override keybind", &g_cfg->rage_bot.m_override_damage_key_bind, &g_cfg->rage_bot.m_override_damage_key_bind_style);
            ImGui::SliderInt("hit chance", &g_cfg->rage_bot.m_hit_chance, 1, 100);
            ImGui::Checkbox("auto fire", &g_cfg->rage_bot.m_auto_fire);
            ImGui::Checkbox("auto stop", &g_cfg->rage_bot.m_auto_stop);
        }
        ImGui::EndChild();
    }

    if (m_selected_tab == 1) {
        ImGui::BeginChild("anti-hit", ImVec2{ 0, 0 }, true);
        {
            ImGui::Checkbox("enabled", &g_cfg->anti_hit.m_enabled);
            ImGui::Checkbox("override pitch", &g_cfg->anti_hit.m_override_pitch);
            if (g_cfg->anti_hit.m_override_pitch)
                ImGui::SliderInt("##override_pitch", &g_cfg->anti_hit.m_pitch_amount, -89, 89);
            ImGui::Checkbox("override yaw", &g_cfg->anti_hit.m_override_yaw);
            if (g_cfg->anti_hit.m_override_yaw) {
                ImGui::Checkbox("at target", &g_cfg->anti_hit.m_at_target);
                ImGui::SliderInt("##override_yaw", &g_cfg->anti_hit.m_yaw_amount, 0, 180);
                ImGui::SliderInt("jiter amount", &g_cfg->anti_hit.m_jitter_amount, 0, 180);
            }
        }
        ImGui::EndChild();
    }

    if (m_selected_tab == 2) {
        ImGui::BeginChild("world esp ", ImVec2{ 0, 0 }, true);
        {
            ImGui::Checkbox("enabled thirdperson", &g_cfg->world_esp.m_enable_thirdperson);
            ImGui::SliderInt("override distance", &g_cfg->world_esp.m_distance, 0, 180);

            ImGui::Keybind("thirdperson keybind", &g_cfg->world_esp.m_thirdperson_key_bind, &g_cfg->world_esp.m_thirdperson_key_bind_style);

            ImGui::Checkbox("render fog", &g_cfg->world.m_render_fog);
            ImGui::ColorEdit4("sky color", reinterpret_cast<float*>(&g_cfg->world.m_sky), ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("sky and clouds color", reinterpret_cast<float*>(&g_cfg->world.m_sky_clouds), ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("wall color", reinterpret_cast<float*>(&g_cfg->world.m_wall), ImGuiColorEditFlags_NoInputs);
            ImGui::ColorEdit4("lighting color", reinterpret_cast<float*>(&g_cfg->world.m_lighting), ImGuiColorEditFlags_NoInputs);
            ImGui::SliderInt("exposure ", reinterpret_cast<int*>(&g_cfg->world.m_exposure), 1, 100);
        }
        ImGui::EndChild();
    }

    if (m_selected_tab == 3) {
        ImGui::BeginChild("player esp ", ImVec2{ 0, 0 }, true);
        {
            ImGui::Checkbox("teammates", &g_cfg->visuals.m_player_esp.m_teammates);
            ImGui::Checkbox("bounding box", &g_cfg->visuals.m_player_esp.m_bounding_box);
            ImGui::Checkbox("health bar", &g_cfg->visuals.m_player_esp.m_health_bar);
            ImGui::Checkbox("name", &g_cfg->visuals.m_player_esp.m_name);
            ImGui::Checkbox("weapon", &g_cfg->visuals.m_player_esp.m_weapon);

            ImGui::ColorEdit4("name color", reinterpret_cast<float*>(&g_cfg->visuals.m_player_esp.m_name_color), ImGuiColorEditFlags_NoInputs);
        }
        ImGui::EndChild();
    }

    if (m_selected_tab == 4) {
        ImGui::BeginChild("misc", ImVec2{ 165, 0 }, true);
        {
            ImGui::Checkbox("Override fov", &g_cfg->misc.m_custom_fov);
            if (g_cfg->misc.m_custom_fov) {
                ImGui::SliderFloat("Fov", &g_cfg->misc.m_fov, 0.f, 180.f);
			}

            ImGui::Checkbox("Custom view model", &g_cfg->misc.m_custom_viewmodel);
            if (g_cfg->misc.m_custom_viewmodel)
            {
                ImGui::SliderFloat("Viewmodel X", &g_cfg->misc.m_viewmodel_x, -100.f, 100.f);
                ImGui::SliderFloat("Viewmodel Y", &g_cfg->misc.m_viewmodel_y, -100.f, 100.f);
                ImGui::SliderFloat("Viewmodel Z", &g_cfg->misc.m_viewmodel_z, -100.f, 100.f);
                ImGui::SliderFloat("Viewmodel fov", &g_cfg->misc.m_viewmodel_fov, -100.f, 100.f);
            }

            ImGui::Checkbox("Switch hands on knife", &g_cfg->misc.m_switch_hands_on_knife);

            ImGui::Checkbox("Ragdoll effect", &g_cfg->misc.set_ragedoll_effect);
            if (g_cfg->misc.set_ragedoll_effect) {
                ImGui::SliderFloat("Ragdoll effect duration", &g_cfg->misc.m_ragedoll_effect_duration, 0.1f, 10.f);
            }
        }
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild("misc_movement", ImVec2{ 165, 0 }, true);
        {
            static constexpr const char* types[]{ "local view", "directional" };

            ImGui::Checkbox("bunny hop", &g_cfg->misc.m_bunny_hop);
            ImGui::Checkbox("auto strafe", &g_cfg->misc.m_auto_strafe);
            if (g_cfg->misc.m_auto_strafe) {
                ImGui::Text("auto strafer smooth");
                ImGui::SliderInt("##auto_strafer_smooth", &g_cfg->misc.m_strafe_smooth, 1, 99);
            }
        }
        ImGui::EndChild();
    }

    if (m_selected_tab == 5) {
        ImGui::BeginChild("misc", ImVec2{ 0, 0 }, true);
        {
            static constexpr const char* type[]{ "weapons", "knives", "gloves", "agents" };
            ImGui::Combo("Types", &g_cfg->skins.m_selected_type, type, IM_ARRAYSIZE(type));

            if (g_cfg->skins.m_selected_type == 0) {
                static std::vector<const char*> weapon_names{};

                if (weapon_names.size() < g_skins->m_dumped_items.size())
                    for (auto& item : g_skins->m_dumped_items)
                        weapon_names.emplace_back(item.m_name);

                ImGui::Combo("Weapons", &g_cfg->skins.m_selected_weapon, weapon_names.data(), weapon_names.size());
                auto& selected_entry = g_cfg->skins.m_skin_settings[g_cfg->skins.m_selected_weapon];

                if (ImGui::BeginListBox("##skins"))
                {
                    auto& selected_weapon_entry = g_skins->m_dumped_items[g_cfg->skins.m_selected_weapon];

                    for (auto& skin : selected_weapon_entry.m_dumped_skins)
                    {
                        ImGui::PushID(&skin);
                        if (ImGui::Selectable(skin.m_name, selected_entry.m_paint_kit == skin.m_id))
                        {
                            if (selected_weapon_entry.m_selected_skin == &skin)
                                selected_weapon_entry.m_selected_skin = nullptr;
                            else
                            {
                                selected_weapon_entry.m_selected_skin = &skin;
                                selected_entry.m_paint_kit = skin.m_id;
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::EndListBox();
                }
            }

            if (g_cfg->skins.m_selected_type == 1)
                ImGui::Combo("##knifes", &g_cfg->skins.m_knives.m_selected, g_skins->m_knives.m_dumped_knife_name.data(), g_skins->m_knives.m_dumped_knife_name.size());

            if (g_cfg->skins.m_selected_type == 2)
                ImGui::Combo("##gloves", &g_cfg->skins.m_gloves.m_selected, g_skins->m_gloves.m_dumped_glove_name.data(), g_skins->m_gloves.m_dumped_glove_name.size());

            if (g_cfg->skins.m_selected_type == 3) {
                ImGui::Combo("##agents ct", &g_cfg->skins.m_agents.m_selected_ct, g_skins->m_agents.m_dumped_agent_name.data(), g_skins->m_agents.m_dumped_agent_name.size());
                ImGui::Combo("##agents t", &g_cfg->skins.m_agents.m_selected, g_skins->m_agents.m_dumped_agent_name.data(), g_skins->m_agents.m_dumped_agent_name.size());
            }
        }
        ImGui::EndChild();
    }

    if (m_selected_tab == 6) {
        g_config_system->menu();
    }

    ImGui::End();
}

namespace
{
    // Compile‑time RGBA literals
    constexpr ImU32 RGBA(int r, int g, int b, int a = 255)
    {
        return IM_COL32(r, g, b, a);
    }

    // Single‑pixel outline for readable text
    void TextShadow(ImDrawList* dl, ImVec2 p, ImU32 col_shadow, std::string_view txt)
    {
        dl->AddText({ p.x + 1, p.y + 1 }, col_shadow, txt.data());
        dl->AddText(p,                             // real text is drawn by caller
            0,                             // colour ignored here
            "");
    }

    // Convenience for measuring *static* (ASCII) strings
    ImVec2 Measure(std::string_view txt)
    {
        return ImGui::CalcTextSize(txt.data());
    }
}

struct WatermarkItem
{
    std::string_view id;                       // e.g. "Logo", "FPS" (for toggling)
    std::function<ImVec2()> size;              // returns (w,h) or (0,0) when hidden
    std::function<void(ImVec2)> draw;          // draw at absolute position
};

void c_menu::watermark()
{
     static bool show_logo = true;
    static bool show_fps = true;
    /* Add further booleans here to mimic the Lua multiselect if needed */

    constexpr float margin_outside = 10.f;     // distance from viewport
    constexpr float item_spacing = 8.f;      // gap between items
    constexpr float header_h = 3.f;      // small gradient bar on top
    constexpr ImU32 col_text = RGBA(255, 255, 255, 230);
    constexpr ImU32 col_shadow = RGBA(0, 0, 0, 200);
    constexpr ImU32 col_sep = RGBA(210, 210, 210, 255);

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImGuiViewport* vp = ImGui::GetMainViewport();

    const float fps_f = ImGui::GetIO().Framerate;
    char        fps_buf[16]{};
    std::snprintf(fps_buf, sizeof(fps_buf), "%.0f fps", fps_f);

     const std::array<WatermarkItem, 2> items = {           // expand if needed
        WatermarkItem{
            /* Logo */
            "Logo",
            /* size */ [&]() -> ImVec2
            {
                return show_logo ? Measure("lambdahook") : ImVec2{};
            },
        /* draw */ [&](ImVec2 pos)
        {
            if (!show_logo) return;
            TextShadow(dl, pos, col_shadow, "lambdahook");
            dl->AddText(pos, col_text, "lambdahook");
        }
    },
    WatermarkItem{
            /* FPS */
            "FPS",
            /* size */ [&]() -> ImVec2
            {
                return show_fps ? Measure(fps_buf) : ImVec2{};
            },
        /* draw */ [&](ImVec2 pos)
        {
            if (!show_fps) return;
            TextShadow(dl, pos, col_shadow, fps_buf);
            dl->AddText(pos, col_text, fps_buf);
        }
    }
    };

    float total_w = 0.f;
    float max_h = 0.f;

    for (const auto& it : items)
    {
        ImVec2 sz = it.size();
        if (sz.x == 0.f) continue;             // hidden
        total_w += sz.x;
        max_h = max(max_h, sz.y);
        total_w += item_spacing;               // sep or last item, trimmed later
    }

    if (total_w == 0.f)
        return;                // nothing enabled

    total_w -= item_spacing;                   // remove last trailing spacing
    const ImVec2 box_sz{ total_w + 6.f, max_h + 6.f + header_h }; // padding

    const ImVec2 origin{
        vp->WorkPos.x + vp->WorkSize.x - box_sz.x - margin_outside,
        vp->WorkPos.y + margin_outside
    };

    const ImVec2 bg_min{ origin.x, origin.y };
    const ImVec2 bg_max{ origin.x + box_sz.x, origin.y + box_sz.y };

    dl->AddRectFilled(bg_min, bg_max, RGBA(18, 18, 18, 180), 3.f);         // main bg
    dl->AddRect(bg_min, bg_max, RGBA(62, 62, 62, 255), 3.f);               // border

    dl->AddRectFilledMultiColor(
        ImVec2(bg_min.x, bg_min.y),
        ImVec2(bg_max.x, bg_min.y + header_h),
        RGBA(59, 175, 222), RGBA(202, 70, 205),
        RGBA(202, 70, 205), RGBA(201, 227, 58)
    );

    float cursor = origin.x + 3.f;             
    const float baseline = origin.y + header_h + 3.f;  

    for (size_t i = 0; i < items.size(); ++i)
    {
        ImVec2 sz = items[i].size();
        if (sz.x == 0.f) continue;

        items[i].draw({ cursor, baseline });

        cursor += sz.x;

        bool is_last_visible = true;
        for (size_t j = i + 1; j < items.size(); ++j)
            if (items[j].size().x > 0.f) { is_last_visible = false; break; }

        if (!is_last_visible)
        {
            dl->AddRectFilled(
                { cursor + 4.f, baseline + 1.f },
                { cursor + 5.f, baseline + sz.y - 1.f },
                col_sep
            );
        }
        cursor += item_spacing;
    }
}


void c_menu::on_create_move()
{
    if (!m_opened)
        return;

    g_ctx->m_user_cmd->m_button_state.m_button_state &= ~(IN_ATTACK | IN_ATTACK2);
    g_ctx->m_user_cmd->m_button_state.m_button_state2 &= ~(IN_ATTACK | IN_ATTACK2);
    g_ctx->m_user_cmd->m_button_state.m_button_state3 &= ~(IN_ATTACK | IN_ATTACK2);
}
