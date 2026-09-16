import importlib.util
from pathlib import Path
import unittest
spec=importlib.util.spec_from_file_location('camera_trace',Path(__file__).resolve().parents[1]/'tools/analyze_camera_trace.py')
trace=importlib.util.module_from_spec(spec);spec.loader.exec_module(trace)

def row(i,x):
    r=dict(ready='1',source_changed='1',phase='17',sequence=str(i+1),source_cycles=str(i*1128960),wall_s=str(i/30),sample_monotonic_s=str(100+i/30),swap_end_s=str(i/30+.002),marker='0',interpolation_held='0')
    for prefix in ('source_camera_','camera_'):
        for c,value in zip('xyz',(x,0,0)):r[prefix+c]=str(value)
    for prefix in ('source_q','render_q'):
        for c,value in zip('xyzw',(0,0,0,1)):r[prefix+c]=str(value)
    for k in range(9):r['source_matrix_'+str(k)]=str(4096 if k%4==0 else 0)
    return r

class CameraTraceTests(unittest.TestCase):
    def test_constant_motion_repeats_and_missing_source(self):
        rows=[row(i,i*10) for i in range(4)]
        s=trace.summarize(rows,0,1)
        self.assertAlmostEqual(s['source_speed_units_per_guest_second']['median'],300)
        self.assertAlmostEqual(s['rendered_speed_units_per_wall_second']['median'],300)
        self.assertEqual(s['repeated_source_poses'],0)
        self.assertEqual(s['source_turn_degrees_per_guest_second']['maximum'],0)
        rows[1]=row(1,0)
        self.assertEqual(trace.summarize(rows,0,1)['repeated_source_poses'],1)
        rows.pop(2)
        s=trace.summarize(rows,0,1)
        self.assertEqual(s['skipped_source_snapshots'],1)
        self.assertEqual(s['source_pairs'],1)
    def test_quaternion_sign_and_capture_exclusion(self):
        a,b=row(0,0),row(1,10);b['render_qw']='-1'
        self.assertEqual(trace.angle(a,b,'render_q'),0)
        b['marker']='1'
        self.assertIsNone(trace.summarize([a,b],0,1)['rendered_speed_units_per_wall_second'])
