#pragma once
#include <unordered_map>
#include <array>
#include <algorithm>
#include <cstring>
#include <vector>
#include <cstdint>
// Cache per-page hashes when many palettes refer to the same packed pixels.
struct TextureSignatures {
    const std::vector<uint16_t>&vram;std::unordered_map<unsigned,uint64_t>pages;
    uint64_t get(unsigned page,unsigned clut,bool resident=false){
        unsigned mode=(page>>7)&3;if(mode>2||vram.size()!=524288)return 0;
        unsigned pageKey=page&0x19f;auto found=pages.find(pageKey);uint64_t hash;
        if(resident&&mode==0)hash=1469598103934665603ull;
        else if(found!=pages.end())hash=found->second;else{
            hash=1469598103934665603ull;
            for(unsigned y=0;y<256;y++)for(unsigned x=0;x<(64u<<mode);x++){
                hash^=vram[((((page>>4)&1)*256+y)&511)*1024+(((page&15)*64+x)&1023)];hash*=1099511628211ull;
            }
            pages.emplace(pageKey,hash);
        }
        if(mode<2)for(unsigned i=0;i<(mode==0?16u:256u);i++){
            hash^=vram[((clut>>6)&511)*1024+(((clut&63)*16+i)&1023)];hash*=1099511628211ull;
        }
        return hash;
    }
};
// Expand palette colours once, rather than doing RGB555 conversion per texel.
// The output buffer can be reused between texture uploads on the render thread.
inline void texturePixelsInto(std::vector<uint8_t>&rgba,const std::vector<uint16_t>&vram,unsigned page,unsigned clut,unsigned window=0,const std::vector<uint16_t>*resident=nullptr){
    rgba.resize(256*256*4);unsigned mode=(page>>7)&3;
    if(mode>2||vram.size()!=524288){std::fill(rgba.begin(),rgba.end(),0);return;}
    auto colour=[](uint16_t word){
        std::array<uint8_t,4> pixel{};
        for(int c=0;c<3;c++){unsigned value=(word>>(c*5))&31;pixel[c]=(value<<3)|(value>>2);}
        pixel[3]=word?255:0;return pixel;
    };
    std::array<std::array<uint8_t,4>,256> palette{};
    if(mode<2)for(unsigned i=0;i<(mode==0?16u:256u);i++)
        palette[i]=colour(vram[((clut>>6)&511)*1024+(((clut&63)*16+i)&1023)]);
    const unsigned maskX=(window&31)*8,maskY=((window>>5)&31)*8;
    const unsigned offsetX=((window>>10)&31)*8&maskX,offsetY=((window>>15)&31)*8&maskY;
    const bool useResident=resident&&mode==0&&resident->size()==16384;
    for(unsigned y=0;y<256;y++){
        unsigned ty=(y&~maskY)|offsetY;
        const uint16_t*row=useResident?resident->data()+ty*64:vram.data()+(((((page>>4)&1)*256+ty)&511)*1024);
        for(unsigned x=0;x<256;x++){
            unsigned tx=(x&~maskX)|offsetX;
            uint16_t word=row[useResident?(tx>>2):(((page&15)*64+(tx>>(2-mode)))&1023)];
            if(mode<2){
                unsigned index=(word>>((tx&((1u<<(2-mode))-1))*(4u<<mode)))&((1u<<(4u<<mode))-1);
                std::memcpy(rgba.data()+(y*256+x)*4,palette[index].data(),4);
            }else{
                auto pixel=colour(word);std::memcpy(rgba.data()+(y*256+x)*4,pixel.data(),4);
            }
        }
    }
}
inline std::vector<uint8_t> texturePixels(const std::vector<uint16_t>&vram,unsigned page,unsigned clut,unsigned window=0,const std::vector<uint16_t>*resident=nullptr){
    std::vector<uint8_t> rgba;texturePixelsInto(rgba,vram,page,clut,window,resident);return rgba;
}
