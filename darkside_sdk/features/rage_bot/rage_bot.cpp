#include "rage_bot.hpp"
#include "../../entity_system/entity.hpp"
#include "../movement/movement.hpp"
#include "../../render/render.hpp"

//                            _ooOoo_
//                           o8888888o
//                           88" . "88
//                           (| -_- |)
//                            O\ = /O
//                        ____/`---'\____
//                      .   ' \\| |// `.
//                       / \\||| : |||// \
//                     / _||||| -:- |||||- \
//                       | | \\\ - /// | |
//                     | \_| ''\---/'' | |
//                      \ .-\__ `-` ___/-. /
//                   ___`. .' /--.--\ `. . __
//                ."" '< `.___\_<|>_/___.' >'"".
//               | | : `- \`.;`\ _ /`;.`/ - ` : | |
//                 \ \ `-. \_ __\ /__ _/ .-` / /
//         ======`-.____`-.___\_____/___.-`____.-'======
//                            `=---='
//
//         .............................................
//                  佛祖保佑             永无BUG


void c_rage_bot::store_records()
{
	/*-------------------------------------------------------------------------*/
	/*  PRE‑CONDITIONS                                                         */
	/*-------------------------------------------------------------------------*/
	if (!g_interfaces->m_engine->is_in_game() || !g_interfaces->m_engine->is_connected())
		return;

	if (!g_ctx->m_local_pawn)
		return;

	static auto sv_maxunlag = g_interfaces->m_var->get_by_name("sv_maxunlag");
	const int   max_ticks = TIME_TO_TICKS(sv_maxunlag->get_float());

	/*-------------------------------------------------------------------------*/
	/*  ITERATE EVERY CONTROLLER IN THE GAME                                   */
	/*-------------------------------------------------------------------------*/
	const auto& controllers = g_entity_system->get("CCSPlayerController");

	for (auto entity : controllers)
	{
		auto controller = reinterpret_cast<c_cs_player_controller*>(entity);
		if (!controller || controller == g_ctx->m_local_controller)
			continue;

		const int handle = controller->get_handle().to_int();

		/*---------------------------------------------------------------------*/
		/*  CLEAN‑UP FOR DEAD / INVALID PLAYERS                                 */
		/*---------------------------------------------------------------------*/
		if (!controller->m_pawn_is_alive())
		{
			m_lag_records.erase(handle);
			continue;
		}

		auto pawn = reinterpret_cast<c_cs_player_pawn*>(
			g_interfaces->m_entity_system->get_base_entity(
				controller->m_pawn().get_entry_index()));

		if (!pawn)
		{
			m_lag_records.erase(handle);
			continue;
		}

		/*  Ignore team‑mates and the local player itself                       */
		if (pawn == g_ctx->m_local_pawn ||
			pawn->m_team_num() == g_ctx->m_local_pawn->m_team_num())
			continue;

		/*---------------------------------------------------------------------*/
		/*  FETCH / CREATE THE PLAYER‑SPECIFIC CIRCULAR BUFFER                  */
		/*---------------------------------------------------------------------*/
		auto& buffer = m_lag_records
			.try_emplace(handle, util::circular_buffer<lag_record_t>(max_ticks))
			.first->second;

		/*  Make sure capacity matches *sv_maxunlag* (server may change it)     */
		if (buffer.max_size != static_cast<std::size_t>(max_ticks))
			buffer.reserve(max_ticks);           // discards old data by design

		/*---------------------------------------------------------------------*/
		/*  INSERT THE NEWEST TICK, DISCARD OLDEST IF NECESSARY                 */
		/*---------------------------------------------------------------------*/
		if (buffer.exhausted())
			buffer.pop_back();                   // make room for the new sample

		if (auto* rec = buffer.push_front(); rec)    // cannot fail after pop
			*rec = lag_record_t{ pawn };             // store() called via ctor
	}
}

