"""Preserve PS1 texture output while avoiding resident-page invalidation."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
HARNESS = r'''
#include "texture_data.h"
#include <cassert>
#include <chrono>
#include <iostream>
inline std::vector<uint8_t> referencePixels(const std::vector<uint16_t>&vram,unsigned page,unsigned clut,unsigned window=0,const std::vector<uint16_t>*resident=nullptr){
    std::vector<uint8_t>rgba(256*256*4);unsigned mode=(page>>7)&3;
    if(mode>2||vram.size()!=524288)return rgba;
    for(unsigned y=0;y<256;y++)for(unsigned x=0;x<256;x++){
        unsigned maskX=(window&31)*8,maskY=((window>>5)&31)*8;
        unsigned tx=(x&~maskX)|(((window>>10)&31)*8&maskX),ty=(y&~maskY)|(((window>>15)&31)*8&maskY);
        uint16_t word=resident&&mode==0&&resident->size()==16384?(*resident)[ty*64+(tx>>2)]:vram[((((page>>4)&1)*256+ty)&511)*1024+(((page&15)*64+(tx>>(2-mode)))&1023)];
        if(mode<2){unsigned index=(word>>((tx&((1u<<(2-mode))-1))*(4u<<mode)))&((1u<<(4u<<mode))-1);word=vram[((clut>>6)&511)*1024+(((clut&63)*16+index)&1023)];}
        for(int c=0;c<3;c++){unsigned value=(word>>(c*5))&31;rgba[(y*256+x)*4+c]=(value<<3)|(value>>2);}
        rgba[(y*256+x)*4+3]=word?255:0;
    }
    return rgba;
}

int main(){
 std::vector<uint16_t> ram(524288),resident(16384);uint32_t seed=17;
 for(auto&x:ram){seed=seed*1664525+1013904223;x=seed>>8;}
 for(auto&x:resident){seed=seed*1664525+1013904223;x=seed>>8;}
 ram[0]=0;ram[1]=0x8000;
 std::vector<uint8_t> scratch;
 for(unsigned mode=0;mode<4;mode++)for(unsigned page:{0u,15u,28u,31u})for(unsigned clut:{0u,63u,32767u})for(unsigned window:{0u,0x12345u,0xfffffu})for(bool stored:{false,true}){
  auto*r=stored?&resident:nullptr;unsigned p=page|(mode<<7);
  texturePixelsInto(scratch,ram,p,clut,window,r);
  assert(scratch==referencePixels(ram,p,clut,window,r));
 }
 auto modified=ram;modified[256*1024+12*64]^=123;
 TextureSignatures a{ram,{}},b{modified,{}};
 assert(a.get(28,0,true)==b.get(28,0,true));
 assert(a.get(28,0)!=b.get(28,0));
 modified[0]^=1;TextureSignatures c{modified,{}};
 assert(a.get(28,0,true)!=c.get(28,0,true));
 // Per-page cache must never mix resident and live signatures.
 TextureSignatures fresh{ram,{}};assert(a.get(28,0)==fresh.get(28,0));
 volatile unsigned checksum=0;
 auto run=[&](bool fast){auto start=std::chrono::steady_clock::now();for(int i=0;i<600;i++){
  if(fast)texturePixelsInto(scratch,ram,28,i%64,0,&resident);
  else scratch=referencePixels(ram,28,i%64,0,&resident);
  checksum+=scratch[i%65536];
 }return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();};
 auto oldMs=run(false),newMs=run(true);std::cout<<"old_ms="<<oldMs<<" new_ms="<<newMs<<" ratio="<<oldMs/newMs<<"\n";
}
'''
class TextureTests(unittest.TestCase):
    def test_palette_expansion_and_resident_signatures(self):
        with tempfile.TemporaryDirectory() as tmp:
            p=Path(tmp);(p/'test.cpp').write_text(HARNESS)
            subprocess.run(['c++','-std=c++17','-O2','-I'+str(ROOT/'src/scene'),str(p/'test.cpp'),'-o',str(p/'test')],check=True,capture_output=True)
            result=subprocess.run([str(p/'test')],check=True,capture_output=True,text=True)
            print(result.stdout.strip())
