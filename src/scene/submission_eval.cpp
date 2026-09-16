/* Bounded read-only evaluation of Revolution's rendering submissions.
 * Guest CPU/RAM, GPU, I/O and clocks are never modified. Only local stack and
 * scratch memory writes are accepted. Visibility changes affect this copy. */
#include "shared.h"
#include "cpu_state.h"
#include <array>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <cstring>
#include <cstdio>
#include <algorithm>
namespace {
using Mat=std::array<int16_t,9>;
struct Eval {
 const uint8_t*ram;std::array<uint8_t,1024>scratch{};std::array<uint8_t,2048>stack{};
 uint32_t r[32]{},pc,next,hi=0,lo=0;int pending=-1;uint32_t pendingValue=0;
 Mat rotation{};int32_t translation[3]{};uint32_t who,bank;bool full,scenery;
 std::vector<RRVModel>output;
 Eval(const uint8_t*source,const uint8_t*spad,const CPUState*cpu,uint32_t entry,bool all):ram(source),pc(entry),next(entry+4),who(cpu->gpr[4]),bank(0),full(all),scenery(entry==0x80036934){
  std::memcpy(r,cpu->gpr,sizeof r);r[31]=0;r[29]=0x0fff0800;
  if(spad)std::memcpy(scratch.data(),spad,1024);
  bank=read(0x1f80000c,4);
  if(full&&!scenery)r[5]=1; // RenderCar's uncullled detailed submission path.
 }
 uint32_t read(uint32_t a,unsigned n){
  a&=0x1fffffff;uint32_t value=0;
  uint32_t mask=0;if(full&&scenery)std::memcpy(&mask,ram+0x1de2c0,4);
  if(mask&&a>=(mask&0x1fffffff)&&a<(mask&0x1fffffff)+128&&n==4)return ~0u;
  const uint8_t*p;
  if(a>=0x0fff0000&&a+n<=0x0fff0800)p=stack.data()+a-0x0fff0000;
  else if(a>=0x1f800000&&a+n<=0x1f800400)p=scratch.data()+a-0x1f800000;
  else if(a+n<=2097152)p=ram+a;
  else throw std::runtime_error("submission read bounds");
  std::memcpy(&value,p,n);return value;
 }
 void write(uint32_t a,uint32_t v,unsigned n){
  a&=0x1fffffff;uint8_t*p;
  if(a>=0x0fff0000&&a+n<=0x0fff0800)p=stack.data()+a-0x0fff0000;
  else if(a>=0x1f800000&&a+n<=0x1f800400)p=scratch.data()+a-0x1f800000;
  else throw std::runtime_error("submission attempted nonlocal write");
  std::memcpy(p,&v,n);
 }
 Mat matrix(uint32_t a){Mat m;for(int i=0;i<9;i++)m[i]=int16_t(read(a+i*2,2));return m;}
 void matrix(uint32_t a,const Mat&m){for(int i=0;i<9;i++)write(a+i*2,uint16_t(m[i]),2);}
 Mat multiply(const Mat&a,const Mat&b){Mat m;for(int i=0;i<3;i++)for(int j=0;j<3;j++){int64_t sum=0;for(int k=0;k<3;k++)sum+=int(a[i*3+k])*b[k*3+j];m[i*3+j]=int16_t(std::clamp<int64_t>(sum>>12,-32768,32767));}return m;}
 int sine(int32_t angle){int sign=angle<0?-1:1;unsigned a=unsigned(angle<0?-int64_t(angle):angle)&4095;if(a>=2048){a-=2048;sign=-sign;}if(a>=1024)a=2047-a;return sign*int16_t(read(0x80075bcc+a*2,2));}
 bool helper(){
  if(pc==0x80020d88){r[2]=0;return true;} // Lighting bookkeeping, no geometry.
  if(pc==0x8005276c||pc==0x800527dc||pc==0x8005284c){
   int s=sine(int32_t(r[5])),c=sine(int64_t(int32_t(r[5]))<0?int32_t(-int64_t(int32_t(r[5]))+1024):int32_t(r[5]+1024));
   Mat m={4096,0,0,0,4096,0,0,0,4096};int a=pc==0x8005284c?1:0,b=pc==0x8005276c?1:2;m[a*3+a]=m[b*3+b]=c;m[a*3+b]=-s;m[b*3+a]=s;matrix(r[4],m);return true;
  }
  if(pc==0x8005c3fc||pc==0x8005c2f0){uint32_t dest=pc==0x8005c3fc?r[5]:r[4];matrix(dest,multiply(matrix(r[4]),matrix(r[5])));r[2]=dest;return true;}
  if(pc==0x8005c508||pc==0x8005cd68){
   auto m=matrix(r[4]);int32_t v[3];for(int i=0;i<3;i++)v[i]=pc==0x8005c508?int16_t(read(r[5]+i*2,2)):int32_t(read(r[5]+i*4,4));
   for(int i=0;i<3;i++){int64_t sum=0;for(int k=0;k<3;k++)sum+=int64_t(m[i*3+k])*v[k];write(r[6]+i*4,uint32_t(sum>>12),4);}return true;
  }
  if(pc==0x80042ac0){
   rotation=matrix(r[6]);auto camera=matrix(0x801f9a5c);int32_t v[3];
   for(int i=0;i<3;i++){v[i]=int32_t(read(r[5]+i*4,4)-read(0x801de360+i*4,4));if(!full)v[i]=int16_t(v[i]);}
   for(int i=0;i<3;i++){int64_t sum=0;for(int k=0;k<3;k++)sum+=int64_t(camera[i*3+k])*v[k];translation[i]=int32_t(sum>>12)*4;write(r[4]+44+i*4,translation[i],4);}return true;
  }
  if(pc==0x80053d24||pc==0x80054654||pc==0x80054914||pc==0x800552f8||pc==0x80055a58){
   RRVModel m{};m.owner=scenery?r[18]:who;m.site=r[31];m.model=r[5]+(scenery?119:0);m.part=m.site==0x8001bd34?r[17]+1:0;
   m.pass=read(r[4]+48,4)?1:0;m.palette=read(r[4]+28,4);
   if(m.model>252)throw std::runtime_error("submission model bounds");
   for(int i=0;i<9;i++)m.rotation[i/2]|=uint32_t(uint16_t(rotation[i]))<<((i%2)*16);
   std::memcpy(m.translation,translation,sizeof translation);output.push_back(m);
   if(output.size()>RRV_MODEL_CAP)throw std::runtime_error("submission capacity");return true;
  }
  return false;
 }
 void run(){
  for(unsigned steps=0;pc;steps++){
   if(steps>=50000)throw std::runtime_error("scenery instruction budget");
   if(helper()){pc=r[31];next=pc+4;continue;}
   if(!((pc>=0x8001b660&&pc<0x8001be2c)||(pc>=0x80036934&&pc<0x80036c78)))throw std::runtime_error("submission call boundary: "+std::to_string(pc));
   uint32_t w=read(pc,4),op=w>>26,s=(w>>21)&31,t=(w>>16)&31,d=(w>>11)&31,a=(w>>6)&31,fn=w&63;
   uint32_t x=r[s],y=r[t],nn=next+4;int32_t imm=int16_t(w);int old=pending,written=-1;uint32_t value=pendingValue;pending=-1;
   auto reg=[&](unsigned i,uint32_t v){if(i){r[i]=v;written=i;}};
   auto branch=[&](bool yes){if(yes)nn=pc+4+imm*4;};
   if(op==0){switch(fn){
    case 0:reg(d,y<<a);break;case 2:reg(d,y>>a);break;case 3:reg(d,int32_t(y)>>a);break;
    case 4:reg(d,y<<(x&31));break;case 6:reg(d,y>>(x&31));break;case 7:reg(d,int32_t(y)>>(x&31));break;
    case 8:nn=x;break;case 9:nn=x;reg(d,pc+8);break;case 16:reg(d,hi);break;case 18:reg(d,lo);break;
    case 24:case 25:{uint64_t v=fn==24?uint64_t(int64_t(int32_t(x))*int32_t(y)):uint64_t(x)*y;lo=v;hi=v>>32;break;}
    case 26:case 27:{int64_t xx=fn==26?int64_t(int32_t(x)):int64_t(x),yy=fn==26?int64_t(int32_t(y)):int64_t(y);if(!yy)throw std::runtime_error("scenery divide by zero");lo=xx/yy;hi=xx%yy;break;}
    case 32:case 33:reg(d,x+y);break;case 34:case 35:reg(d,x-y);break;
    case 36:reg(d,x&y);break;case 37:reg(d,x|y);break;case 38:reg(d,x^y);break;case 39:reg(d,~(x|y));break;
    case 42:reg(d,int32_t(x)<int32_t(y));break;case 43:reg(d,x<y);break;
    default:throw std::runtime_error("scenery SPECIAL");}
   }else switch(op){
    case 1:if(t>1)throw std::runtime_error("scenery branch");branch(t?int32_t(x)>=0:int32_t(x)<0);break;
    case 2:case 3:nn=((pc+4)&0xf0000000)|((w&0x3ffffff)<<2);if(op==3)reg(31,pc+8);break;
    case 4:branch(x==y);break;case 5:branch(x!=y);break;case 6:branch(int32_t(x)<=0);break;case 7:branch(int32_t(x)>0);break;
    case 8:case 9:reg(t,x+imm);break;case 10:reg(t,int32_t(x)<imm);break;case 11:reg(t,x<uint32_t(imm));break;
    case 12:reg(t,x&(w&65535));break;case 13:reg(t,x|(w&65535));break;case 14:reg(t,x^(w&65535));break;case 15:reg(t,w<<16);break;
    case 32:case 33:case 35:case 36:case 37:{unsigned n=op==35?4:(op==33||op==37)?2:1;uint32_t v=read(x+imm,n);if(op==32)v=int8_t(v);if(op==33)v=int16_t(v);pending=t;pendingValue=v;break;}
    case 40:case 41:case 43:write(x+imm,y,op==43?4:op==41?2:1);break;
    default:throw std::runtime_error("scenery opcode");
   }
   if(old>0&&old!=written)r[old]=value;r[0]=0;pc=next;next=nn;
  }
 }
};
}
extern "C" int revolution_evaluate(const uint8_t*ram,const uint8_t*scratch,const CPUState*cpu,uint32_t entry,int full,RRVModel*out,unsigned capacity){
 try{Eval e(ram,scratch,cpu,entry,full!=0);e.run();if(e.output.size()>capacity)return -1;std::memcpy(out,e.output.data(),e.output.size()*sizeof *out);return int(e.output.size());}
 catch(const std::exception&e){static unsigned errors;if(errors++<8)std::fprintf(stderr,"Revolution submission evaluator %08x: %s\n",entry,e.what());return -1;}
}
