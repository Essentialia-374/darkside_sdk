#include "c_game_movement.hpp"

void c_game_movement::air_accelerate(vec3_t& wish_vel, vec3_t& wish_angles, float friction, vec3_t& move, float frame_time, float max_speed)
{
	vec3_t forward{}, right{}, up{};
	g_math->angle_vectors(wish_angles, forward, right, up);

	if (forward.z) {
		forward.z = 0.f;
		forward.normalize_in_place();
	}

	if (right.z) {
		right.z = 0.f;
		right.normalize_in_place();
	}

	vec3_t dir{};
	dir.x = ((forward.x * move.x) * max_speed) + ((right.x * move.y) * max_speed);
	dir.y = ((forward.y * move.x) * max_speed) + ((right.y * move.y) * max_speed);
	dir.z = 0.f;

	vec3_t wish_dir = dir;
	float wish_speed = wish_dir.length();
	if (wish_speed > 0.f)
		wish_dir /= wish_speed;

	wish_speed = min(wish_speed, max_speed);

	float max_wish = g_interfaces->m_var->get_by_name("sv_air_max_wishspeed")->get_float();
	float speed = min(wish_speed, max_wish - wish_vel.dot(dir));

	if (speed > 0.f) {
		float accel = g_interfaces->m_var->get_by_name("sv_airaccelerate")->get_float();
		float accelerate_speed = min(speed, ((wish_speed * frame_time) * accel) * friction);
		wish_vel += (wish_dir * accelerate_speed);
	}
}
