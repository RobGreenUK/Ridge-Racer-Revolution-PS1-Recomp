#pragma once
#include <stdint.h>
#define RRV_SHARED_MAGIC 0x52525636u
#define RRV_MODEL_CAP 512
#define RRV_HUD_CAP 8192
#define RRV_SKY_CAP 2048
struct RRVModel {
    uint32_t owner,site,model,pass,part,rotation[5],translation[3],palette;
};
struct RRVSnapshot {
    uint32_t sequence,state,course,valid;
    uint64_t cycles;
    int32_t camera[3];int16_t matrix[9],mirror_matrix[9];
    uint32_t model_count,hud_count,sky_count,screen_width,screen_height,display_x,display_y,mirror_enabled,full_scene;
    struct RRVModel models[RRV_MODEL_CAP];
    uint32_t hud[RRV_HUD_CAP],sky[RRV_SKY_CAP];
    uint16_t vram[1024*512];
    uint32_t screen[640*512];
    uint32_t camera_valid,camera_mode,camera_target,camera_shot;
    uint32_t menu_scene,menu_substate;int32_t projection[3];
};
struct RRVShared {
    uint32_t magic,size;
    uint64_t input_ns;
    uint32_t input_buttons,input_active;
    uint64_t published_ns;
    struct RRVSnapshot frame;
};
