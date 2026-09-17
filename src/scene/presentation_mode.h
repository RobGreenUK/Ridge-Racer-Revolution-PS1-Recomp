#pragma once
#include <stdint.h>
// Verified SLUS-00214 dispatch states. Selection submenus are gated separately.
static inline int revolution_replay_state(uint32_t state){return state==32;}
static inline int revolution_camera_scene(uint32_t state){return state==29||state==32;}
static inline int revolution_menu_state(uint32_t state){return state==3||state==5;}
static inline int revolution_scene_state(uint32_t state){return state==17||state==19||revolution_camera_scene(state);}
static inline int revolution_menu_scene(uint32_t state,uint32_t sub){return revolution_menu_state(state)&&(sub<=3||sub==5);}
