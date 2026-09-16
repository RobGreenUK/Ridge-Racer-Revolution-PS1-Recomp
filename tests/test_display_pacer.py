"""The Mac display callback wakes rendering and stops cleanly on a lost source."""
import shlex
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(sys.platform == 'darwin', 'CoreVideo is macOS-only')
class DisplayPacerTests(unittest.TestCase):
    def test_real_callbacks_timeout_and_cleanup(self):
        source = r'''
#include <SDL3/SDL.h>
#include "display_pacer.h"
#include <cassert>
int main() {
    assert(SDL_Init(SDL_INIT_VIDEO));
    auto* window=SDL_CreateWindow("Display pacing test",320,240,0);
    assert(window);
    auto* renderer=SDL_CreateRenderer(window,"opengl");
    assert(renderer);
    DisplayPacer pacer;
    if(!pacer.open()){fprintf(stderr,"Pacer open failed: %s; CGL=%p\n",SDL_GetError(),CGLGetCurrentContext());return 2;}
    for(int i=0;i<3;++i)assert(pacer.wait());
    assert(pacer.stop(pacer.link)==kCVReturnSuccess);
    // A callback already in flight may leave one pending wake after Stop.
    const auto begin=SDL_GetTicksNS();
    if(pacer.wait())assert(!pacer.wait());
    assert(SDL_GetTicksNS()-begin<1000000000ull);
    pacer.close();pacer.close();
    SDL_DestroyRenderer(renderer);SDL_DestroyWindow(window);SDL_Quit();
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp)
            (path / 'test.cpp').write_text(source)
            flags = shlex.split(subprocess.check_output(
                ['/opt/homebrew/bin/pkg-config', '--cflags', '--libs', 'sdl3'], text=True))
            subprocess.run(['c++', '-std=c++17', '-I'+str(ROOT/'src/scene'),
                            str(path/'test.cpp'), *flags, '-framework', 'OpenGL',
                            '-o', str(path/'test')], check=True, capture_output=True)
            result=subprocess.run([str(path/'test')], capture_output=True, timeout=10)
            self.assertEqual(result.returncode,0,result.stderr.decode())
