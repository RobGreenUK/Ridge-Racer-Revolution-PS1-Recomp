"""The visibility evaluator must never write guest RAM or CPU state."""
from pathlib import Path
import subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
HARNESS=r'''
#include "shared.h"
#include "cpu_state.h"
#include <vector>
#include <cstring>
#include <cassert>
extern "C" int revolution_evaluate(const uint8_t*,const uint8_t*,const CPUState*,uint32_t,int,RRVModel*,unsigned);
int main(){
 std::vector<uint8_t>ram(2097152,0);CPUState cpu{};cpu.gpr[4]=0x801f9b18;cpu.gpr[6]=0x76543210;RRVModel out[16];
 auto words=[&](std::initializer_list<uint32_t> code){size_t at=0x1b660;for(auto w:code){std::memcpy(ram.data()+at,&w,4);at+=4;}};
 // Private stack writes execute, including delayed reads; caller stays intact.
 words({0x27bdfff0,0xafbf000c,0x8fbf000c,0,0x03e00008,0});
 auto before=ram;auto original=cpu;
 assert(revolution_evaluate(ram.data(),nullptr,&cpu,0x8001b660,0,out,16)==0);
 assert(ram==before&&std::memcmp(&cpu,&original,sizeof cpu)==0);
 // A write through the supplied actor pointer must fail instead of touching it.
 words({0xac800000,0x03e00008,0});before=ram;
 assert(revolution_evaluate(ram.data(),nullptr,&cpu,0x8001b660,1,out,16)==-1);
 assert(ram==before&&std::memcmp(&cpu,&original,sizeof cpu)==0);
 // Unbounded code loops stop at the evaluator's instruction budget.
 words({0x08006d98,0});before=ram;
 assert(revolution_evaluate(ram.data(),nullptr,&cpu,0x8001b660,1,out,16)==-1);
 assert(ram==before&&std::memcmp(&cpu,&original,sizeof cpu)==0);
}
'''
class SubmissionTests(unittest.TestCase):
    def test_local_memory_and_execution_bounds(self):
        with tempfile.TemporaryDirectory() as d:
            source=Path(d)/'test.cpp';source.write_text(HARNESS);exe=Path(d)/'test'
            subprocess.run(['c++','-std=c++17','-O2','-I'+str(ROOT/'src/scene'),'-I'+str(ROOT/'psxrecomp/runtime/include'),str(ROOT/'src/scene/submission_eval.cpp'),str(source),'-o',str(exe)],check=True,capture_output=True)
            subprocess.run([str(exe)],check=True,capture_output=True,timeout=10)
