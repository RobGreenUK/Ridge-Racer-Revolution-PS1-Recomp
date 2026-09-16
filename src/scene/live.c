/* Nonblocking local scene transport. Simulation state is read-only;
 * viewer input is applied only at the verified digital controller boundary. */
#include "shared.h"
#include "cpu_state.h"
#include "mod_plugins.h"
#include "psx_cycles.h"
#include "gpu.h"
#include "gpu_render.h"
#include <sys/file.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static int fd=-1;static struct RRVShared*shared;static struct RRVSnapshot frame;
extern uint8_t*memory_get_ram_ptr(void);
extern uint8_t*memory_get_scratchpad_ptr(void);
extern int revolution_evaluate(const uint8_t*,const uint8_t*,const CPUState*,uint32_t,int,struct RRVModel*,unsigned);
static int car_distance=0;
static int full_scene=1,scenery_owned[2];
static int audit_enabled;static unsigned audit_checks,audit_errors,reference_count;
static struct RRVModel reference[RRV_MODEL_CAP];
static void audit_reference(const CPUState*cpu,uint32_t entry){
    if(!audit_enabled)return;
    int n=revolution_evaluate(memory_get_ram_ptr(),memory_get_scratchpad_ptr(),cpu,entry,0,reference+reference_count,RRV_MODEL_CAP-reference_count);
    if(n>0)reference_count+=n;
}
static void audit_model(const struct RRVModel*m){
    if(!audit_enabled)return;
    for(unsigned j=0;j<reference_count;j++){
        struct RRVModel*r=&reference[j];if(r->owner!=m->owner||r->site!=m->site||r->model!=m->model||r->part!=m->part||r->pass!=m->pass)continue;
        int same=r->palette==m->palette;
        for(unsigned i=0;i<9;i++)same&=(uint16_t)(r->rotation[i/2]>>((i%2)*16))==(uint16_t)(m->rotation[i/2]>>((i%2)*16));
        for(unsigned i=0;i<3;i++)same&=r->translation[i]==m->translation[i];
        audit_checks++;if(!same&&audit_errors++<8)fprintf(stderr,"submission mismatch owner=%08x site=%08x model=%u actual=%d,%d,%d expected=%d,%d,%d\n",m->owner,m->site,m->model,(int)m->translation[0],(int)m->translation[1],(int)m->translation[2],(int)r->translation[0],(int)r->translation[1],(int)r->translation[2]);
        *r=reference[--reference_count];break;
    }
}
static void audit_report(void){if(audit_enabled)fprintf(stderr,"Revolution submission audit: %u checks, %u mismatches\n",audit_checks,audit_errors);}

