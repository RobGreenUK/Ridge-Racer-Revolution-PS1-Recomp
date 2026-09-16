/* Opt-in deterministic controller input for isolated scene validation.
 * Same digital packet read by 80040194. Never writes handling/physics fields. */
#include "cpu_state.h"
#include "presentation_mode.h"
#include "mod_plugins.h"
#include <stdlib.h>
#include <string.h>
static void input(CPUState*cpu,uint32_t address){
    (void)address;if(cpu->gpr[31]!=0x80019E70u)return;
    static unsigned tick;tick++;
    unsigned state=psx_mod_read_half(0x801DD0BCu);
    unsigned buttons=0xffff;
    if(state==17)buttons&=~0x4000u; /* accelerate */
    else if(!revolution_replay_state(state)&&tick%120>=60&&tick%120<68)buttons&=~8u; /* Start then release */
    psx_mod_write_byte(0x801DC904u,0);psx_mod_write_byte(0x801DC905u,0x41);
    psx_mod_write_byte(0x801DC906u,buttons&255);psx_mod_write_byte(0x801DC907u,buttons>>8);
}
PSX_MOD_CONSTRUCTOR(register_revolution_test_input){
    const char*enable=getenv("REVOLUTION_TEST_INPUT");
    if(enable&&strcmp(enable,"1")==0)psx_mod_register_function_entry_plugin("revolution.test.input",0x80040194u,input);
}