void c_rage_bot::store_hitboxes() {
	if (g_cfg->rage_bot.m_hitboxes[0])
		m_hitboxes.emplace_back(HITBOX_HEAD);

	if (g_cfg->rage_bot.m_hitboxes[1]) {
		m_hitboxes.emplace_back(HITBOX_CHEST);
		m_hitboxes.emplace_back(HITBOX_LOWER_CHEST);
		m_hitboxes.emplace_back(HITBOX_UPPER_CHEST);
	}

	if (g_cfg->rage_bot.m_hitboxes[2]) {
		m_hitboxes.emplace_back(HITBOX_PELVIS);
		m_hitboxes.emplace_back(HITBOX_STOMACH);
	}

	if (g_cfg->rage_bot.m_hitboxes[3]) {
		m_hitboxes.emplace_back(HITBOX_RIGHT_HAND);
		m_hitboxes.emplace_back(HITBOX_LEFT_HAND);
		m_hitboxes.emplace_back(HITBOX_RIGHT_UPPER_ARM);
		m_hitboxes.emplace_back(HITBOX_RIGHT_FOREARM);
		m_hitboxes.emplace_back(HITBOX_LEFT_UPPER_ARM);
		m_hitboxes.emplace_back(HITBOX_LEFT_FOREARM);
	}

	if (g_cfg->rage_bot.m_hitboxes[4]) {
		m_hitboxes.emplace_back(HITBOX_RIGHT_THIGH);
		m_hitboxes.emplace_back(HITBOX_LEFT_THIGH);

		m_hitboxes.emplace_back(HITBOX_RIGHT_CALF);
		m_hitboxes.emplace_back(HITBOX_LEFT_CALF);
	}

	if (g_cfg->rage_bot.m_hitboxes[5]) {
		m_hitboxes.emplace_back(HITBOX_RIGHT_FOOT);
		m_hitboxes.emplace_back(HITBOX_LEFT_FOOT);
	}
}

lag_record_t* c_rage_bot::select_record(int handle)
{
	/*---------------------------------------------------------------------*/
	/*  Sanity checks                                                       */
	/*---------------------------------------------------------------------*/
	if (!g_ctx->m_local_pawn || !g_ctx->m_local_pawn->is_alive())
		return nullptr;

	auto it_player = m_lag_records.find(handle);
	if (it_player == m_lag_records.end())
		return nullptr;

	auto& records = it_player->second;
	if (records.empty())
		return nullptr;

	/*---------------------------------------------------------------------*/
	/*  Fast‑path: only one record                                          */
	/*---------------------------------------------------------------------*/
	if (records.size() == 1)
		return &records.front();

	/*---------------------------------------------------------------------*/
	/*  Scan the circular buffer from newest → oldest                       */
	/*---------------------------------------------------------------------*/
	lag_record_t* best_record = nullptr;

	for (auto it = records.begin(); it != records.end(); ++it)   // <‑‑ FIX: ++it instead of std::next(it)
	{
		lag_record_t* record = &*it;

		if (!record->m_pawn || !record->is_valid())
			continue;

		/*  First valid sample becomes baseline                               */
		if (!best_record)
		{
			best_record = record;
			continue;
		}

		/*  Prefer grenade‑throwing animations                                */
		if (record->m_throwing)
		{
			best_record = record;
			continue;
		}

		/*  Prefer on‑ground state if current best is in air                  */
		const bool cur_on_ground = (record->m_pawn->m_flags() & FL_ONGROUND);
		const bool best_on_ground = (best_record->m_pawn->m_flags() & FL_ONGROUND);

		if (cur_on_ground && !best_on_ground)
			best_record = record;
	}

	return best_record;
}


void c_rage_bot::find_targets() {
	if (!g_ctx->m_local_pawn->is_alive())
		return;

	auto entities = g_entity_system->get("CCSPlayerController");

	for (auto entity : entities) {
		if (!entity)
			continue;

		auto player_controller = reinterpret_cast<c_cs_player_controller*>(entity);
		if (!player_controller || player_controller == g_ctx->m_local_controller)
			continue;

		int handle = player_controller->get_handle().to_int();

		lag_record_t* best_record = this->select_record(handle);

		if (!best_record)
			continue;

		auto target_ptr = m_aim_targets.find(handle);

		if (target_ptr == m_aim_targets.end()) {
			m_aim_targets.insert_or_assign(handle, aim_target_t{});

			continue;
		}

		target_ptr->second = aim_target_t(best_record);
	}
}

aim_target_t* c_rage_bot::get_nearest_target() {
	if (!g_ctx->m_local_pawn->is_alive())
		return nullptr;

	aim_target_t* best_target{};

	auto local_data = g_prediction->get_local_data();

	if (!local_data)
		return nullptr;

	vec3_t shoot_pos = local_data->m_eye_pos;

	float best_distance = FLT_MAX;

	for (auto it = m_aim_targets.begin(); it != m_aim_targets.end(); it = std::next(it)) {
		aim_target_t* target = &it->second;

		if (!target->m_lag_record
			|| (uintptr_t)target->m_lag_record->m_pawn == 0xdddddddddddddddd
			|| !target->m_lag_record->m_pawn
			|| !target->m_lag_record->m_pawn->is_player_pawn()
			|| !target->m_pawn->is_alive()
			|| !target->m_lag_record->m_pawn->m_scene_node()) {

			continue;
		}

		float distance = target->m_lag_record->m_pawn->m_scene_node()->m_origin().dist(shoot_pos);

		if (distance < best_distance) {
			best_distance = distance;
			best_target = target;
		}
	}

	return best_target;
}

