/* Synthetic regression: no generated geometry, textures or game data. */
#define main revolution_viewer_main
#include "../src/scene/native.cpp"
#undef main
#include <cassert>
#include <random>

static std::vector<unsigned char> pixels(SDL_Renderer*r){
    auto*s=SDL_RenderReadPixels(r,nullptr);assert(s);
    auto*c=SDL_ConvertSurface(s,SDL_PIXELFORMAT_RGBA32);assert(c);
    std::vector<unsigned char>out(c->w*c->h*4);
    for(int y=0;y<c->h;y++)std::memcpy(out.data()+y*c->w*4,(char*)c->pixels+y*c->pitch,c->w*4);
    SDL_DestroySurface(c);SDL_DestroySurface(s);return out;
}
static MeshQuad quad(float x,float z,unsigned texture=0){
    MeshQuad q{};q.texture=texture;
    for(int i=0;i<4;i++)q.v[i]={{x+(i&1?10.f:-10.f),i&2?10.f:-10.f,z},i&1?1.f:0.f,i&2?1.f:0.f};
    return q;
}
int main(){
    assert(SDL_Init(SDL_INIT_VIDEO));SDL_Window*w=SDL_CreateWindow("Renderer parity regression",320,240,SDL_WINDOW_HIDDEN);assert(w);
    SDL_Renderer*r=SDL_CreateRenderer(w,"opengl");assert(r);
    CourseMesh mesh;mesh.depthRenderer.measureGpu=false;
    for(auto colour:{0xffffffffu,0xff0000ffu}){
        auto*t=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,256,256);assert(t);
        std::vector<uint32_t>rgba(65536,colour);assert(SDL_UpdateTexture(t,nullptr,rgba.data(),1024));mesh.textures.push_back(t);
    }
    for(int i=0;i<128;i++)mesh.quads.push_back(quad(i<64?0:10000,100,i%2));
    mesh.models={{quad(0,100)}};
    Frame frame{};frame.rotation.w=1;frame.flags=17;
    ModelPose pose{};pose.key=1;pose.model=0;pose.matrix={1,0,0,0,1,0,0,0,1};frame.models.push_back(pose);
    pose.position.x=10000;frame.models.push_back(pose);
    mesh.buildBounds();auto chunks=mesh.chunkBounds,models=mesh.modelBounds;
    for(bool perspective:{false,true})for(float focal:{320.f/240,8.f})for(bool smooth:{false,true}){
        int width=focal==8?176:320,height=focal==8?40:240;
        auto*target=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,width,height);assert(target);
        assert(SDL_SetRenderTarget(r,target));mesh.perspective=perspective;mesh.focalScale=focal;mesh.smooth=smooth;
        std::vector<unsigned char>before,after;unsigned candidates[2];
        for(int optimized=0;optimized<2;optimized++){
            mesh.chunkBounds=optimized?chunks:std::vector<CourseMesh::Bounds>{};
            mesh.modelBounds=optimized?models:std::vector<CourseMesh::Bounds>{};
            SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);mesh.resetFrameStats();mesh.draw(r,frame,{},width,height);
            (optimized?after:before)=pixels(r);candidates[optimized]=mesh.candidateTriangles;
            for(size_t i=1;i<mesh.faces.size();i++)if(mesh.faces[i-1].texture==mesh.faces[i].texture&&mesh.faces[i-1].bias==mesh.faces[i].bias)assert(mesh.faces[i-1].order<mesh.faces[i].order);
        }
        assert(before==after&&candidates[1]<candidates[0]);assert(mesh.culledChunks==1&&mesh.culledModels==1);
        SDL_SetRenderTarget(r,nullptr);SDL_DestroyTexture(target);
    }
    // Bounds must contain an actually visible point, including transformed
    // instances, camera rotation and the mirror's different projection.
    std::mt19937 random(42);std::uniform_real_distribution<float>d(-1000,1000);
    for(int i=0;i<10000;i++){
        CourseMesh::Bounds b;Vec point{d(random),d(random),d(random)};b.add(point-Vec{20,30,40});b.add(point+Vec{20,30,40});
        pose.position={d(random),d(random),d(random)};pose.matrix={1,.2f,0,0,2,0,.1f,0,-1};
        frame.rotation=normalized({.1f,.2f,.3f,1});Vec camera{d(random),d(random),d(random)};
        Vec p=pose.position+Vec{point.x+.2f*point.y,2*point.y,.1f*point.x-point.z};p=rotate(frame.rotation,p-camera);
        for(float focal:{320.f/240,8.f})if(p.z>=20&&std::abs(p.x/p.z)<=.5f/focal*16/9&&std::abs(p.y/p.z)<=.5f/focal)
            assert(b.visible(frame,camera,&pose,1600,900,focal));
    }
    // Cache hits preserve first-match semantics and separate resident pixels,
    // streamed pages, palettes and texture windows. Live CLUT changes still
    // refresh resident textures; displaced page words do not.
    mesh.sky.vram.assign(524288,0x1111);mesh.textureKeys={{28,0},{28,0}};mesh.textureWindows={0,0};
    mesh.residentTexels.resize(2);mesh.residentTexels[0].assign(16384,0x1111);
    assert(mesh.paletteTexture(r,0,0)==0);assert(mesh.paletteTexture(r,1,0)==1);
    auto variant=mesh.paletteTexture(r,0,64u<<16,1);
    auto streamed=mesh.paletteTexture(r,1,64u<<16,1);assert(variant!=streamed);
    for(int i=0;i<100;i++)assert(mesh.paletteTexture(r,0,64u<<16,1)==variant);
    assert(mesh.textures.size()==4);
    mesh.textureDirty.assign(4,false);auto words=mesh.sky.vram;words[256*1024+12*64]^=1;
    TextureSignatures a{mesh.sky.vram,{}},b{words,{}};
    HudRenderer hud;hud.updateVram(a,b);mesh.updateVram(words,a,b);
    assert(!mesh.textureDirty[0]&&mesh.textureDirty[1]&&!mesh.textureDirty[variant]&&mesh.textureDirty[streamed]);
    mesh.textureDirty.assign(4,false);words[0]^=1;mesh.updateVram(words);assert(mesh.textureDirty[0]);
    mesh.resetFrameStats();mesh.prepareTexture(0);assert(mesh.textureUpdates==1);mesh.prepareTexture(0);assert(mesh.textureUpdates==1);
    mesh.resetFrameStats();assert(mesh.textureUpdates==0);
    mesh.close();hud.close();SDL_DestroyRenderer(r);SDL_DestroyWindow(w);SDL_Quit();
    std::cout<<"culling, stable ordering, palette cache and resident uploads passed\n";
}
