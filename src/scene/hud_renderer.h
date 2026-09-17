#pragma once
#include <unordered_map>
#include "texture_data.h"
#include "gp0_commands.h"
#include "hud_layout.h"
struct HudRenderer {
    std::unordered_map<uint32_t,SDL_Texture*>textures;
    void updateVram(const std::vector<uint16_t>&oldWords,const std::vector<uint16_t>&words){
        TextureSignatures before{oldWords,{}},after{words,{}};updateVram(before,after);
    }
    void updateVram(TextureSignatures&before,TextureSignatures&after){
        const auto&words=after.vram;
        for(auto&entry:textures){unsigned page=entry.first&65535,clut=entry.first>>16;
            if(before.get(page,clut)==after.get(page,clut))continue;
            auto rgba=texturePixels(words,page,clut);
            if(!SDL_UpdateTexture(entry.second,nullptr,rgba.data(),1024))throw std::runtime_error(SDL_GetError());
        }
    }
    SDL_Texture*texture(SDL_Renderer*r,const std::vector<uint16_t>&vram,uint16_t page,uint16_t clut){
        uint32_t key=uint32_t(clut)<<16|page;
        auto found=textures.find(key);if(found!=textures.end())return found->second;
        unsigned mode=(page>>7)&3;if(mode==3||vram.size()!=524288)return nullptr;
        // Bound texture cache even during long sessions with changing palettes.
        if(textures.size()>=256)close();
        std::vector<uint8_t>rgba(256*256*4);unsigned cx=(clut&63)*16,cy=(clut>>6)&511;
        for(unsigned y=0;y<256;y++)for(unsigned x=0;x<256;x++){
            uint16_t word=vram[((((page>>4)&1)*256+y)&511)*1024+(((page&15)*64+(x>>(2-mode)))&1023)];
            if(mode<2){unsigned index=(word>>((x&((1u<<(2-mode))-1))*(4u<<mode)))&((1u<<(4u<<mode))-1);word=vram[cy*1024+((cx+index)&1023)];}
            for(int c=0;c<3;c++){unsigned v=(word>>(c*5))&31;rgba[(y*256+x)*4+c]=(v<<3)|(v>>2);}
            rgba[(y*256+x)*4+3]=word?255:0;
        }
        SDL_Texture*t=SDL_CreateTexture(r,SDL_PIXELFORMAT_RGBA32,SDL_TEXTUREACCESS_STATIC,256,256);
        if(!t||!SDL_UpdateTexture(t,nullptr,rgba.data(),1024))throw std::runtime_error(SDL_GetError());
        SDL_SetTextureScaleMode(t,SDL_SCALEMODE_NEAREST);SDL_SetTextureBlendMode(t,SDL_BLENDMODE_BLEND);textures.emplace(key,t);return t;
    }
    void drawBackground(SDL_Renderer*r,const Frame&f,const std::vector<uint16_t>&vram,int width,int height){
        SDL_SetRenderClipRect(r,nullptr);
        for(const auto&q:f.background){
            SDL_Texture*t=q.textured?texture(r,vram,q.page,q.clut):nullptr;
            if(q.textured&&!t)continue;
            SDL_Vertex vertices[4];float divisor=q.textured?128.f:255.f;
            for(int i=0;i<4;i++)vertices[i]={{q.xy[i][0]*width/320.f,q.xy[i][1]*height/240.f},{q.rgb[i][0]/divisor,q.rgb[i][1]/divisor,q.rgb[i][2]/divisor,1},{q.uv[i][0],q.uv[i][1]}};
            int indices[6]={0,1,2,2,1,3};SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
            if(!SDL_RenderGeometry(r,t,vertices,4,indices,6))throw std::runtime_error(SDL_GetError());
        }
    }
    void draw(SDL_Renderer*r,const Frame&f,const std::vector<uint16_t>&vram,int width,int height,int displayX=0,int displayY=0,bool fillWidth=false,bool omitMirrorBackground=false){
        SDL_SetRenderClipRect(r,nullptr);int clipLeft=0,clipTop=0,clipRight=319,clipBottom=239;bool explicitClip=false,clipDirty=false;float clipShift=0;
        uint16_t page=0;float scale=height/240.f,xscale=fillWidth?width/320.f:scale,left=fillWidth?0:(width-320*scale)/2;
        auto color=[](uint32_t rgb,float divisor){return SDL_FColor{(rgb&255)/divisor,((rgb>>8)&255)/divisor,((rgb>>16)&255)/divisor,1};};
        // DMA packets may bundle a texture-page command, sprites and lines.
        // Split GP0 commands before decoding; a packet is not one primitive.
        auto commands=splitDrawingCommands(f.hud);
        for(size_t offset=0;offset<commands.size();){
            unsigned n=commands[offset++];if(!n||n>commands.size()-offset)break;
            const uint32_t*p=commands.data()+offset;offset+=n;unsigned cmd=p[0]>>24,type=cmd&0xfc;
            if(cmd>=0xe0){
                for(unsigned i=0;i<n;i++){
                    auto code=p[i]>>24;
                    if(code==0xe1)page=p[i]&0x1ff;
                    if(code==0xe3){clipLeft=int(p[i]&1023)-displayX;clipTop=int((p[i]>>10)&511)-displayY;}
                    if(code==0xe4){clipRight=int(p[i]&1023)-displayX;clipBottom=int((p[i]>>10)&511)-displayY;}
                    if(code==0xe3||code==0xe4){explicitClip=true;clipDirty=true;SDL_Rect clip{int(left+clipLeft*xscale),int(clipTop*scale),std::max(0,int((clipRight-clipLeft+1)*xscale)),std::max(0,int((clipBottom-clipTop+1)*scale))};SDL_SetRenderClipRect(r,&clip);}
                }
                continue;
            }
            const float shift=revolutionHudShift(p,n,page,f.flags,width,height,fillWidth);
            // Translate the draw area with its anchored group. Otherwise an
            // original 4:3 scissor can cut off HUD elements in the wider margins.
            if(clipDirty||shift!=clipShift){
                if(explicitClip||shift!=0){SDL_Rect clip{int(left+clipLeft*xscale+shift),int(clipTop*scale),std::max(0,int((clipRight-clipLeft+1)*xscale)),std::max(0,int((clipBottom-clipTop+1)*scale))};SDL_SetRenderClipRect(r,&clip);}
                else SDL_SetRenderClipRect(r,nullptr);
                clipShift=shift;clipDirty=false;
            }
            SDL_Vertex v[4];int count=4;SDL_Texture*t=nullptr;
            auto xy=[&](uint32_t packed){return SDL_FPoint{left+shift+int16_t(packed)*xscale,int16_t(packed>>16)*scale};};
            if((type==0x64&&n==4)||((type==0x74||type==0x7c)&&n==3)){
                unsigned w=type==0x64?(p[3]&0xffff):(type==0x74?8:16),h=type==0x64?(p[3]>>16):w;
                float u=p[2]&255,texY=(p[2]>>8)&255;auto origin=xy(p[1]);
                t=texture(r,vram,page,p[2]>>16);if(!t)continue;
                for(int i=0;i<4;i++)v[i]={{origin.x+(i&1?w*xscale:0),origin.y+(i&2?h*scale:0)},color(p[0],cmd&1?255:128),{(u+(i&1?w:0))/256,(texY+(i&2?h:0))/256}};
                if(cmd&1)for(auto&vertex:v)vertex.color={1,1,1,1};
            }else if((type==0x2c&&n==9)||(type==0x24&&n==7)){
                count=type==0x24?3:4;
                page=(p[4]>>16)&0x1ff;t=texture(r,vram,page,p[2]>>16);if(!t)continue;
                for(int i=0;i<count;i++){uint32_t uv=p[2+i*2];v[i]={xy(p[1+i*2]),cmd&1?SDL_FColor{1,1,1,1}:color(p[0],128),{(uv&255)/256.f,((uv>>8)&255)/256.f}};}
                // PS1 rectangle tiles sample integer texels; SDL samples pixel
                // centres. A reversed one-to-one UV span otherwise starts one
                // texel early and leaks the neighbouring atlas tile at its end.
                // Limit this correction to axis-aligned, unscaled FT4 tiles.
                if(count==4){
                    int x[4],y[4],u[4],tv[4];
                    for(int i=0;i<4;i++){x[i]=int16_t(p[1+i*2]);y[i]=int16_t(p[1+i*2]>>16);u[i]=p[2+i*2]&255;tv[i]=(p[2+i*2]>>8)&255;}
                    int w=x[1]-x[0],h=y[2]-y[0],du=u[1]-u[0],dv=tv[2]-tv[0];
                    if(w>0&&h>0&&x[0]==x[2]&&x[1]==x[3]&&y[0]==y[1]&&y[2]==y[3]&&
                       u[0]==u[2]&&u[1]==u[3]&&tv[0]==tv[1]&&tv[2]==tv[3]&&std::abs(du)==w&&std::abs(dv)==h)
                        for(auto&vertex:v){if(du<0)vertex.tex_coord.x+=1.f/256;if(dv<0)vertex.tex_coord.y+=1.f/256;}
                }
            }else if((type==0x28&&n==5)||(type==0x20&&n==4)){
                count=type==0x20?3:4;for(int i=0;i<count;i++)v[i]={xy(p[i+1]),color(p[0],255),{0,0}};
            }else if((type==0x38&&n==8)||(type==0x30&&n==6)){
                count=type==0x30?3:4;for(int i=0;i<count;i++)v[i]={xy(p[i*2+1]),color(p[i*2],255),{0,0}};
            }else if(type==0x60&&n==3){
                auto origin=xy(p[1]);for(int i=0;i<4;i++)v[i]={{origin.x+(i&1?(p[2]&0xffff)*xscale:0),origin.y+(i&2?(p[2]>>16)*scale:0)},color(p[0],255),{0,0}};
            }else if(type==0x50||type==0x58){
                // Gouraud lines carry a colour before each endpoint.
                for(unsigned i=2;i+1<n;i+=2){
                    if((p[i]&0xf000f000)==0x50005000)break;
                    auto c=color(p[i-2],255);SDL_SetRenderDrawColorFloat(r,c.r,c.g,c.b,1);
                    auto a=xy(p[i-1]),b=xy(p[i+1]);SDL_RenderLine(r,a.x,a.y,b.x,b.y);
                }
                continue;
            }else if(type==0x40||type==0x48){
                auto c=color(p[0],255);SDL_SetRenderDrawColorFloat(r,c.r,c.g,c.b,1);
                std::vector<SDL_FPoint> points;for(unsigned i=1;i<n;i++){if((p[i]&0xf000f000)==0x50005000)break;points.push_back(xy(p[i]));}
                if(points.size()>1)SDL_RenderLines(r,points.data(),points.size());continue;
            }else continue;
            if(omitMirrorBackground&&!t&&count==4){
                bool inside=true;for(int i=0;i<4;i++)inside=inside&&v[i].position.x>=left+72*scale&&v[i].position.x<=left+248*scale&&v[i].position.y>=16*scale&&v[i].position.y<=56*scale;
                if(inside||(clipLeft==72&&clipRight==247&&clipTop==16&&clipBottom==55))continue;
            }
            // Average transparency covers the HUD's basic fades. Other PS1
            // semitransparency equations remain part of renderer fidelity work.
            if(cmd&2)for(int i=0;i<count;i++)v[i].color.a=.5f;
            int indices[6]={0,1,2,2,1,3};SDL_SetRenderDrawBlendMode(r,SDL_BLENDMODE_BLEND);
            if(omitMirrorBackground&&!t&&count==4&&clipLeft==0&&clipRight==319){
                SDL_Rect regions[]={{0,0,width,int(16*scale)},{0,int(56*scale),width,height-int(56*scale)},{0,int(16*scale),int(left+72*scale),int(40*scale)},{int(left+248*scale),int(16*scale),width-int(left+248*scale),int(40*scale)}};
                for(auto region:regions){SDL_SetRenderClipRect(r,&region);if(!SDL_RenderGeometry(r,t,v,count,indices,6))throw std::runtime_error(SDL_GetError());}
                SDL_Rect clip{int(left),0,int(320*scale),int(240*scale)};SDL_SetRenderClipRect(r,&clip);clipDirty=true;
            }else if(!SDL_RenderGeometry(r,t,v,count,indices,count==3?3:6))throw std::runtime_error(SDL_GetError());
        }
        SDL_SetRenderClipRect(r,nullptr);
    }
    void close(){for(auto&entry:textures)SDL_DestroyTexture(entry.second);textures.clear();}
};