static uint32_t owner,ready_state,ready_course;static int scene_ready;static char path[1024];
extern const uint16_t*gpu_get_vram(void);
static uint64_t now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return(uint64_t)t.tv_sec*1000000000+t.tv_nsec;}
static void cleanup(void){if(shared)munmap(shared,sizeof *shared);if(fd>=0){close(fd);unlink(path);}shared=NULL;fd=-1;}
static int ram(uint32_t a,unsigned n){uint32_t p=a&0x1fffffff;return p<=0x200000 && n<=0x200000-p;}
static int course(uint32_t base){
    if(!ram(base,2072)||psx_mod_read_word(base)!=2072)return -1;
    uint32_t size=psx_mod_read_word(base+4);
    switch(size){case 0x3fe48:return 0;case 0x42f70:return 1;case 0x47810:return 2;case 0x431a8:return 3;default:return -1;}
}
static unsigned packets(uint32_t addr,uint32_t stop,uint32_t*out,unsigned cap){
    unsigned count=0;
    for(unsigned links=0;links<2048;links++){
        if((addr&0xffffff)==0xffffff||addr==stop)return count;
        addr&=0x1fffffff;if(!ram(addr,4)||(addr&3))return 0;
        uint32_t tag=psx_mod_read_word(addr),n=tag>>24;
        if(!ram(addr,4+n*4)||count+n+1>cap)return 0;
        if(n){out[count++]=n;for(unsigned i=0;i<n;i++)out[count++]=psx_mod_read_word(addr+4+i*4);}
        addr=tag&0xffffff;
    }
    return 0;
}
static int mirror_enabled(uint32_t addr,unsigned y){
    for(unsigned links=0;links<2048;links++){
        if((addr&0xffffff)==0xffffff)return 0;
        addr&=0x1fffffff;if(!ram(addr,4)||(addr&3))return 0;
        uint32_t tag=psx_mod_read_word(addr),n=tag>>24;
        if(!ram(addr,4+n*4))return 0;
        if(n>=2){uint32_t a=psx_mod_read_word(addr+4),b=psx_mod_read_word(addr+8);
            if(a>>24==0xe3&&b>>24==0xe4&&(a&1023)==72&&(b&1023)==247&&((a>>10)&511)==y+16&&((b>>10)&511)==y+55)return 1;
        }
        addr=tag&0xffffff;
    }
    return 0;
}
static void complete_cars(const CPUState*cpu){
    // The eleven opponent records continue to carry authoritative positions
    // even when the original render pass stops submitting their geometry.
    for(unsigned i=0;i<11;i++){
        uint32_t who=0x801F9B18+i*0x118;
        if(!psx_mod_read_half(who)||psx_mod_read_half(who+2)>=16)continue;
        // RenderCar 8001B778 uses abs(dx)+abs(dz) < 0xd00.
        int64_t dx=(int64_t)(int32_t)psx_mod_read_word(who+16)-(int32_t)psx_mod_read_word(0x801DE360);
        int64_t dz=(int64_t)(int32_t)psx_mod_read_word(who+24)-(int32_t)psx_mod_read_word(0x801DE368);
        if(car_distance>0&&llabs(dx)+llabs(dz)>=3328*car_distance)continue;
        CPUState local=*cpu;local.gpr[4]=who;local.gpr[5]=1;
        struct RRVModel parts[16];int n=revolution_evaluate(memory_get_ram_ptr(),memory_get_scratchpad_ptr(),&local,0x8001B660,1,parts,16);
        if(n<=0)continue;
        unsigned keep=0;for(unsigned j=0;j<frame.model_count;j++)if(frame.models[j].owner!=who)keep++;
        if(keep+(unsigned)n>RRV_MODEL_CAP)continue;
        keep=0;for(unsigned j=0;j<frame.model_count;j++)if(frame.models[j].owner!=who)frame.models[keep++]=frame.models[j];
        for(int j=0;j<n;j++){parts[j].pass=0;frame.models[keep++]=parts[j];}frame.model_count=keep;
    }
}
static void boundary(const CPUState*cpu){
    frame.cycles=psx_cycle_count;frame.sequence++;frame.state=psx_mod_read_half(0x801DD0BC);
    uint32_t base=psx_mod_read_word(0x801DD200);int id=course(base);frame.course=id<0?~0u:(uint32_t)id;
    int candidate=(frame.state==17||frame.state==19)&&id>=0;
    if(!candidate||ready_state!=frame.state||ready_course!=frame.course)scene_ready=0;
    ready_state=frame.state;ready_course=frame.course;
    if(candidate&&frame.model_count)scene_ready=1;
    frame.valid=candidate&&scene_ready;frame.full_scene=full_scene;
    if(frame.valid&&car_distance!=1)complete_cars(cpu);
    for(unsigned i=0;i<3;i++)frame.camera[i]=(int32_t)psx_mod_read_word(0x801DE360+i*4);
    for(unsigned i=0;i<9;i++)frame.matrix[i]=(int16_t)psx_mod_read_half(0x801F9A5C+i*2);
    frame.hud_count=frame.sky_count=0;frame.screen_width=frame.screen_height=0;frame.mirror_enabled=0;
    GpuDisplayInfo info;gpu_get_display_info(&info);frame.display_x=info.display_x;frame.display_y=info.display_y;
    for(unsigned i=0;i<9;i++)frame.mirror_matrix[i]=(i/3==1?1:-1)*frame.matrix[i];
    if(frame.valid){
        uint32_t env=psx_mod_read_word(0x80194F68)&0x1fffffff;
        if(ram(env,0x16cc)){
            // The queued DRAWENV targets the other framebuffer. Clip commands
            // use that origin, not the currently displayed DISPENV origin.
            frame.display_x=psx_mod_read_half(env);frame.display_y=psx_mod_read_half(env+2);
            frame.hud_count=packets(env+0xcc+8,~0u,frame.hud,RRV_HUD_CAP);
            // The minimap is in slot 2. Mirror lettering is in the second OT's
            // near list, composited after the main HUD by the original game.
            frame.hud_count+=packets(env+0xbcc+4,~0u,frame.hud+frame.hud_count,RRV_HUD_CAP-frame.hud_count);
            frame.mirror_enabled=mirror_enabled(env+0xbcc+703*4,frame.display_y);
            frame.sky_count=packets(env+0xcc+703*4,env+0xcc+702*4,frame.sky,RRV_SKY_CAP);
        }
    }else{
        if(!info.disabled&&!info.depth24&&info.width&&info.width<=640&&info.height&&info.height<=512){
            gr_render_display(frame.screen,info.width*4,info.display_x,info.display_y,info.width,info.height);
            frame.screen_width=info.width;frame.screen_height=info.height;
        }
    }
    const uint16_t*vram=gpu_get_vram();if(vram)memcpy(frame.vram,vram,sizeof frame.vram);
    uint64_t published=now();
    if(flock(fd,LOCK_EX|LOCK_NB)==0){shared->published_ns=published;memcpy(&shared->frame,&frame,sizeof frame);flock(fd,LOCK_UN);}
    reference_count=0;frame.model_count=0;owner=0;scenery_owned[0]=scenery_owned[1]=0;
}
static void hook(CPUState*cpu,uint32_t address){
    if(address==0x80040194){
        if(cpu->gpr[31]!=0x80019E70||getenv("REVOLUTION_TEST_INPUT"))return;
        if(flock(fd,LOCK_SH|LOCK_NB)!=0)return;
        uint64_t stamp=shared->input_ns;unsigned active=shared->input_active,buttons=shared->input_buttons;
        flock(fd,LOCK_UN);uint64_t time=now();
        if(!active||buttons>65535||stamp>time||time-stamp>250000000)return;
        psx_mod_write_byte(0x801DC904,0);psx_mod_write_byte(0x801DC905,0x41);
        psx_mod_write_byte(0x801DC906,buttons&255);psx_mod_write_byte(0x801DC907,buttons>>8);return;
    }
    if(address==0x80036934){
        unsigned state=psx_mod_read_half(0x801DD0BC);if(!full_scene||(state!=17&&state!=19))return;
        audit_reference(cpu,address);
        struct RRVModel models[RRV_MODEL_CAP];int n=revolution_evaluate(memory_get_ram_ptr(),memory_get_scratchpad_ptr(),cpu,address,1,models,RRV_MODEL_CAP);
        unsigned pass=psx_mod_read_word(0x1F800030)?1:0;
        if(n>=0&&frame.model_count+(unsigned)n<=RRV_MODEL_CAP){memcpy(frame.models+frame.model_count,models,n*sizeof models[0]);frame.model_count+=n;scenery_owned[pass]=1;}return;
    }
    if(address==0x800232C4){owner=0;return;}
    if(address==0x8001B660){owner=cpu->gpr[4];unsigned st=psx_mod_read_half(0x801DD0BC);if(st==17||st==19)audit_reference(cpu,address);return;}
    if(address==0x80057578){if(cpu->gpr[31]==0x80019D00)boundary(cpu);return;}
    unsigned state=psx_mod_read_half(0x801DD0BC);if(state!=17&&state!=19)return;
    unsigned ctx=cpu->gpr[4],index=cpu->gpr[5];
    if(index>4096||frame.model_count>=RRV_MODEL_CAP)return;
    uint32_t bank=psx_mod_read_word(ctx+12),base=psx_mod_read_word(0x801DD200);int id=course(base);if(id<0)return;
    if(bank==base+psx_mod_read_word(base+4)+4){unsigned count=psx_mod_read_word(bank-4);if(index>=count||count>134)return;index+=119;}
    else{
        if(!ram(bank-4,8)||psx_mod_read_word(bank-4)!=119||index>=119)return;
        uint32_t first=psx_mod_read_word(bank);if(!ram(first,28)||psx_mod_read_word(first)!=0x00680003)return;
    }
    struct RRVModel*m=&frame.models[frame.model_count++];m->owner=owner;m->site=cpu->gpr[31];m->model=index;
    if(m->site==0x80036B08)m->owner=cpu->gpr[18];
    // RenderCar loops over four wheels at one call site; s1 is the corner index.
    m->part=m->site==0x8001BD34?cpu->gpr[17]+1:0;
    m->pass=psx_mod_read_word(ctx+48)?1:0;m->palette=psx_mod_read_word(ctx+28);
    for(unsigned i=0;i<5;i++)m->rotation[i]=cpu->gte_ctrl[i];
    for(unsigned i=0;i<3;i++)m->translation[i]=cpu->gte_ctrl[5+i];
    audit_model(m);
    if(m->site==0x80036B08&&scenery_owned[m->pass]){frame.model_count--;return;}
    if(m->pass)for(unsigned i=0;i<9;i++)frame.mirror_matrix[i]=(int16_t)psx_mod_read_half(0x801F9A5C+i*2);
}
PSX_MOD_CONSTRUCTOR(register_revolution_live){
    const char*p=getenv("REVOLUTION_SCENE_FILE");if(!p||!*p||strlen(p)>=sizeof path)return;
    audit_enabled=getenv("REVOLUTION_EVAL_AUDIT")!=NULL;atexit(audit_report);
    const char*distance=getenv("REVOLUTION_CAR_DISTANCE");car_distance=distance?atoi(distance):0;if(car_distance<0||car_distance>5)car_distance=0;
    const char*all=getenv("REVOLUTION_FULL_SCENE");full_scene=!all||strcmp(all,"0");
    strcpy(path,p);fd=open(path,O_RDWR|O_CREAT|O_EXCL,0600);if(fd<0){perror("Revolution scene transport");return;}
    if(ftruncate(fd,sizeof(struct RRVShared))<0){cleanup();return;}
    void*map=mmap(NULL,sizeof(struct RRVShared),PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);if(map==MAP_FAILED){cleanup();return;}
    shared=map;memset(shared,0,sizeof *shared);shared->magic=RRV_SHARED_MAGIC;shared->size=sizeof *shared;atexit(cleanup);
    uint32_t entries[]={0x80036934,0x80057578,0x800232C4,0x8001B660,0x80053D24,0x80054654,0x80054914,0x800552F8,0x80055A58,0x80040194};
    for(unsigned i=0;i<sizeof(entries)/sizeof(entries[0]);i++)psx_mod_register_function_entry_plugin("revolution.scene.live",entries[i],hook);
}