vec3_t c_rage_bot::extrapolate(c_cs_player_pawn* pawn, vec3_t pos) {
	vec3_t velocity = pawn->m_vec_velocity();

	return pos + velocity * INTERVAL_PER_TICK;
}

float c_rage_bot::calc_point_scale(vec3_t& point, const float& hitbox_radius) {
	auto local_data = g_prediction->get_local_data();

	if (!local_data)
		return 0.f;

	float spread = local_data->m_spread + local_data->m_inaccuracy;
	float distance = point.dist(local_data->m_eye_pos);

	float new_distance = distance / std::sin(deg2rad(90.f - rad2deg(spread)));
	float scale = ((hitbox_radius - new_distance * spread) + 0.1f) * 100.f;

	return std::clamp(scale, 0.f, 95.f);
}

vec3_t c_rage_bot::transform_point(matrix3x4_t matrix, vec3_t point)
{
	vec3_t result{ };
	g_math->vector_transform(point, matrix, result);
	return result;
}

std::vector<vec3_t> c_rage_bot::calculate_sphere_points(float radius, int num_points)
{
	std::vector<vec3_t> points;
	points.reserve(num_points);

	const float phi = _pi * (3.0f - std::sqrt(5.0f));

	for (int i = 0; i < num_points; ++i)
	{
		float y = 1 - (i / float(num_points - 1)) * 2;
		float radius_at_y = std::sqrt(1 - y * y);

		float theta = phi * i;

		float x = std::cos(theta) * radius_at_y;
		float z = std::sin(theta) * radius_at_y;

		vec3_t vec_point{};
		points.push_back({ x * radius, y * radius, z * radius });
	}

	return points;
}

std::vector<vec3_t> c_rage_bot::calculate_points(int num_points, float radius, float height, matrix3x4_t matrix, hitbox_data_t hitbox)
{
	std::vector<vec3_t> points;
	points.reserve(num_points);

	for (int i = 0; i < num_points; ++i)
	{
		float theta = 2.f * _pi * i / num_points;
		float y = radius * std::cos(theta);
		float z = radius * std::sin(theta);

		vec3_t in_point = { hitbox.m_mins.x + hitbox.m_radius * height, y, z };

		vec3_t vec_point{};
		g_math->vector_transform(in_point, matrix, vec_point);
		points.push_back(vec_point);
	}

	return points;
}

bool c_rage_bot::multi_points(lag_record_t* record, int hitbox, std::vector<aim_point_t>& points) {
	if (!record)
		return false;

	c_cs_player_pawn* pawn = record->m_pawn;

	if (!pawn || !pawn->is_player_pawn() || !pawn->is_alive())
		return false;

	hitbox_data_t hitbox_data = get_hitbox_data(pawn, hitbox);

	if (hitbox_data.m_invalid_data)
		return false;

	auto local_data = g_prediction->get_local_data();

	if (!local_data)
		return false;

	vec3_t center_local = (hitbox_data.m_mins + hitbox_data.m_maxs) * 0.5f;

	matrix3x4_t matrix = g_math->transform_to_matrix(record->m_bone_data[hitbox_data.m_num_bone]);

	vec3_t center = transform_point(matrix, center_local);

	int hitbox_from_menu = this->get_hitbox_from_menu(hitbox);

	if (hitbox_from_menu == -1)
		return false;

	points.emplace_back(center, hitbox, true);

	if (!g_cfg->rage_bot.m_multi_points[hitbox_from_menu])
		return true;

	float scale = g_cfg->rage_bot.m_hitbox_scale[hitbox_from_menu];

	if (!g_cfg->rage_bot.m_static_scale)
		scale = calc_point_scale(center, hitbox_data.m_radius);

	float radius = hitbox_data.m_radius * (scale / 100.f);

	if (radius <= 0.f)
		return false;

	if (hitbox == HITBOX_HEAD) {
		auto sphere_points = calculate_sphere_points(radius, 15);

		if (sphere_points.empty())
			return false;

		for (const auto& point : sphere_points) {
			auto point_pos = transform_point(matrix, center_local + point);
			points.emplace_back(point_pos, hitbox);
		}

		return true;
	}

	vec3_t start = transform_point(matrix, hitbox_data.m_mins);
	vec3_t end = transform_point(matrix, hitbox_data.m_maxs);
	vec3_t dir = (end - start).normalize();
	float length = start.dist(end);
	vec3_t capsule_center = start + dir * (length * 0.5f);

	vec3_t arbitrary = (std::abs(dir.z) < 0.9f) ? vec3_t(0.f, 0.f, 1.f) : vec3_t(1.f, 0.f, 0.f);
	vec3_t right = dir.cross(arbitrary).normalize();
	vec3_t up = dir.cross(right).normalize();

	int num_points = 4;
	for (int i = 0; i < num_points; ++i) {
		float theta = 2.f * _pi * static_cast<float>(i) / num_points;
		vec3_t offset = right * (radius * std::cos(theta)) + up * (radius * std::sin(theta));
		vec3_t point_pos = capsule_center + offset;
		points.emplace_back(point_pos, hitbox);
	}

	return true;
}

