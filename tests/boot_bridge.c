/* Synthetic boot pixels/input: no disc bytes or game-derived code. */
#include <assert.h>
#include "../src/scene/live.c"
#include "../src/scene/input.c"

uint64_t psx_cycle_count;
static unsigned reads,writes,pads,evaluations;
static uint8_t test_ram[0x200000];
static void word(uint32_t a,uint32_t v){memcpy(test_ram+(a&0x1fffff),&v,4);}
static void half(uint32_t a,uint16_t v){memcpy(test_ram+(a&0x1fffff),&v,2);}static uint16_t pad;
static GpuDisplayInfo display;
static void(*frame_hook)(void);
void mod_register_frame_hook(void(*f)(void)){frame_hook=f;}
int psx_mod_register_function_entry_plugin(const char*id,uint32_t a,PSXModFunctionEntryCallback cb){return 1;}
uint8_t*memory_get_ram_ptr(void){return NULL;}
uint8_t*memory_get_scratchpad_ptr(void){return NULL;}
int revolution_evaluate(const uint8_t*a,const uint8_t*b,const CPUState*c,uint32_t d,int e,struct RRVModel*f,unsigned g){evaluations++;return 0;}
uint16_t psx_mod_read_half(uint32_t a){reads++;uint16_t v;memcpy(&v,test_ram+(a&0x1fffff),2);return v;}
uint32_t psx_mod_read_word(uint32_t a){reads++;uint32_t v;memcpy(&v,test_ram+(a&0x1fffff),4);return v;}
void psx_mod_write_byte(uint32_t a,uint8_t v){writes++;test_ram[a&0x1fffff]=v;}
void sio_set_pad_state_slot(int slot,uint16_t value){assert(slot==0);pads++;pad=value;}
void gpu_get_display_info(GpuDisplayInfo*out){*out=display;}
const uint16_t*gpu_get_vram(void){return NULL;}
int gr_render_display(uint32_t*out,int pitch,int x,int y,int w,int h){
    assert(pitch==w*4);for(int i=0;i<w*h;i++)out[i]=0xff123456;return 1;
}
void gr_vram_transfer_out(int x,int y,int w,int h,uint16_t*out){
    for(int j=0;j<h;j++)for(int i=0;i<w*2;i++)((uint8_t*)out)[j*w*2+i]=(uint8_t)(i+1+j*10);
}
static void replay_tests(void){
    CPUState cpu={0};
    for(unsigned s=0;s<47;s++)assert(revolution_scene_state(s)==(s==17||s==19||s==32));
    assert(!revolution_scene_state(~0u));
    word(0x801DD200,0x800A0000);word(0x800A0000,2072);word(0x800A0004,0x3fe48);
    // Synthetic valid model bank/context, with no game data.
    word(0x800B0000,119);word(0x800B0004,0x800B1000);word(0x800B1000,0x00680003);
    word(0x800C000C,0x800B0004);
    half(0x801DD0BC,17);hook(&cpu,0x800232C4);
    cpu.gpr[4]=0x800C0000;cpu.gpr[5]=1;hook(&cpu,0x80053D24);
    assert(frame.model_count==1);
    // Race handler changed state at its tail: its geometry is not a replay.
    half(0x801DD0BC,32);boundary(&cpu);assert(!shared->frame.valid);
    hook(&cpu,0x80026EE0);cpu.gpr[4]=0;cpu.gpr[5]=0x8007C438;hook(&cpu,0x8002C290);
    boundary(&cpu);assert(!shared->frame.valid); // camera alone is not readiness
    hook(&cpu,0x80026EE0);cpu.gpr[4]=0;cpu.gpr[5]=0x8007C438;hook(&cpu,0x8002C290);
    word(0x801DE36C,0x01012345);cpu.gpr[4]=0x800C0000;cpu.gpr[5]=1;
    hook(&cpu,0x80053D24);
    // A stale active opponent record must not invoke race completion in replay.
    half(0x801F9B18,1);unsigned before=evaluations;
    boundary(&cpu);assert(shared->frame.valid&&shared->frame.model_count==1);
    assert(evaluations==before&&shared->frame.screen_width==0);
    assert(shared->frame.camera_valid&&shared->frame.camera_target==0x8007C438);
    assert(shared->frame.camera_mode==0&&shared->frame.camera_shot==0x01012345);
    hook(&cpu,0x80026EE0);boundary(&cpu);assert(!shared->frame.valid); // missing camera
    half(0x801DD0BC,22);boundary(&cpu);assert(!shared->frame.valid); // results/menu
    assert(shared->frame.model_count==0&&shared->frame.hud_count==0);
    half(0x801DD0BC,32);hook(&cpu,0x80026EE0);
    cpu.gpr[4]=1;cpu.gpr[5]=0x8007C438;hook(&cpu,0x8002C290);
    boundary(&cpu);assert(!shared->frame.valid); // no stale readiness on re-entry
    // Race remains supported with its original completion option.
    half(0x801DD0BC,17);hook(&cpu,0x800232C4);
    cpu.gpr[4]=0x800C0000;cpu.gpr[5]=1;hook(&cpu,0x80053D24);boundary(&cpu);
    assert(shared->frame.valid&&evaluations>before&&!shared->frame.camera_valid);
    // The opt-in smoke driver must not skip the replay it is testing.
    cpu.gpr[31]=0x80019E70;half(0x801DD0BC,32);
    for(unsigned i=0;i<240;i++){input(&cpu,0);assert(psx_mod_read_half(0x801DC906)==0xffff);}
    half(0x801DD0BC,17);input(&cpu,0);assert(psx_mod_read_half(0x801DC906)==0xbfff);
    half(0x801DD0BC,1);int start_seen=0;
    for(unsigned i=0;i<120;i++){input(&cpu,0);start_seen|=psx_mod_read_half(0x801DC906)==0xfff7;}
    assert(start_seen);
}
int main(void){
    // The constructor uses the test's unique REVOLUTION_SCENE_FILE path.
    assert(shared&&frame_hook);
    display.width=3;display.height=2;display.disabled=0;
    shared->input_ns=now();shared->input_active=1;shared->input_buttons=0xfff7;
    psx_cycle_count=100;frame_hook();
    assert(reads==0&&writes==0&&pads==1&&pad==0xfff7);
    assert(shared->frame.sequence==1&&shared->frame.cycles==100&&!shared->frame.valid);
    assert(shared->frame.screen_width==3&&shared->frame.screen_height==2);
    assert(shared->frame.screen[5]==0xff123456);
    // Packed RGB24, including the padding word for odd widths.
    display.depth24=1;frame_hook();
    assert(shared->frame.screen[0]==0xff010203&&shared->frame.screen[3]==0xff0b0c0d);
    unsigned before=pads;
    shared->input_ns=now()-300000000;frame_hook();assert(pads==before);
    shared->input_ns=now()+1000000000;frame_hook();assert(pads==before);
    shared->input_ns=now();shared->input_buttons=0x10000;frame_hook();assert(pads==before);
    shared->input_buttons=0xffff;shared->input_active=0;frame_hook();assert(pads==before);
    // A busy reader never stalls the producer or exposes a partial frame.
    int locked=open(path,O_RDWR);assert(locked>=0&&flock(locked,LOCK_EX|LOCK_NB)==0);
    uint32_t held_sequence=shared->frame.sequence;frame_hook();
    assert(shared->frame.sequence==held_sequence);
    flock(locked,LOCK_UN);close(locked);
    // Disabled display must invalidate the preceding image.
    display.disabled=1;frame_hook();assert(shared->frame.screen_width==0);
    display.disabled=0;display.width=641;frame_hook();assert(shared->frame.screen_width==0);
    // Once the verified racing boundary takes over, boot frames must not
    // overwrite its publication or feed the racing pad through boot SIO.
    CPUState cpu={0};boundary(&cpu);uint32_t seq=shared->frame.sequence;
    shared->input_active=1;shared->input_ns=now();frame_hook();
    assert(shared->frame.sequence==seq&&pads==before);
    // Racing input remains guarded by its original verified return address.
    hook(&cpu,0x80040194);assert(writes==0);
    cpu.gpr[31]=0x80019E70;hook(&cpu,0x80040194);assert(writes==4);
    replay_tests();
    puts("boot bridge and replay readiness/capture/fallback passed");
}
