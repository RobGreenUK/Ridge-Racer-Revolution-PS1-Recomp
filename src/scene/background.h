#pragma once
#include <array>
#include <vector>
#include <cstdint>
#include "gp0_commands.h"
// Captured panorama tiles retain their UV identity as they scroll across the
// screen. Float positions let presentation interpolate without PS1 pixel steps.
struct BackgroundQuad {
    std::array<std::array<float,2>,4> xy{},uv{};
    std::array<std::array<float,3>,4> rgb{};
    uint32_t page=0,clut=0;bool textured=false;
};
inline std::vector<BackgroundQuad> decodeBackground(const std::vector<uint32_t>&packets){
    std::vector<BackgroundQuad> result;auto commands=splitDrawingCommands(packets);
    for(size_t i=0;i<commands.size();){
        unsigned n=commands[i++];if(n>commands.size()-i)break;const auto*p=commands.data()+i;i+=n;
        if(!n)continue;unsigned type=(p[0]>>24)&0xfc;BackgroundQuad q;
        auto xy=[](uint32_t p){return std::array<float,2>{float(int16_t(p)),float(int16_t(p>>16))};};
        auto rgb=[](uint32_t p){return std::array<float,3>{float(p&255),float((p>>8)&255),float((p>>16)&255)};};
        if(type==0x2c&&n==9){
            q.textured=true;q.page=(p[4]>>16)&511;q.clut=p[2]>>16;
            for(int v=0;v<4;v++){q.xy[v]=xy(p[1+v*2]);q.uv[v]={float(p[2+v*2]&255)/256,float((p[2+v*2]>>8)&255)/256};q.rgb[v]=rgb(p[0]);if(p[0]&0x01000000)q.rgb[v]={128,128,128};}
        }else if(type==0x3c&&n==12){
            q.textured=true;q.page=(p[5]>>16)&511;q.clut=p[2]>>16;
            for(int v=0;v<4;v++){q.xy[v]=xy(p[1+v*3]);q.uv[v]={float(p[2+v*3]&255)/256,float((p[2+v*3]>>8)&255)/256};q.rgb[v]=rgb(p[v*3]);if(p[0]&0x01000000)q.rgb[v]={128,128,128};}
        }else if(type==0x38&&n==8){for(int v=0;v<4;v++){q.xy[v]=xy(p[v*2+1]);q.rgb[v]=rgb(p[v*2]);}}
        else if(type==0x60&&n==3){auto origin=xy(p[1]);for(int v=0;v<4;v++){q.xy[v]={origin[0]+float(v&1?p[2]&65535:0),origin[1]+float(v&2?p[2]>>16:0)};q.rgb[v]=rgb(p[0]);}}
        else continue;
        result.push_back(q);
    }
    return result;
}
inline std::vector<BackgroundQuad> interpolateBackground(const std::vector<BackgroundQuad>&a,const std::vector<BackgroundQuad>&b,float t){
    if(t>=1)return b;auto out=a;std::vector<bool>used(b.size());
    for(auto&q:out){
        size_t match=b.size();float best=128.f*128*4;
        for(size_t i=0;i<b.size();i++){
            const auto&r=b[i];if(used[i]||q.textured!=r.textured||(q.textured&&(q.page!=r.page||q.clut!=r.clut||q.uv!=r.uv)))continue;
            float distance=0;for(int v=0;v<4;v++)for(int k=0;k<2;k++){float d=r.xy[v][k]-q.xy[v][k];distance+=d*d;}
            if(distance<best){best=distance;match=i;}
        }
        if(match==b.size())continue;used[match]=true;const auto&r=b[match];
        for(int v=0;v<4;v++){for(int k=0;k<2;k++)q.xy[v][k]+=(r.xy[v][k]-q.xy[v][k])*t;for(int k=0;k<3;k++)q.rgb[v][k]+=(r.rgb[v][k]-q.rgb[v][k])*t;}
    }
    return out;
}