aim_point_t c_rage_bot::select_points(lag_record_t* record, float& damage)
{
	// initial‑state defaults
	damage = 0.f;
	aim_point_t best_point{ vec3_t(0.f, 0.f, 0.f), -1 /* invalid hitbox */ };

	// sanity checks – never assume any pointer is valid
	if (!record || !record->m_pawn)
		return best_point;

	if (!record->m_pawn->is_player_pawn() || !record->m_pawn->is_alive())
		return best_point;

	c_cs_player_pawn* local_pawn = g_ctx->m_local_pawn;
	if (!local_pawn || !local_pawn->is_alive())
		return best_point;

	c_base_player_weapon* weapon = local_pawn->get_active_weapon();
	if (!weapon)
		return best_point;

	c_cs_weapon_base_v_data* weapon_data = weapon->get_weapon_data();
	if (!weapon_data)
		return best_point;

	auto local_data = g_prediction->get_local_data();
	if (!local_data)
		return best_point;

	const vec3_t shoot_pos = local_data->m_eye_pos;
	const bool   is_taser = weapon_data->m_weapon_type() == WEAPONTYPE_TASER;

	// iterate every requested hitbox → generate points → evaluate penetration
	float best_damage = -FLT_MAX;                  // allows 0‑damage points
	bool  best_is_center = false;                  // tiebreaker: prefer centre

	for (int hitbox_id : m_hitboxes)
	{
		std::vector<aim_point_t> point_list;
		point_list.reserve(64);

		if (!multi_points(record, hitbox_id, point_list))
			continue;                              // could not generate points

		for (const aim_point_t& pt : point_list)
		{
			penetration_data_t pen{};
			if (!g_auto_wall->fire_bullet(shoot_pos,
				pt.m_point,
				record->m_pawn,
				weapon_data,
				pen,
				is_taser))
				continue;                          // trace failed / no damage

			const float dmg = pen.m_damage;

			// ------------------------------------------------------------------
			// selection rule:
			// 1) strictly highest damage wins
			// 2) if damage is equal (rare), prefer a centre point for consistency
			// ------------------------------------------------------------------
			const bool is_better =
				(dmg > best_damage) ||
				(dmg == best_damage && pt.m_center && !best_is_center);

			if (is_better)
			{
				best_point = pt;
				best_damage = dmg;
				best_is_center = pt.m_center;

				if (dmg >= record->m_pawn->m_health())
					goto done;
			}
		}
	}

done:
	damage = max(0.f, best_damage);
	return best_point;
}

