#pragma once
// USA 0x80018358: tiled cylindrical background, pitched and rolled in screen
// space. The tile/UV tables and palette are extracted from the user's disc.
struct SkyRenderer {
    std::vector<uint16_t>vram;std::array<uint16_t,16>tiles{};std::array<uint16_t,48>uv{};
    SDL_Texture*pages[2]={};uint32_t palette=~0u;
    void load(std::ifstream&in){
        auto word=[&](){uint8_t b[2];in.read((char*)b,2);return uint16_t(b[0]|unsigned(b[1])<<8);};
        vram.resize(524288);for(auto&v:vram)v=word();for(auto&v:tiles)v=word();for(auto&v:uv)v=word();
        for(auto tile:tiles)if(tile>=12)throw std::runtime_error("invalid sky tile");
    }
    void close(){for(auto*&p:pages){if(p)SDL_DestroyTexture(p);p=nullptr;}palette=~0u;}
    void draw(SDL_Renderer*r,const Frame&f,int width,int height){
        if(vram.empty()||!f.sky.enabled)return;
        const auto&s=f.sky;
        if(palette!=s.clut){
            close();palette=s.clut;std::vector<uint8_t>rgba(256*256*4);
            unsigned cx=(s.clut&63)*16,cy=(s.clut>>6)&511;
            for(int page=0;page<2;page++){
                for(unsigned y=0;y<256;y++)for(unsigned x=0;x<256;x++){
                    unsigned word=vram[(256+y)*1024+(5+page)*64+x/4];
                    uint16_t color=vram[cy*1024+((cx+((word>>((x%4)*4))&15))&1023)];
                    for(int c=0;c<3;c++){unsigned v=(color>>(c*5))&31;rgba[(y*256+x)*4+c]=(v<<3)|(v>>2);}
                    rgba[(y*256+x)*4+3]=color?255:0;
                }
                pages[page]=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,256,256);
                if(!pages[page]||!SDL_UpdateTexture(pages[page],nullptr,rgba.data(),1024))throw std::runtime_error(SDL_GetError());
                SDL_SetTextureScaleMode(pages[page],SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(pages[page],SDL_BLENDMODE_BLEND);
            }
        }
        const float scale=height/240.f;
        float yaw=std::fmod((s.mirror?-s.yaw:s.yaw)+512,4096.f);if(yaw<0)yaw+=4096;
        int base=int(std::floor(yaw/128));float fraction=std::fmod(yaw/2,64.f);
        float pitch=std::remainder(s.pitch,4096.f)+2+f.camera.y/16;
        float top=-128-pitch/2;
        float angle=(s.mirror?s.roll:-s.roll)*6.28318530718f/4096,co=std::cos(angle),si=std::sin(angle);
        SDL_FColor tint{(s.rgb&255)/128.f,((s.rgb>>8)&255)/128.f,((s.rgb>>16)&255)/128.f,1};
        int radius=int(width/scale/64)+10;
        for(int j=-radius;j<=radius;j++)for(int upper=1;upper>=0;upper--){
            unsigned tile=tiles[(base+j+(upper?0:3))&15];float x=-256-fraction+j*64;
            SDL_Vertex vertices[4];
            for(int i=0;i<4;i++){
                float px=x+(i&1?64:0),py=top+(i&2?(upper?-128:128):0);
                unsigned tex=uv[tile*4+(upper?(i&1):i)];
                vertices[i]={{width/2.f+(co*px+si*py)*scale,height/2.f+(-si*px+co*py)*scale},tint,{(tex&255)/256.f,(tex>>8)/256.f}};
            }
            int indices[6]={0,1,2,2,1,3};
            if(!SDL_RenderGeometry(r,pages[tile>=8],vertices,4,indices,6))throw std::runtime_error(SDL_GetError());
        }
    }
};
