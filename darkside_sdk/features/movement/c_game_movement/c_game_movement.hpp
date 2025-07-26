#pragma once

#include "../../../darkside.hpp"

class c_game_movement {
public:
	void air_accelerate(vec3_t& wish_vel, vec3_t& wish_angles, float friction, vec3_t& move, float frame_time, float max_speed);
};
