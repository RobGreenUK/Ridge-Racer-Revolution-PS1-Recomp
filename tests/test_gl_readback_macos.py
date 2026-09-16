"""Real hidden OpenGL regression; run with scripts/macos-env.sh on macOS."""
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(sys.platform == 'darwin', 'macOS OpenGL context required')
class GLReadbackTests(unittest.TestCase):
    def test_queued_draws_survive_diagnostic_readback(self):
        runtime = ROOT / 'psxrecomp/runtime'
        flags = shlex.split(subprocess.check_output(
            ['pkg-config', '--cflags', '--libs', 'sdl3'], text=True))
        with tempfile.TemporaryDirectory(prefix='ridge-gl-readback-') as tmp:
            binary = Path(tmp) / 'readback'
            built = subprocess.run([
                'cc', '-std=c11', '-O2', '-flto', '-DPSX_SDL3=1',
                '-DPSX_NO_DEBUG_TOOLS=1', '-DGL_SILENCE_DEPRECATION',
                '-I'+str(runtime/'include'), '-I'+str(runtime/'src'),
                str(runtime/'tests/test_gl_readback_region.c'),
                str(runtime/'src/gpu_sw_renderer.c'), '-Wl,-dead_strip',
                *flags, '-framework', 'OpenGL', '-o', str(binary),
            ], capture_output=True, text=True, timeout=60)
            self.assertEqual(built.returncode, 0, built.stderr)
            for scale in (1, 4):
                with self.subTest(scale=scale):
                    result = subprocess.run([str(binary), str(scale)],
                                            capture_output=True, text=True, timeout=30)
                    self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
                    self.assertIn('checks=95 failures=0', result.stdout)


if __name__ == '__main__':
    unittest.main()
