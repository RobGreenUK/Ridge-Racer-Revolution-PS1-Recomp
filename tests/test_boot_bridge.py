"""Exercise the real boot transport with synthetic GPU/controller services."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class BootBridgeTests(unittest.TestCase):
    def test_boot_pixels_input_and_racing_handoff(self):
        with tempfile.TemporaryDirectory(prefix='revolution-boot-') as tmp:
            binary = Path(tmp)/'boot'
            subprocess.run(['cc', '-std=c11',
                            '-I'+str(ROOT/'psxrecomp/runtime/include'),
                            str(ROOT/'tests/boot_bridge.c'), '-o', str(binary)], check=True)
            env = dict(os.environ, REVOLUTION_SCENE_FILE=str(Path(tmp)/'scene'))
            env.pop('REVOLUTION_TEST_INPUT', None)
            subprocess.run([str(binary)], env=env, check=True, timeout=10)

if __name__ == '__main__':
    unittest.main()
