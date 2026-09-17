#pragma once
#include "scene_flags.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <vector>
#include "background.h"
namespace ridge {
struct Vec { float x=0,y=0,z=0; };
inline Vec operator+(Vec a, Vec b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec operator-(Vec a, Vec b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec operator*(Vec a,float t) { return {a.x*t,a.y*t,a.z*t}; }
struct Quat { float x=0,y=0,z=0,w=1; };
inline Quat normalized(Quat q) {
    float n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if(n<1e-8f) throw std::runtime_error("zero quaternion");
    return {q.x/n,q.y/n,q.z/n,q.w/n};
}
inline Quat slerp(Quat a,Quat b,float t) {
    float dot=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w;
    if(dot<0) { b={-b.x,-b.y,-b.z,-b.w}; dot=-dot; }
    float x=1-t,y=t;
    if(dot<0.9995f) {
        float angle=std::acos(std::clamp(dot,-1.f,1.f));
        x=std::sin((1-t)*angle)/std::sin(angle);y=std::sin(t*angle)/std::sin(angle);
    }
    return normalized({a.x*x+b.x*y,a.y*x+b.y*y,a.z*x+b.z*y,a.w*x+b.w*y});
}
inline Vec rotate(Quat q,Vec p) {
    Vec u={q.x,q.y,q.z};
    auto cross=[](Vec a,Vec b)->Vec{return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};
    Vec t=cross(u,p)*2;
    return p+t*q.w+cross(u,t);
}
inline Quat matrixRotation(const std::array<float,9>&m) {
    float q[4]={};float trace=m[0]+m[4]+m[8];
    if(trace>0){float s=std::sqrt(trace+1)*2;q[3]=s/4;q[0]=(m[7]-m[5])/s;q[1]=(m[2]-m[6])/s;q[2]=(m[3]-m[1])/s;}
    else {int i=0;if(m[4]>m[0])i=1;if(m[8]>m[i*3+i])i=2;int j=(i+1)%3,k=(i+2)%3;
        float s=std::sqrt(std::max(0.f,1+m[i*3+i]-m[j*3+j]-m[k*3+k]))*2;
        if(s<1e-6)return {};
        q[i]=s/4;q[j]=(m[j*3+i]+m[i*3+j])/s;q[k]=(m[k*3+i]+m[i*3+k])/s;q[3]=(m[k*3+j]-m[j*3+k])/s;}
    return normalized({q[0],q[1],q[2],q[3]});
}
inline std::array<float,9> interpolateMatrix(std::array<float,9>a,std::array<float,9>b,float t) {
    // Keep model scale while interpolating rotation on a sphere. Linear matrix
    // interpolation collapses a turning car at the midpoint of a half turn.
    float sa[3],sb[3];
    for(int c=0;c<3;c++) {
        sa[c]=std::sqrt(a[c]*a[c]+a[c+3]*a[c+3]+a[c+6]*a[c+6]);
        sb[c]=std::sqrt(b[c]*b[c]+b[c+3]*b[c+3]+b[c+6]*b[c+6]);
        if(sa[c]<1e-6||sb[c]<1e-6)return t<1?a:b;
    }
    auto determinant=[](const std::array<float,9>&m){return m[0]*(m[4]*m[8]-m[5]*m[7])-m[1]*(m[3]*m[8]-m[5]*m[6])+m[2]*(m[3]*m[7]-m[4]*m[6]);};
    if(determinant(a)<0)sa[0]=-sa[0];
    if(determinant(b)<0)sb[0]=-sb[0];
    if(sa[0]*sb[0]<0)return t<1?a:b; // reflection changes are cuts
    for(int c=0;c<3;c++)for(int r=0;r<3;r++){a[r*3+c]/=sa[c];b[r*3+c]/=sb[c];}
    Quat q=slerp(matrixRotation(a),matrixRotation(b),t);std::array<float,9>out;
    for(int c=0;c<3;c++) {
        Vec axis{c==0?1.f:0.f,c==1?1.f:0.f,c==2?1.f:0.f};
        Vec v=rotate(q,axis)*(sa[c]+(sb[c]-sa[c])*t);
        out[c]=v.x;out[c+3]=v.y;out[c+6]=v.z;
    }
    return out;
}
inline uint64_t modelKey(uint32_t owner,uint32_t site,uint32_t part){return (uint64_t(owner)<<32)|((site&0x1fffffff)<<3)|(part&7);}
struct ModelPose { uint64_t key;uint32_t model;Vec position;std::array<float,9> matrix;uint32_t paletteOffset=0; };
struct Sky {float pitch=0,yaw=0,roll=0;uint32_t mirror=0,clut=0,rgb=0,enabled=0;};
struct Frame { double time; uint32_t flags; Vec camera; Quat rotation; Vec car; float yaw;std::vector<ModelPose> models;Sky sky;std::vector<uint32_t>hud;int hudDisplayX=0,hudDisplayY=0;std::vector<BackgroundQuad>background;
    std::array<uint32_t,4> cameraIdentity{}; bool menuScene=false; std::array<float,3> projection{160,120,320}; std::vector<uint32_t> menuBackdrop; };
inline Frame interpolate(const Frame&a,const Frame&b,double time) {
    // State/camera discontinuities are cuts. Freeze previous scene until boundary.
    Vec delta=b.camera-a.camera;
    if(b.time<=a.time || b.time-a.time>.15 || a.flags!=b.flags || a.cameraIdentity!=b.cameraIdentity || a.projection!=b.projection ||
       delta.x*delta.x+delta.y*delta.y+delta.z*delta.z>4000.f*4000.f)
        return time<b.time?a:b;
    float t=static_cast<float>(std::clamp((time-a.time)/(b.time-a.time),0.,1.));
    float yawDelta=std::remainder(b.yaw-a.yaw,6.28318530718f);
    Frame result{time,a.flags,a.camera+(b.camera-a.camera)*t,slerp(a.rotation,b.rotation,t),
            a.car+(b.car-a.car)*t,a.yaw+yawDelta*t,{},{},{},0,0,{}};
    result.cameraIdentity=a.cameraIdentity;result.menuScene=a.menuScene;result.menuBackdrop=a.menuBackdrop;result.projection=a.projection;
    result.background=interpolateBackground(a.background,b.background,t);
    result.hud=a.hud;result.hudDisplayX=a.hudDisplayX;result.hudDisplayY=a.hudDisplayY;result.sky=a.sky;
    if(a.sky.enabled&&b.sky.enabled&&a.sky.mirror==b.sky.mirror) {
        result.sky.pitch=a.sky.pitch+std::remainder(b.sky.pitch-a.sky.pitch,4096.f)*t;
        result.sky.yaw=a.sky.yaw+std::remainder(b.sky.yaw-a.sky.yaw,4096.f)*t;
        result.sky.roll=a.sky.roll+std::remainder(b.sky.roll-a.sky.roll,4096.f)*t;
    }
    if(t>=1){result.menuBackdrop=b.menuBackdrop;result.hudDisplayX=b.hudDisplayX;result.hudDisplayY=b.hudDisplayY;result.hud=b.hud;result.sky=b.sky;result.models=b.models;return result;}
    // Model animation (notably spinning-wheel variants) may change the mesh
    // while the same component keeps moving. Interpolate its pose by identity.
    for(const auto&previous:a.models) {
        auto pose=previous;
        auto next=std::find_if(b.models.begin(),b.models.end(),[&](const ModelPose&candidate){return candidate.key==previous.key;});
        if((previous.key>>32)!=0&&next!=b.models.end()) {
            pose.position=previous.position+(next->position-previous.position)*t;
            pose.matrix=interpolateMatrix(previous.matrix,next->matrix,t);
        }
        result.models.push_back(pose);
    }
    return result;
}
inline Frame sample(const std::vector<Frame>& frames,double time) {
    if(frames.empty()) throw std::runtime_error("empty timeline");
    if(time<=frames.front().time)return frames.front();
    auto it=std::upper_bound(frames.begin(),frames.end(),time,[](double t,const Frame&f){return t<f.time;});
    if(it==frames.end())return frames.back();
    return interpolate(*(it-1),*it,time);
}
}
