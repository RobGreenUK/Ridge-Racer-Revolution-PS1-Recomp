"""Check Enhanced launch ownership and hidden-companion policy without a disc."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock, patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'launcher'))
import native_scene
from settings import DEFAULTS

class NativeLaunchTests(unittest.TestCase):
    def test_hidden_default_and_explicit_diagnostic_opt_out(self):
        for visible in (False, True):
            with self.subTest(visible=visible), tempfile.TemporaryDirectory() as tmp:
                root=Path(tmp);build=root/'build-macos'
                (build/'native-scene').mkdir(parents=True)
                (build/'mods/bundled').mkdir(parents=True)
                for name in ('RevolutionNative','RidgeRacerRevolution'):
                    (build/name).write_text('test executable')
                for i in range(4):
                    (build/f'native-scene/course-{i}.rrassets').touch()
                (build/'settings.toml').write_text('[video]\nfullscreen=2\n')
                game=Mock();game.poll.return_value=0
                viewer=Mock();viewer.poll.return_value=0
                env={'REVOLUTION_TEST_INPUT':'1','PSX_HIDDEN_COMPANION':'0'}
                if visible:env['REVOLUTION_VISIBLE_COMPANION']='1'
                with patch.object(native_scene.subprocess,'Popen',side_effect=[game,viewer]) as spawn:
                    native_scene.launch(root,DEFAULTS,env,None)
                    self.assertEqual(spawn.call_count,2)
                    for call in spawn.call_args_list:
                        child=call.kwargs['env']
                        self.assertEqual(child['PSX_HIDDEN_COMPANION'],'0' if visible else '1')
                        self.assertEqual(child['SDL_RENDER_DRIVER'],'opengl')
                        self.assertNotIn('REVOLUTION_TEST_INPUT',child)
                    self.assertIn(str(root/'saves'),spawn.call_args_list[0].args[0])
                self.assertEqual((build/'settings.toml').read_text(),'[video]\nfullscreen=2\n')
                self.assertEqual(env['PSX_HIDDEN_COMPANION'],'0')

if __name__ == '__main__':
    unittest.main()
