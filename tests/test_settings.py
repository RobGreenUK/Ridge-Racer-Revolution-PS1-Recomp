import sys
from pathlib import Path
import tempfile
import tomllib
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'launcher'))
import settings
class SettingsTests(unittest.TestCase):
    def test_roundtrip_preserves_controls_and_disables_blending(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'settings.toml';p.write_text('[controller]\nmode="digital"\n[video]\nframe_interpolation=true\n')
            values=dict(settings.DEFAULTS,scale=4,filtering='bilinear');settings.save(p,values)
            self.assertEqual(settings.read(p),values)
            data=tomllib.loads(p.read_text());self.assertEqual(data['controller']['mode'],'digital')
            self.assertFalse(data['video']['frame_interpolation']);self.assertEqual(data['video']['aspect_ratio'],'4:3')
    def test_native_migration_and_widescreen_roundtrip(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'settings.toml';p.write_text('[video]\nwindow_width=1280\n')
            self.assertFalse(settings.read(p)['nativeScene'])
            values=dict(settings.DEFAULTS,nativeScene=True,nativeFps=0,nativeAspect='16:9',nativeWidth=2560,nativeHeight=1440,perspective=False,frameGraph=True,fullScene=False)
            settings.save(p,values)
            self.assertEqual(settings.read(p),values)
    def test_previous_launcher_saves_4k_preserving_new_options(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'settings.toml'
            current=dict(settings.DEFAULTS,frameGraph=True,fullScene=False)
            settings.save(p,current)
            legacy={k:v for k,v in current.items() if k not in ('frameGraph','fullScene')}
            legacy.update(nativeScene=True,nativeAspect='16:9',nativeWidth=3840,nativeHeight=2160)
            settings.save(p,legacy)
            self.assertEqual(settings.read(p),dict(legacy,frameGraph=True,fullScene=False))
            before=p.read_bytes()
            for bad in [dict(legacy,unknown=True),{k:v for k,v in legacy.items() if k!='nativeWidth'}]:
                with self.assertRaises(ValueError):settings.save(p,bad)
                self.assertEqual(p.read_bytes(),before)

    def test_invalid_input_does_not_write(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'settings.toml'
            for values in [dict(settings.DEFAULTS,scale=True),dict(settings.DEFAULTS,scale=20),dict(settings.DEFAULTS,nativeFps=361),dict(settings.DEFAULTS,nativeAspect="16:9"),dict(settings.DEFAULTS,nativeWidth=0)]:
                with self.assertRaises(ValueError):settings.save(p,values)
                self.assertFalse(p.exists())
    def test_matching_resolution_and_visibility_options(self):
        with tempfile.TemporaryDirectory() as d:
            p=Path(d)/'settings.toml'
            for aspect,width,height in [('16:9',3840,2160),('16:9',7680,4320),('4:3',5760,4320),('16:9',2304,1296)]:
                for distance in range(6):
                    values=dict(settings.DEFAULTS,nativeScene=True,nativeAspect=aspect,nativeWidth=width,nativeHeight=height,nativeCarDistance=distance,fullScene=False)
                    settings.save(p,values)
                    self.assertEqual(settings.read(p),values)
            legacy=dict(settings.DEFAULTS);legacy.pop('nativeCarDistance');legacy.pop('rewind')
            current=settings.read(p);settings.save(p,legacy)
            self.assertEqual(settings.read(p)['nativeCarDistance'],current['nativeCarDistance'])
            with self.assertRaises(ValueError):settings.save(p,dict(settings.DEFAULTS,nativeCarDistance=6))

if __name__=='__main__':unittest.main()
