#pragma once
#include "shared.h"
#include "gpu.h"
#include "gpu_render.h"

/* Match the original display in boot/minigame and non-scene states. The GPU
 * readback API flushes queued primitives; raw CPU VRAM alone can be stale. */
static void revolution_capture_screen(struct RRVSnapshot*frame){
    GpuDisplayInfo info;gpu_get_display_info(&info);
    frame->screen_width=frame->screen_height=0;
    if(info.disabled||!info.width||info.width>640||!info.height||info.height>512)return;
    if(info.depth24){
        static uint16_t raw[1024*512];unsigned words=(info.width*3+1)/2;
        gr_vram_transfer_out(info.display_x,info.display_y,words,info.height,raw);
        for(unsigned y=0;y<info.height;y++)for(unsigned x=0;x<info.width;x++){
            const uint8_t*p=(const uint8_t*)(raw+y*words)+x*3;
            frame->screen[y*info.width+x]=0xff000000u|(uint32_t)p[0]<<16|(uint32_t)p[1]<<8|p[2];
        }
    }else if(!gr_render_display(frame->screen,info.width*4,info.display_x,info.display_y,info.width,info.height))return;
    frame->screen_width=info.width;frame->screen_height=info.height;
}