void c_rage_bot::select_target()
{
	if (!g_ctx->m_local_pawn || !g_ctx->m_local_pawn->is_alive())
		return;

	if (m_aim_targets.empty())
		return;

	m_best_target = nullptr;            

	auto  active_weapon = g_ctx->m_local_pawn->get_active_weapon();
	if (!active_weapon)
		return;

	auto  weapon_data = active_weapon->get_weapon_data();
	if (!weapon_data)
		return;

	const bool is_taser = weapon_data->m_weapon_type() == WEAPONTYPE_TASER;

	float best_damage_global = 0.f;

	for (auto& it : m_aim_targets)
	{
		aim_target_t* target = &it.second;

		if (!target->m_lag_record || !target->m_pawn || !target->m_pawn->is_alive())
			continue;

		target->m_best_point.reset();   

		int min_damage = min(g_cfg->rage_bot.m_minimum_damage,
			target->m_pawn->m_health());

		if (g_key_handler->is_pressed(g_cfg->rage_bot.m_override_damage_key_bind,
			g_cfg->rage_bot.m_override_damage_key_bind_style))
		{
			min_damage = min(g_cfg->rage_bot.m_minimum_damage_override,
				target->m_pawn->m_health());
		}

		if (is_taser)
			min_damage = target->m_pawn->m_health(); 

		target->m_lag_record->apply(target->m_pawn);     

		float   damage = 0.f;
		aim_point_t best = select_points(target->m_lag_record, damage);

		target->m_lag_record->reset(target->m_pawn);

		if (best.m_hitbox == -1 || damage < min_damage)
			continue;

		target->m_best_point = std::make_unique<aim_point_t>(best);

		if (damage > best_damage_global)
		{
			best_damage_global = damage;
			m_best_target = target;
		}
	}
}

bool c_rage_bot::weapon_is_at_max_accuracy(c_cs_weapon_base_v_data* weapon_data, float inaccuracy) {
	auto local_data = g_prediction->get_local_data();

	if (!local_data)
		return false;

	constexpr auto round_accuracy = [](float accuracy) { return floorf(accuracy * 170.f) / 170.f; };
	constexpr auto round_duck_accuracy = [](float accuracy) { return floorf(accuracy * 300.f) / 300.f; };

	float speed = g_ctx->m_local_pawn->m_vec_abs_velocity().length();

	bool is_scoped = ((weapon_data->m_weapon_type() == WEAPONTYPE_SNIPER_RIFLE)
		&& !(g_ctx->m_user_cmd->m_button_state.m_button_state & IN_ZOOM)
		&& !g_ctx->m_local_pawn->m_scoped());

	bool is_ducking = ((g_ctx->m_local_pawn->m_flags() & FL_DUCKING) || (g_ctx->m_user_cmd->m_button_state.m_button_state & IN_DUCK));

	float rounded_accuracy = round_accuracy(inaccuracy);
	float rounded_duck_accuracy = round_duck_accuracy(inaccuracy);

	if (is_ducking && is_scoped && (g_ctx->m_local_pawn->m_flags() & FL_ONGROUND) && (rounded_duck_accuracy < local_data->m_inaccuracy))
		return true;

	if (speed <= 0 && is_scoped && (g_ctx->m_local_pawn->m_flags() & FL_ONGROUND) && (rounded_accuracy < local_data->m_inaccuracy))
		return true;

	return false;
}

vec3_t c_rage_bot::calculate_spread_angles(vec3_t angle, int random_seed, float weapon_inaccuarcy, float weapon_spread)
{
	float r1, r2, r3, r4, s1, c1, s2, c2;

	g_interfaces->m_random_seed(random_seed + 1);

	r1 = g_interfaces->m_random_float(0.f, 1.f);
	r2 = g_interfaces->m_random_float(0.f, _pi2);
	r3 = g_interfaces->m_random_float(0.f, 1.f);
	r4 = g_interfaces->m_random_float(0.f, _pi2);

	c1 = std::cos(r2);
	c2 = std::cos(r4);
	s1 = std::sin(r2);
	s2 = std::sin(r4);

	vec3_t spread = {
		(c1 * (r1 * weapon_inaccuarcy)) + (c2 * (r3 * weapon_spread)),
		(s1 * (r1 * weapon_inaccuarcy)) + (s2 * (r3 * weapon_spread))
	};

	vec3_t vec_forward, vec_right, vec_up;
	g_math->angle_vectors(angle, vec_forward, vec_right, vec_up);

	return (vec_forward + (vec_right * spread.x) + (vec_up * spread.y)).normalize();
}

