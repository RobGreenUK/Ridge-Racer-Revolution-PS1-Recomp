/* Offline parity preview using the final Ridge Racer geometry/depth backend. */
#include "timeline.h"
#include <SDL3/SDL.h>
#include <fstream>
#include <iostream>
#include <cstring>
using namespace ridge;
#include "geometry.h"
#include "depth_renderer.h"
struct Face {uint32_t texture;int32_t bias;std::vector<MeshVertex> vertices;float layerOffset=0;};
static uint32_t u32(std::ifstream&f){uint32_t v;f.read((char*)&v,4);return v;}
static float f32(std::ifstream&f){float v;f.read((char*)&v,4);if(!std::isfinite(v))throw std::runtime_error("Invalid vertex");return v;}
int main(int argc,char**argv)try{
    if(argc!=3)throw std::runtime_error("usage: RevolutionScenePreview frame.rrmesh screenshot.bmp");
    std::ifstream in(argv[1],std::ios::binary);in.exceptions(std::ios::badbit|std::ios::failbit);char magic[8];in.read(magic,8);
    if(std::memcmp(magic,"RRVMESH1",8))throw std::runtime_error("Invalid mesh header");
    unsigned nt=u32(in),nq=u32(in);if(!nt||nt>4096||nq>100000)throw std::runtime_error("Invalid counts");
    SDL_SetHint(SDL_HINT_RENDER_DRIVER,"opengl");if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());
    SDL_Window*w;SDL_Renderer*r;if(!SDL_CreateWindowAndRenderer("Revolution scene parity preview",1280,960,SDL_WINDOW_HIDDEN,&w,&r))throw std::runtime_error(SDL_GetError());
    auto*target=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1280,960);
    if(!target||!SDL_SetRenderTarget(r,target))throw std::runtime_error(SDL_GetError());
    std::vector<SDL_Texture*> textures;std::vector<uint8_t>pixels(256*256*4);
    for(unsigned i=0;i<nt;i++){in.read((char*)pixels.data(),pixels.size());auto*t=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,256,256);if(!t||!SDL_UpdateTexture(t,nullptr,pixels.data(),1024))throw std::runtime_error(SDL_GetError());textures.push_back(t);}
    std::vector<Face> faces;
    for(unsigned j=0;j<nq;j++){
        auto material=u32(in);auto bias=int32_t(u32(in));if(material>=nt)throw std::runtime_error("Invalid material");
        MeshVertex v[4];for(auto&x:v){x.position={f32(in),f32(in),f32(in)};x.u=f32(in);x.v=f32(in);}
        for(auto order:{std::array<int,3>{0,1,2},std::array<int,3>{2,1,3}}){
            std::vector<MeshVertex> clipped;
            for(int k=0;k<3;k++){auto a=v[order[k]],b=v[order[(k+1)%3]];bool ai=a.position.z>=20,bi=b.position.z>=20;if(ai)clipped.push_back(a);if(ai!=bi){float t=(20-a.position.z)/(b.position.z-a.position.z);clipped.push_back({a.position+(b.position-a.position)*t,a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t});}}
            if(clipped.size()<3)continue;float area=0;
            for(size_t k=0;k<clipped.size();k++){auto a=clipped[k].position,b=clipped[(k+1)%clipped.size()].position;area+=a.x/a.z*b.y/b.z-a.y/a.z*b.x/b.z;}
            if(area<=0)continue;
            for(size_t k=1;k+1<clipped.size();k++)faces.push_back({material,bias,{clipped[0],clipped[k],clipped[k+1]},float(bias)*.125f});
        }
    }
    std::stable_sort(faces.begin(),faces.end(),[](auto&a,auto&b){return a.texture<b.texture;});
    SDL_SetRenderDrawColor(r,55,105,190,255);SDL_RenderClear(r);DepthRenderer depth;
    if(!depth.draw(r,textures,faces,1280,960))throw std::runtime_error("OpenGL depth backend required");
    auto*surface=SDL_RenderReadPixels(r,nullptr);if(!surface||!SDL_SaveBMP(surface,argv[2]))throw std::runtime_error(SDL_GetError());SDL_DestroySurface(surface);
    depth.close();for(auto*t:textures)SDL_DestroyTexture(t);SDL_DestroyTexture(target);SDL_DestroyRenderer(r);SDL_DestroyWindow(w);SDL_Quit();
    std::cout<<"Rendered "<<faces.size()<<" triangles\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}
