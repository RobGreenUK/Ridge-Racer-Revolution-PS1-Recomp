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
static void hudLayoutRegression(SDL_Renderer*r){
    auto xy=[](int x,int y){return uint32_t(uint16_t(x))|(uint32_t(uint16_t(y))<<16);};
    uint32_t left[]={0x65000000,xy(7,16),0x79cf1000,0x00080030};
    uint32_t right[]={0x65000000,xy(258,16),0x79cf1030,0x00080030};
    uint32_t mirror[]={0x65000000,xy(74,16),0x780030c8,0x00080038};
    uint32_t map[]={0x2dffffff,xy(4,56),0x78050000,xy(75,56),15u<<16,xy(4,111),0,xy(75,111),0};
    uint32_t arrow[]={0x2900ffff,xy(-1,108),xy(9,102),xy(4,109),xy(6,113)};
    uint32_t needle[]={0x2980ffff,xy(264,196),xy(237,173),xy(266,194),xy(238,172)};
    assert(revolutionHudAnchor(left,4,5)==-1&&revolutionHudAnchor(right,4,5)==1);
    assert(revolutionHudAnchor(mirror,4,5)==0);
    assert(revolutionHudAnchor(map,9,0)==-1&&revolutionHudAnchor(arrow,5,5)==-1);
    assert(revolutionHudAnchor(needle,5,5)==1);
    assert(revolutionHudAnchor(left,4,4)==0&&revolutionHudAnchor(left,3,5)==0);
    for(int h:{240,720,1080,2160}){
        assert(revolutionHudShift(left,4,5,17,h*4/3,h,false)==0);
        assert(revolutionHudShift(left,4,5,17,h*16/9,h,false)==-(h*16/9-h*4.f/3)/2);
        for(unsigned state:{0u,1u,16u,32u,~0u})assert(revolutionHudShift(left,4,5,state,h*16/9,h,false)==0);
        assert(revolutionHudShift(left,4,5,17,h*16/9,h,true)==0);
    }
    // Real GL: explicit framebuffer scissor must move with the group. Use only
    // synthetic red texels, then compare the complete image with direct expected
    // rectangles for race, attract, replay and 4:3 at both framebuffer origins.
    HudRenderer hud;std::vector<uint16_t>vram(524288,0x001f);
    for(int h:{720,960})for(unsigned state:{17u,19u,32u})for(int displayY:{0,240}){
        auto*target=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,1280,h);assert(target);
        SDL_SetRenderTarget(r,target);SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);
        Frame f{};f.flags=state;
        f.hud={1,0xe1000005,1,0xe3000000u|(unsigned(displayY)<<10),1,0xe4000000u|319u|(unsigned(displayY+239)<<10)};
        for(auto*p:{left,right,mirror}){f.hud.push_back(4);f.hud.insert(f.hud.end(),p,p+4);}
        hud.draw(r,f,vram,1280,h,0,displayY);
        auto actual=pixels(r);
        SDL_SetRenderClipRect(r,nullptr);SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);
        float scale=h/240.f,margin=(1280-320*scale)/2;
        for(auto*p:{left,right,mirror}){
            int anchor=p==left?-1:p==right?1:0;
            float shift=state==32?0:anchor*margin;
            SDL_FRect box{margin+int16_t(p[1])*scale+shift,16*scale,float(p[3]&65535)*scale,8*scale};
            SDL_SetRenderDrawColor(r,255,0,0,255);SDL_RenderFillRect(r,&box);
        }
        assert(actual==pixels(r));SDL_SetRenderTarget(r,nullptr);SDL_DestroyTexture(target);
    }
    hud.close();
}
static void mirroredTileRegression(SDL_Renderer*r){
    HudRenderer hud;std::vector<uint16_t>vram(524288,0x001f);
    for(int y=0;y<8;y++)for(int x=0;x<8;x++)vram[(48+y)*1024+32+x]=uint16_t((x+1)|((y+1)<<5));
    auto xy=[](int x,int y){return uint32_t(x)|(uint32_t(y)<<16);};
    for(int scale:{1,3})for(bool flipX:{false,true})for(bool flipY:{false,true}){
        int width=320*scale,height=240*scale;
        auto*target=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,width,height);assert(target);
        SDL_SetRenderTarget(r,target);SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);
        Frame frame{};frame.flags=3;frame.hud={9,0x2dffffff};
        for(int i=0;i<4;i++){
            frame.hud.push_back(xy(20+(i&1?8:0),30+(i&2?8:0)));
            int u=flipX?39-(i&1?8:0):32+(i&1?8:0);
            int v=flipY?55-(i&2?8:0):48+(i&2?8:0);
            frame.hud.push_back(uint32_t(u)|(uint32_t(v)<<8)|(i==1?256u<<16:0));
        }
        hud.draw(r,frame,vram,width,height);auto actual=pixels(r);
        SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);
        for(int y=0;y<8;y++)for(int x=0;x<8;x++){
            unsigned red=(flipX?7-x:x)+1,green=(flipY?7-y:y)+1;
            SDL_SetRenderDrawColor(r,(red<<3)|(red>>2),(green<<3)|(green>>2),0,255);
            SDL_FRect cell{float((20+x)*scale),float((30+y)*scale),float(scale),float(scale)};SDL_RenderFillRect(r,&cell);
        }
        assert(actual==pixels(r));SDL_SetRenderTarget(r,nullptr);SDL_DestroyTexture(target);
    }
    hud.close();
}
int main(){
    assert(SDL_Init(SDL_INIT_VIDEO));SDL_Window*w=SDL_CreateWindow("Renderer parity regression",320,240,SDL_WINDOW_HIDDEN);assert(w);
    SDL_Renderer*r=SDL_CreateRenderer(w,"opengl");assert(r);
    hudLayoutRegression(r);mirroredTileRegression(r);
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
    // Menu models retain the GTE centre and omit static race geometry.
    frame.menuScene=true;frame.models.resize(1);frame.projection={256,156,320};
    mesh.focalScale=320.f/240;mesh.smooth=false;
    for(int width:{320,480}){
        auto*target=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA8888,SDL_TEXTUREACCESS_TARGET,width,240);assert(target);
        SDL_SetRenderTarget(r,target);SDL_SetRenderDrawColor(r,0,0,0,255);SDL_RenderClear(r);
        mesh.resetFrameStats();mesh.draw(r,frame,{},width,240);auto actual=pixels(r);
        assert(mesh.candidateTriangles==2);
        double xsum=0,ysum=0;unsigned count=0;
        for(int y=0;y<240;y++)for(int x=0;x<width;x++)if(actual[(y*width+x)*4]){xsum+=x+.5;ysum+=y+.5;count++;}
        assert(count&&std::abs(xsum/count-(width/2+96))<1&&std::abs(ysum/count-156)<1);
        SDL_SetRenderTarget(r,nullptr);SDL_DestroyTexture(target);
    }
    frame.menuScene=false;frame.projection={160,120,320};
    auto background=decodeBackground({12,0x3c102030,0,0x00100000,0x00405060,320,0x000500ff,0x00708090,240u<<16,0x0000ff00,0x00a0b0c0,(240u<<16)|320,0x0000ffff});
    assert(background.size()==1&&background[0].textured&&background[0].page==5&&background[0].clut==16);
    assert(background[0].rgb[3][0]==192&&background[0].xy[3][1]==240);
    // Authored car OT priorities must not inflate adjoining panels in world
    // space. Scenery retains its existing surface separation.
    frame.models.resize(1);frame.models[0].key=modelKey(1,0x8001B8CC,0);
    mesh.models[0][0].bias=-20;
    for(auto site:{0x8001B8CCu,0x8001B910u,0x8001BAA0u,0x8001BAF4u,0x8001BB38u,0x8001BD34u}){
        auto part=frame.models[0];part.key=modelKey(1,site,4);assert(revolutionCarSurface(frame,part));
    }
    mesh.draw(r,frame,{},320,240);
    bool carFace=false;for(const auto&face:mesh.faces)if(face.bias==-20){assert(face.layerOffset==0);carFace=true;}
    assert(carFace);
    frame.models[0].key=modelKey(1,0x80036B08,0);assert(!revolutionCarSurface(frame,frame.models[0]));
    frame.menuScene=true;frame.cameraIdentity[1]=3;assert(!revolutionCarSurface(frame,frame.models[0]));
    frame.cameraIdentity[1]=5;assert(revolutionCarSurface(frame,frame.models[0]));
    frame.menuScene=false;frame.cameraIdentity={};mesh.models[0][0].bias=0;
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
