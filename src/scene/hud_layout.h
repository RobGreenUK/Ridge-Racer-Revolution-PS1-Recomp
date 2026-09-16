#pragma once
#include <algorithm>
#include <cstdint>
// SLUS-00214 HUD packet families, checked against race captures on EASY/MID/HIGH.
// Match whole primitives so map arrows and the tachometer needle stay coherent.
inline int revolutionHudAnchor(const uint32_t*p,unsigned n,uint16_t page){
    if(!n)return 0;
    unsigned type=(p[0]>>24)&0xfc;
    auto inside=[&](unsigned first,unsigned stride,unsigned count,int l,int t,int r,int b){
        for(unsigned i=0;i<count;i++){auto xy=p[first+i*stride];int x=int16_t(xy),y=int16_t(xy>>16);if(x<l||x>r||y<t||y>b)return false;}return true;
    };
    if((type==0x64&&n==4)||((type==0x74||type==0x7c)&&n==3)){
        if(page!=5)return 0;
        int x=int16_t(p[1]),y=int16_t(p[1]>>16);
        unsigned clut=p[2]>>16,u=p[2]&255,v=(p[2]>>8)&255;
        // TIME / POSITION headings.
        if(type==0x64&&clut==0x79cf&&y==16&&v==16&&p[3]==0x00080030){
            if(x==7&&u==0)return -1;if(x==258&&u==48)return 1;
        }
        // RECORD / TOTAL labels and times.
        if(type==0x74&&(clut==0x7800||clut==0x7846)&&x>=7&&x<=71&&
           (y==130||y==138||y==154||y==162)&&v<=8)return -1;
        // Six gear strips, including the highlighted gear.
        if(type==0x64&&(clut==0x7807||clut==0x7808)&&x==8&&y>=178&&y<=218&&
           (y-178)%8==0&&v==32&&p[3]==0x00080018)return -1;
        // Lap labels and completed/current lap times.
        if(type==0x74&&(clut==0x7800||clut==0x7846)&&x>=249&&x<=305&&y>=56&&y<=120&&v<=8)return 1;
        // Dial, speed unit panel, digital speed and gear beside the dial.
        if(type==0x64&&clut==0x7845){
            if(x==224&&y==154&&u==72&&v==72&&p[3]==0x00500058)return 1;
            if(x==256&&y==186&&u==136&&v==56&&p[3]==0x00100010)return 1;
        }
        if(type==0x74&&clut==0x7800&&v==16&&
           ((x>=260&&x<=276&&y==220)||(x==293&&y==209)))return 1;
    }
    if(type==0x2c&&n==9){
        unsigned texturePage=(p[4]>>16)&511,clut=p[2]>>16;
        // Course map uses its own page; mirrored UV order is equally valid.
        if(texturePage==15&&clut==0x7805&&inside(1,2,4,4,56,75,111))return -1;
        if(texturePage==5&&clut==0x7803&&inside(1,2,4,7,26,53,50))return -1;
        if(texturePage==5&&clut==0x7802&&inside(1,2,4,258,26,304,50))return 1;
    }
    if(type==0x28&&n==5){
        // Arrow tips can extend beyond the map rectangle as cars turn.
        if(inside(1,1,4,-8,48,88,122))return -1;
        if(inside(1,1,4,224,154,312,234))return 1;
    }
    return 0; // mirror, messages, fades, unknown commands
}
inline float revolutionHudShift(const uint32_t*p,unsigned n,uint16_t page,uint32_t state,int width,int height,bool fillWidth){
    if(fillWidth||(state!=17&&state!=19))return 0;
    return revolutionHudAnchor(p,n,page)*std::max(0.f,(width-height*4.f/3.f)/2.f);
}
