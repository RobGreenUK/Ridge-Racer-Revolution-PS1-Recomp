#pragma once
#include <stdint.h>
// SLUS-00214 dispatch table 80070EAC: 17 race, 19 attract,
// 32 recorded post-race replay (80026EE0). Setup/menu states stay 2D.
static inline int revolution_replay_state(uint32_t state){return state==32;}
static inline int revolution_scene_state(uint32_t state){return state==17||state==19||revolution_replay_state(state);}