int c_rage_bot::calculate_hit_chance(c_cs_player_pawn* pawn, vec3_t angles, c_base_player_weapon* active_weapon, c_cs_weapon_base_v_data* weapon_data, bool no_spread) {
	if (!g_ctx->m_local_pawn->is_alive())
		return 0;

	if (!pawn)
		return 0;

	auto local_data = g_prediction->get_local_data();

	if (!local_data)
		return 0;

	if (no_spread)
		return 100;

	vec3_t shoot_pos = local_data->m_eye_pos;

	if (shoot_pos.dist(angles) > weapon_data->m_range())
		return 0;

	active_weapon->update_accuracy_penality();

	if (weapon_is_at_max_accuracy(weapon_data, active_weapon->get_inaccuracy()))
		return 100;

	const float spread = local_data->m_spread;
	const float inaccuracy = local_data->m_inaccuracy;

	int hits = 0;

	for (int seed = 0; seed < 570; seed++) {
		vec3_t spread_angle = calculate_spread_angles(angles, seed, inaccuracy, spread);

		vec3_t result = spread_angle * weapon_data->m_range() + shoot_pos;

		ray_t ray{};
		game_trace_t trace{};
		trace_filter_t filter{};
		g_interfaces->m_trace->init_trace(filter, g_ctx->m_local_pawn, MASK_SHOT, 0x3, 0x7);

		g_interfaces->m_trace->trace_shape(&ray, shoot_pos, result, &filter, &trace);
		g_interfaces->m_trace->clip_ray_entity(&ray, shoot_pos, result, pawn, &filter, &trace);

		if (trace.m_hit_entity && trace.m_hit_entity->is_player_pawn() && trace.m_hit_entity->get_handle().get_entry_index() == pawn->get_handle().get_entry_index())
			hits++;
	}

	const auto m_last_hit_chance = static_cast<int>((static_cast<float>(hits / 570.f)) * 100.f);
	return m_last_hit_chance;
}

// note: unfinished
no_spread_result c_rage_bot::get_no_spread(vec3_t& angle)
{
	no_spread_result result{};

	auto* pawn = g_ctx->m_local_pawn;
	if (!pawn || !pawn->is_alive())
		return result;

	auto* weapon = pawn->get_active_weapon();
	if (!weapon)
		return result;

	auto* local_data = g_prediction->get_local_data();
	if (!local_data)
		return result;

	//  ALWAYS bring the weapon state up‑to‑date first
	weapon->update_accuracy_penality();      // <‑‑ mandatory!

	// 	get the *real* values that the server will use
	const float inaccuracy = local_data->m_inaccuracy;   // already jump‑aware
	const float spread = local_data->m_spread;

	const std::uint32_t tick = g_ctx->m_local_controller->m_tick_base();
	const std::uint32_t seed0 = g_no_spread->compute_random_seed(&angle, tick);

	// quick check: maybe this seed already gives zero spread 
	if (g_no_spread->calculate_spread(seed0 + 1, inaccuracy, spread).x == 0.f &&
		g_no_spread->calculate_spread(seed0 + 1, inaccuracy, spread).y == 0.f)
	{
		result = { true, angle, 0, static_cast<int>(seed0) };
		return result;
	}

	//  brute‑force the self‑consistent solution
	for (int i = 0; i < 720; ++i)                                 // 0 … 359.5°
	{
		vec3_t tmp = { angle.x + i * 0.5f, angle.y, 0.f };

		const std::uint32_t seed = g_no_spread->compute_random_seed(&tmp, tick);
		const auto vec = g_no_spread->calculate_spread(seed + 1, inaccuracy, spread);

		/* convert (spread.x, spread.y) to <pitch, roll> compensation */
		vec3_t adj = angle;
		adj.x += rad2deg(std::atan(std::sqrt(vec.x * vec.x + vec.y * vec.y))); // pitch down
		adj.z = -rad2deg(std::atan2(vec.x, vec.y));                            // roll

		if (g_no_spread->compute_random_seed(&adj, tick) == seed)
		{
			result = { true, adj, i, static_cast<int>(seed) };
			break;
		}
	}
	return result;
}


void c_rage_bot::auto_pistol()
{
	auto weapon = g_ctx->m_local_pawn->get_active_weapon();

	if (weapon->get_weapon_data()->m_weapon_type() != WEAPONTYPE_PISTOL || weapon->m_clip1() <= 0)
		return;

	static bool is_fire = false;

	if (g_ctx->m_user_cmd->m_button_state.m_button_state & IN_ATTACK) {
		if (is_fire) {

			g_ctx->m_user_cmd->m_button_state.m_button_state &= ~IN_ATTACK;
		}
	}

	is_fire = g_ctx->m_user_cmd->m_button_state.m_button_state & IN_ATTACK;
}

