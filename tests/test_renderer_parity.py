"""Real OpenGL checks for portable RR performance fixes, using synthetic data."""
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]

@unittest.skipUnless(sys.platform=='darwin','macOS OpenGL required')
class RendererParityTests(unittest.TestCase):
    def test_culling_order_and_texture_caches(self):
        with tempfile.TemporaryDirectory(prefix='rrr-renderer-') as tmp:
            binary=Path(tmp)/'parity'
            flags=shlex.split(subprocess.check_output(['pkg-config','--cflags','--libs','sdl3'],text=True))
            subprocess.run(['c++','-std=c++17','-O2','-DGL_SILENCE_DEPRECATION',str(ROOT/'tests/renderer_parity.cpp'),*flags,'-framework','OpenGL','-o',str(binary)],check=True,capture_output=True)
            subprocess.run([str(binary)],check=True,timeout=30)

if __name__=='__main__':unittest.main()
