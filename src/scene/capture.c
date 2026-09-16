/* Revolution USA: opt-in, read-only scene capture. No CPU/RAM/clock writes. */
#include "cpu_state.h"
#include "mod_plugins.h"
#include "psx_cycles.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#define MAIN_DRAW_SYNC_RETURN 0x80019D00u
#define STATE 0x801DD0BCu
#define CAMERA 0x801DE360u
#define MATRIX 0x801F9A5Cu
#define COURSE 0x801DD200u
static FILE *output;
static unsigned sequence,models,owner,owner_site,race_frames;
extern uint8_t *memory_get_ram_ptr(void);
extern const uint16_t *gpu_get_vram(void);
static void dump(const char *suffix,const void *data,size_t size){
    const char *prefix=getenv("REVOLUTION_SCENE_DUMP_PREFIX");if(!prefix||!data)return;
    char path[1024];if(snprintf(path,sizeof path,"%s.%s",prefix,suffix)>=(int)sizeof path)return;
    FILE *f=fopen(path,"wx");if(f){if(fwrite(data,1,size,f)!=size)perror("scene dump");fclose(f);}
}

static unsigned limit=5000;
static void finish(void){if(output)fclose(output);output=NULL;}
static void words(uint32_t addr,unsigned n){fputc('[',output);for(unsigned i=0;i<n;i++)fprintf(output,"%s%u",i?",":"",psx_mod_read_word(addr+4*i));fputc(']',output);}
static void bytes(uint32_t addr,unsigned n){fputc('"',output);for(unsigned i=0;i<n;i++)fprintf(output,"%02x",psx_mod_read_byte(addr+i));fputc('"',output);}
static void hook(CPUState *cpu,uint32_t address){
    if(!output||sequence>=limit)return;
    if(address==0x800232C4u){owner=owner_site=0;return;}
    if(address==0x8001B660u){owner=cpu->gpr[4];owner_site=cpu->gpr[31];return;}
    if(address==0x80057578u){
        if(cpu->gpr[31]!=MAIN_DRAW_SYNC_RETURN)return;
        fprintf(output,"{\"type\":\"frame\",\"sequence\":%u,\"cycles\":%" PRIu64 ",\"state\":%u,\"models\":%u,\"course\":%u,\"wait\":%u,\"camera\":",sequence++,psx_cycle_count,psx_mod_read_half(STATE),models,psx_mod_read_word(COURSE),psx_mod_read_half(0x80194CB0u));
        words(CAMERA,4);fputs(",\"matrix\":",output);bytes(MATRIX,32);
        fputs(",\"course_header\":",output);words(psx_mod_read_word(COURSE),12);
        fputs("}\n",output);
        if((psx_mod_read_half(STATE)==17 || psx_mod_read_half(STATE)==19) && models && ++race_frames==120){
            dump("ram",memory_get_ram_ptr(),2*1024*1024);dump("vram",gpu_get_vram(),1024*512*2);
            fprintf(output,"{\"type\":\"snapshot\",\"sequence\":%u}\n",sequence-1);
        }
        models=0;owner=owner_site=0;
        if(sequence%120==0)fflush(output);
        if(sequence==limit)finish();return;
    }

    uint32_t ctx=cpu->gpr[4],model=cpu->gpr[5],bank=psx_mod_read_word(ctx+12);
    if(model>4096||models++>=1024)return;
    uint32_t data=psx_mod_read_word(bank+4*model);
    fprintf(output,"{\"type\":\"model\",\"sequence\":%u,\"function\":%u,\"site\":%u,\"owner\":%u,\"owner_site\":%u,\"model\":%u,\"bank\":%u,\"data\":%u,\"context\":",sequence,address,cpu->gpr[31],owner,owner_site,model,bank,data);
    words(ctx,14);fputs(",\"gte\":[",output);for(unsigned i=0;i<32;i++)fprintf(output,"%s%u",i?",":"",cpu->gte_ctrl[i]);
    fputs("],\"camera\":",output);words(CAMERA,4);fputs(",\"matrix\":",output);bytes(MATRIX,32);
    fputs(",\"owner_data\":",output);bytes(owner,0x180);fputs(",\"prefix\":",output);bytes(data,68);fputs("}\n",output);
}
PSX_MOD_CONSTRUCTOR(register_revolution_capture){
    const char *path=getenv("REVOLUTION_SCENE_CAPTURE");if(!path||!*path)return;
    output=fopen(path,"wx");if(!output){perror("Revolution scene capture");return;}
    const char *cap=getenv("REVOLUTION_SCENE_CAPTURE_FRAMES");if(cap){unsigned n=(unsigned)strtoul(cap,NULL,10);if(n>0&&n<=18000)limit=n;}
    setvbuf(output,NULL,_IOFBF,256*1024);atexit(finish);
    uint32_t entries[]={0x80057578u,0x800232C4u,0x8001B660u,0x80053D24u,0x80054654u,0x80054914u,0x800552F8u,0x80055A58u};
    for(unsigned i=0;i<sizeof(entries)/sizeof(entries[0]);i++)psx_mod_register_function_entry_plugin("revolution.scene.capture",entries[i],hook);
}