void c_rage_bot::process_backtrack(lag_record_t* record) {
	if (!record)
		return;

	const int tick_shoot = TIME_TO_TICKS(record->m_simulation_time) + 2;

	for (int i = 0; i < g_ctx->m_user_cmd->pb.input_history_size(); i++) {

		if (g_ctx->m_user_cmd->pb.attack1_start_history_index() == -1)
			continue;

		auto input_history = g_ctx->m_user_cmd->pb.mutable_input_history(i);
		if (!input_history)
			continue;

		if (input_history->has_cl_interp()) {
			auto interp_info = input_history->mutable_cl_interp();
			interp_info->set_frac(0.f);
		}

		if (input_history->has_sv_interp0()) {
			auto interp_info = input_history->mutable_sv_interp0();
			interp_info->set_src_tick(tick_shoot);
			interp_info->set_dst_tick(tick_shoot);
			interp_info->set_frac(0.f);
		}

		if (input_history->has_sv_interp1()) {
			auto interp_info = input_history->mutable_sv_interp1();
			interp_info->set_src_tick(tick_shoot);
			interp_info->set_dst_tick(tick_shoot);
			interp_info->set_frac(0.f);
		}

		input_history->set_render_tick_count(tick_shoot);

		g_ctx->m_user_cmd->pb.mutable_base()->set_client_tick(tick_shoot);

		input_history->set_player_tick_count(g_ctx->m_local_controller->m_tick_base());
		input_history->set_player_tick_fraction(g_prediction->get_local_data()->m_player_tick_fraction);
	}
}

bool c_rage_bot::can_shoot(c_cs_player_pawn* pawn, c_base_player_weapon* active_weapon) {
	return (active_weapon->m_clip1() > 0) && (active_weapon->m_next_primary_attack() <= g_ctx->m_local_controller->m_tick_base());
}

static int find_entry_for_tick(const CSGOUserCmdPB& pb, int tick)
{
	for (int i = pb.input_history_size() - 1; i >= 0; --i)
		if (pb.input_history(i).player_tick_count() == tick)
			return i;
	return 0;           // fallback – should not happen
}

void c_rage_bot::process_attack(c_user_cmd* cmd, const vec3_t& desired_angles)
{
{
		char buf[128];
		std::snprintf(buf, sizeof(buf),
			"[RB] process_attack → angles = (%.2f, %.2f, %.2f)",
			desired_angles.x, desired_angles.y, desired_angles.z);
		LOG_INFO(buf);
	}

	if (auto* base = cmd->pb.mutable_base())
		if (auto* va = base->mutable_viewangles())
		{
			va->set_x(desired_angles.x);
			va->set_y(desired_angles.y);
			va->set_z(desired_angles.z);
		}

	const int   shootTick = g_prediction->get_local_data()->m_shoot_tick;
	const float tickFrac = g_prediction->get_local_data()->m_player_tick_fraction;

	{
		char buf[128];
		std::snprintf(buf, sizeof(buf),
			"[RB] shootTick = %d    tickFrac = %.3f    history = %d",
			shootTick, tickFrac, cmd->pb.input_history_size());
		LOG_INFO(buf);
	}

	// 1) Update only the entry that already matches shootTick.
	const int idx = find_entry_for_tick(cmd->pb, shootTick);
	auto* entry = cmd->pb.mutable_input_history(idx);

	for (int i = 0; i < cmd->pb.input_history_size(); ++i)
	{
		auto* entry = cmd->pb.mutable_input_history(i);
		if (!entry)
			continue;

		{
			char buf[128];
			std::snprintf(buf, sizeof(buf),
				"[RB]  ▸ BEFORE  id=%02d  pTick=%d rTick=%d",
				i, entry->player_tick_count(), entry->render_tick_count());
			LOG_INFO(buf);
		}

		entry->set_player_tick_count(shootTick);
		entry->set_render_tick_count(shootTick);
		entry->set_player_tick_fraction(tickFrac);
		entry->set_render_tick_fraction(tickFrac);

		{
			char buf[128];
			std::snprintf(buf, sizeof(buf),
				"[RB]  ▸ AFTER   id=%02d  pTick=%d rTick=%d",
				i, entry->player_tick_count(), entry->render_tick_count());
			LOG_INFO(buf);
		}
	}


	// 2) Point attack‑index at that entry.
	cmd->pb.set_attack1_start_history_index(idx);
	cmd->pb.set_attack2_start_history_index(-1);
	cmd->pb.set_attack3_start_history_index(-1);

	{
		char buf[96];
		std::snprintf(buf, sizeof(buf),
			"[RB] attackIdx  a1=%d  a2=%d  a3=%d",
			cmd->pb.attack1_start_history_index(),
			cmd->pb.attack2_start_history_index(),
			cmd->pb.attack3_start_history_index());
		LOG_INFO(buf);
	}


	// 3) Sub‑tick move step is still required.
	if (g_cfg->rage_bot.m_auto_fire)
	{
		auto* step = g_protobuf->add_subtick_move_step(cmd);
		if (step) {
			step->set_button(IN_ATTACK);
			step->set_pressed(true);
			step->set_when(tickFrac);

			char buf[128];
			std::snprintf(buf, sizeof(buf),
				"[RB] subtick step added  when=%.3f", tickFrac);
			LOG_INFO(buf);
		}
		else {
			LOG_INFO("[RB] ERROR: add_subtick_move_step → nullptr");
		}
		cmd->m_button_state.m_button_state |= IN_ATTACK;
	}

	process_backtrack(m_best_target->m_lag_record);
	LOG_INFO("[RB] backtrack processed");
}


