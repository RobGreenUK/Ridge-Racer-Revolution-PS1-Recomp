#pragma once
// Original triangle-strip topology and near clipping, with native GPU depth.
// Non-OpenGL diagnostic fallback uses perspective tessellation and sorting.
#include "geometry.h"
#include "texture_data.h"
#include "depth_renderer.h"
struct MeshQuad { MeshVertex v[4]; uint32_t texture,kind; int32_t bias;uint32_t window=0,dayOnly=0; };
inline bool nightScenery(const Frame&){return false;}
struct CourseMesh {
    bool fullCourse=true,perspective=true,smooth=false;float focalScale=320.f/240;
    struct Face {float depth;uint32_t texture;int32_t bias;float layerOffset;std::array<MeshVertex,3> vertices;};
    std::vector<Face> faces;
    unsigned textureUpdates=0;
    std::vector<uint8_t> uploadPixels;
    SkyRenderer sky;
    DepthRenderer depthRenderer;
    std::vector<SDL_Texture*> textures;
    std::vector<MeshQuad> quads;
    std::vector<std::pair<int32_t,uint32_t>>textureKeys;
    std::vector<std::vector<MeshQuad>> models;
    std::vector<uint32_t>textureWindows;
    std::vector<bool> textureDirty;
    std::vector<std::vector<uint16_t>> residentTexels;
    void prepareTexture(unsigned i){
        if(i>=textureDirty.size()||!textureDirty[i])return;
        auto [page,clut]=textureKeys[i];
        texturePixelsInto(uploadPixels,sky.vram,page,clut,i<textureWindows.size()?textureWindows[i]:0,i<residentTexels.size()?&residentTexels[i]:nullptr);
        if(!SDL_UpdateTexture(textures[i],nullptr,uploadPixels.data(),1024))throw std::runtime_error(SDL_GetError());
        textureDirty[i]=false;textureUpdates++;
    }
    void load(SDL_Renderer*renderer,const std::string&path) {
        std::ifstream in(path,std::ios::binary);in.exceptions(std::ios::badbit|std::ios::failbit);
        char magic[8];in.read(magic,8);if(std::memcmp(magic,"RRASSET1",8)&&std::memcmp(magic,"RRASSET2",8)&&std::memcmp(magic,"RRASSET3",8)&&std::memcmp(magic,"RRASSET4",8)&&std::memcmp(magic,"RRASSET5",8)&&std::memcmp(magic,"RRASSET6",8)&&std::memcmp(magic,"RRASSET7",8))throw std::runtime_error("invalid assets header");
        auto nt=u32(in),nq=u32(in);if(nt>4096||nq>100000||nt==0)throw std::runtime_error("invalid mesh counts");
        std::vector<uint8_t> pixels(256*256*4);
        for(unsigned i=0;i<nt;i++) {
            in.read((char*)pixels.data(),pixels.size());
            SDL_Texture*t=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,256,256);
            if(!t||!SDL_UpdateTexture(t,nullptr,pixels.data(),256*4))throw std::runtime_error(SDL_GetError());
            SDL_SetTextureScaleMode(t,SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);textures.push_back(t);
        }
        auto readQuad=[&](){
            MeshQuad q;q.texture=u32(in);q.kind=u32(in);q.bias=int32_t(u32(in));if(magic[7]>='5')q.window=u32(in);if(magic[7]>='6')q.dayOnly=u32(in);
            if(q.texture>=nt||q.kind>2||q.dayOnly>1)throw std::runtime_error("invalid quad attributes");
            for(auto&v:q.v){v.position=vec(in);v.u=f32(in);v.v=f32(in);}
            return q;
        };
        for(unsigned i=0;i<nq;i++)quads.push_back(readQuad());
        if(magic[7]>='2') {
            unsigned count=u32(in);if(count>4096)throw std::runtime_error("invalid model count");
            models.resize(count);
            for(auto&model:models){unsigned size=u32(in);if(size>10000)throw std::runtime_error("invalid model size");for(unsigned i=0;i<size;i++)model.push_back(readQuad());}
        }
        if(magic[7]>='3')sky.load(in);
        if(magic[7]>='4')for(unsigned i=0;i<nt;i++){int32_t page=int32_t(u32(in));auto clut=u32(in);textureKeys.emplace_back(page,clut);textureWindows.push_back(0);}
        if(magic[7]>='7'){
            residentTexels.resize(nt);textureDirty.resize(nt);
            unsigned count=u32(in);if(count>nt)throw std::runtime_error("invalid resident texture count");
            for(unsigned j=0;j<count;j++){
                unsigned i=u32(in);if(i>=nt||(textureKeys[i].first!=28&&textureKeys[i].first!=29)||!residentTexels[i].empty())throw std::runtime_error("invalid resident texture");
                auto&words=residentTexels[i];words.resize(16384);
                for(unsigned k=0;k<words.size();k+=2){auto packed=u32(in);words[k]=packed&65535;words[k+1]=packed>>16;}
                textureDirty[i]=true;
            }
        }
    }
    uint32_t paletteTexture(SDL_Renderer*renderer,uint32_t base,uint32_t packed,uint32_t window=0) {
        if(base>=textureKeys.size())return base;
        auto key=textureKeys[base];if(key.first<0)return base;
        key.second=packed>>16;
        bool resident=base<residentTexels.size()&&!residentTexels[base].empty();
        for(size_t i=0;i<textureKeys.size();i++)if(textureKeys[i]==key&&(i<textureWindows.size()?textureWindows[i]:0)==window&&resident==(i<residentTexels.size()&&!residentTexels[i].empty()))return uint32_t(i);
        if(textures.size()>=4096)throw std::runtime_error("too many model palettes");
        auto source=base<residentTexels.size()?residentTexels[base]:std::vector<uint16_t>{};
        auto rgba=texturePixels(sky.vram,key.first,key.second,window,&source);
        auto*t=SDL_CreateTexture(renderer,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,256,256);
        if(!t)throw std::runtime_error(SDL_GetError());
        if(!SDL_UpdateTexture(t,nullptr,rgba.data(),1024)){SDL_DestroyTexture(t);throw std::runtime_error(SDL_GetError());}
        SDL_SetTextureScaleMode(t,SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);
        residentTexels.resize(textureKeys.size());residentTexels.push_back(std::move(source));
        textureWindows.resize(textureKeys.size());textureWindows.push_back(window);textureKeys.push_back(key);textures.push_back(t);return uint32_t(textures.size()-1);
    }
    void updateVram(const std::vector<uint16_t>&words) {
        textureUpdates=0;
        if(words.size()!=524288)return;
        TextureSignatures before{sky.vram,{}},after{words,{}};
        textureDirty.resize(textureKeys.size());
        for(size_t i=0;i<textureKeys.size();i++){
            auto [page,clut]=textureKeys[i];if(page<0)continue;
            // Immutable resident page words, not the live VRAM page, supply these texels.
            bool resident=((page>>7)&3)==0&&i<residentTexels.size()&&residentTexels[i].size()==16384;
            if(before.get(page,clut,resident)==after.get(page,clut,resident))continue;
            textureDirty[i]=true;
        }
        if(sky.palette!=~0u&&(before.get(21,sky.palette)!=after.get(21,sky.palette)||before.get(22,sky.palette)!=after.get(22,sky.palette)))sky.close();
        sky.vram=words;
    }
    static MeshVertex lerp(MeshVertex a,MeshVertex b,float t) {
        return {a.position+(b.position-a.position)*t,a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t};
    }
    void draw(SDL_Renderer*renderer,const Frame&frame,Vec camera,int width,int height) {
        const float cx=width/2.f,cy=height/2.f,focal=height*focalScale;
        bool depthPass=std::strcmp(SDL_GetRendererName(renderer),"opengl")==0;
        faces.clear();faces.reserve(quads.size());
        auto addTriangle=[&](const MeshQuad&q,const ModelPose*pose,std::array<int,3> order) {
            uint32_t texture=q.texture,packed=0;
            bool remap=((pose&&pose->paletteOffset)||q.window)&&q.texture<textureKeys.size()&&textureKeys[q.texture].first>=0;
            if(remap){
                packed=(textureKeys[q.texture].second<<16)|uint32_t(q.v[0].v*256)<<8|uint32_t(q.v[0].u*256);
                if(pose)packed+=pose->paletteOffset;
            }
            // PS1 FT4 order is a triangle strip (0,1,2,3), not a perimeter.
            std::array<MeshVertex,3> polygon;unsigned vertexIndex=0;float depth=0;
            for(int i:order){auto v=q.v[i];
                if(remap&&i==0){v.u=(packed&255)/256.f;v.v=((packed>>8)&255)/256.f;}
                if(pose){auto p=v.position;const auto&m=pose->matrix;v.position=pose->position+Vec{m[0]*p.x+m[1]*p.y+m[2]*p.z,m[3]*p.x+m[4]*p.y+m[5]*p.z,m[6]*p.x+m[7]*p.y+m[8]*p.z};}
                v.position=rotate(frame.rotation,v.position-camera);depth+=v.position.z;polygon[vertexIndex++]=v;}
            if(!fullCourse&&!pose&&depth/3>18000)return;
            std::array<MeshVertex,4> clipped;size_t clippedCount=0;
            for(size_t i=0;i<polygon.size();i++) {
                auto a=polygon[i],b=polygon[(i+1)%polygon.size()];bool ai=a.position.z>=20,bi=b.position.z>=20;
                if(ai)clipped[clippedCount++]=a;
                if(ai!=bi)clipped[clippedCount++]=lerp(a,b,(20-a.position.z)/(b.position.z-a.position.z));
            }
            if(clippedCount<3)return;
            // Retain positive projected area in the normal view.
            float area=0;
            for(size_t i=0;i<clippedCount;i++) {
                auto a=clipped[i].position,b=clipped[(i+1)%clippedCount].position;
                area+=(a.x/a.z)*(b.y/b.z)-(a.y/a.z)*(b.x/b.z);
            }
            if(area<=0)return;
            bool outside[4]={true,true,true,true};
            for(size_t i=0;i<clippedCount;i++){const auto&v=clipped[i];float x=cx+focal*v.position.x/v.position.z,y=cy+focal*v.position.y/v.position.z;
                outside[0]&=x<0;outside[1]&=x>width;outside[2]&=y<0;outside[3]&=y>height;}
            if(outside[0]||outside[1]||outside[2]||outside[3])return;
            if(remap)texture=paletteTexture(renderer,q.texture,packed,q.window);
            for(size_t i=1;i+1<clippedCount;i++){
                // Road contact has an eight-unit depth tolerance: authored tires
                // and shadow planes extend slightly below the geometric road.
                // Keep the car parts' mutual depth and all screen coordinates intact.
                auto emit=[&](MeshVertex a,MeshVertex b,MeshVertex c){faces.push_back({(a.position.z+b.position.z+c.position.z)/3,texture,q.bias,float(q.bias)*.125f+(!pose&&q.kind==1?8.f:0.f),{a,b,c}});};
                if(depthPass||!perspective)emit(clipped[0],clipped[i],clipped[i+1]);
                else perspectiveTriangles(clipped[0],clipped[i],clipped[i+1],emit);
            }
        };
        auto addQuad=[&](const MeshQuad&q,const ModelPose*pose){addTriangle(q,pose,{0,1,2});addTriangle(q,pose,{2,1,3});};
        bool night=nightScenery(frame);
        for(const auto&q:quads)if(!q.dayOnly||!night)addQuad(q,nullptr);
        for(const auto&pose:frame.models){
            if(pose.model<models.size())for(const auto&q:models[pose.model])addQuad(q,&pose);
        }
        // Decode/upload only textures needed by surviving faces. Dirty off-screen
        // variants retain their flag and use the newest VRAM when seen again.
        for(const auto&face:faces)prepareTexture(face.texture);
        if(depthPass){
            std::stable_sort(faces.begin(),faces.end(),[](const Face&a,const Face&b){return a.texture==b.texture?a.bias<b.bias:a.texture<b.texture;});
            if(depthRenderer.draw(renderer,textures,faces,width,height,perspective,focalScale,smooth))return;
        }
        // This approximate ordering is diagnostic, not a replacement for the
        // game's ordering-table/depth-bias rules (bridges/transparency pending).
        std::stable_sort(faces.begin(),faces.end(),[](const Face&a,const Face&b){return a.depth+a.bias*8>b.depth+b.bias*8;});
        std::vector<SDL_Vertex> vertices;
        uint32_t texture=0;
        auto flush=[&](){
            if(!vertices.empty()&&!SDL_RenderGeometry(renderer,textures[texture],vertices.data(),int(vertices.size()),nullptr,0))throw std::runtime_error(SDL_GetError());
            vertices.clear();
        };
        for(const auto&face:faces) {
            if(face.texture!=texture){flush();texture=face.texture;}
            for(const auto&v:face.vertices)vertices.push_back({{cx+focal*v.position.x/v.position.z,cy+focal*v.position.y/v.position.z},{1,1,1,1},{v.u,v.v}});
        }
        flush();
    }
    void close(){depthRenderer.close();sky.close();for(auto*t:textures)SDL_DestroyTexture(t);textures.clear();}
};
