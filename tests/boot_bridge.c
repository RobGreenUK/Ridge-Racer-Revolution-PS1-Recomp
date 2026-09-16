/* Synthetic boot pixels/input: no disc bytes or game-derived code. */
#include <assert.h>
#include "../src/scene/live.c"

uint64_t psx_cycle_count;
static unsigned reads,writes,pads;static uint16_t pad;
static GpuDisplayInfo display;
static void(*frame_hook)(void);
void mod_register_frame_hook(void(*f)(void)){frame_hook=f;}
int psx_mod_register_function_entry_plugin(const char*id,uint32_t a,PSXModFunctionEntryCallback cb){return 1;}
uint8_t*memory_get_ram_ptr(void){return NULL;}
uint8_t*memory_get_scratchpad_ptr(void){return NULL;}
int revolution_evaluate(const uint8_t*a,const uint8_t*b,const CPUState*c,uint32_t d,int e,struct RRVModel*f,unsigned g){return 0;}
uint16_t psx_mod_read_half(uint32_t a){reads++;return 0;}
uint32_t psx_mod_read_word(uint32_t a){reads++;return 0;}
void psx_mod_write_byte(uint32_t a,uint8_t v){writes++;}
void sio_set_pad_state_slot(int slot,uint16_t value){assert(slot==0);pads++;pad=value;}
void gpu_get_display_info(GpuDisplayInfo*out){*out=display;}
const uint16_t*gpu_get_vram(void){return NULL;}
int gr_render_display(uint32_t*out,int pitch,int x,int y,int w,int h){
    assert(pitch==w*4);for(int i=0;i<w*h;i++)out[i]=0xff123456;return 1;
}
void gr_vram_transfer_out(int x,int y,int w,int h,uint16_t*out){
    for(int j=0;j<h;j++)for(int i=0;i<w*2;i++)((uint8_t*)out)[j*w*2+i]=(uint8_t)(i+1+j*10);
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
    puts("boot pixels, input freshness and racing handoff passed");
}