void c_rage_bot::on_create_move()
{
	if (!g_cfg->rage_bot.m_enabled ||                       // feature disabled in menu
		!g_interfaces->m_engine->is_in_game())              // not in a live match
		return;

	/* clear‑per‑tick state */
	m_hitboxes.clear();
	if (m_best_target) m_best_target->reset();

	/* shorthands */
	c_user_cmd* user_cmd = g_ctx->m_user_cmd;
	c_cs_player_pawn* local_pawn = g_ctx->m_local_pawn;
	c_base_player_weapon* active_weapon = local_pawn ? local_pawn->get_active_weapon() : nullptr;
	c_cs_weapon_base_v_data* weapon_data =
		active_weapon ? active_weapon->get_weapon_data() : nullptr;

	/* more guards – bail if anything fundamental is missing */
	if (!user_cmd || !local_pawn || !local_pawn->is_alive() ||
		!active_weapon || !weapon_data ||
		weapon_data->m_weapon_type() == WEAPONTYPE_KNIFE ||
		weapon_data->m_weapon_type() == WEAPONTYPE_GRENADE)
		return;


	static auto set_random_seed = [](c_user_cmd* cmd, std::uint32_t seed)
		{
			cmd->random_seed = seed;                         // client side
			if (auto* base = cmd->pb.mutable_base())           // server copy
				base->set_random_seed(seed);
		};

	auto_pistol();
	store_hitboxes();
	if (m_hitboxes.empty()) 
		return;

	find_targets();
	select_target();
	if (!m_best_target || !m_best_target->m_best_point) 
		return;

	auto* local_data = g_prediction->get_local_data();
	if (!local_data || !can_shoot(local_pawn, active_weapon))
		return;

	const bool convar_no_spread =
		g_interfaces->m_var->get_by_name("weapon_accuracy_nospread")->get_bool();
	const bool want_no_spread = g_cfg->rage_bot.m_no_spread || convar_no_spread;

	vec3_t base_angle = g_math->aim_direction(
		local_data->m_eye_pos,
		m_best_target->m_best_point->m_point)
		- get_removed_aim_punch_angle(local_pawn);

	bool         has_no_spread = false;
	std::uint32_t solved_seed = 0;

	if (want_no_spread)
	{
		const no_spread_result ns = get_no_spread(base_angle);
		if (ns.bFound)
		{
			base_angle = ns.angAdjusted;    // compensated view
			solved_seed = ns.iSeed;          // seed that eliminates spread
			has_no_spread = true;
			set_random_seed(user_cmd, solved_seed);
		}
	}

	g_movement->auto_stop(user_cmd, local_pawn, active_weapon, has_no_spread);

	const bool is_taser = weapon_data->m_weapon_type() == WEAPONTYPE_TASER;
	const int  hitchance = calculate_hit_chance(
		m_best_target->m_pawn,
		base_angle,
		active_weapon,
		weapon_data,
		has_no_spread);

	/* required hit‑chance threshold */
	const int  needed_hc = is_taser ? 70 : g_cfg->rage_bot.m_hit_chance;
	if (hitchance < needed_hc)                // too risky → skip this tick
	{
		m_best_target->m_lag_record->reset(m_best_target->m_pawn);
		return;
	}

	for (int i = 0; i < user_cmd->pb.input_history_size(); ++i)
	{
		auto* tick = user_cmd->pb.mutable_input_history(i);
		if (!tick) 
			continue;

		tick->mutable_view_angles()->set_x(base_angle.x);
		tick->mutable_view_angles()->set_y(base_angle.y);
		tick->mutable_view_angles()->set_z(base_angle.z);
	}

	if (!g_cfg->rage_bot.m_silent)
		g_interfaces->m_csgo_input->set_view_angles(base_angle);

	process_attack(user_cmd, base_angle);
	m_best_target->m_lag_record->reset(m_best_target->m_pawn);
}
