"""Synthetic road/terrain intersections; no game assets required."""
import sys,unittest
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
from terrain_occlusion import repair_terrain,repair_panel_overlap

def vertex(x,y,z):return (x,y,z,x/100,y/100)
def face(kind,v,bias=1):return ((12,10),kind,bias,0,v)
class TerrainTests(unittest.TestCase):
    def test_crossing_wall_is_trimmed_below_road_with_uvs_preserved(self):
        road=face(1,[vertex(-20,0,-20),vertex(20,0,-20),vertex(-20,0,20),vertex(20,0,20)])
        wall=face(0,[vertex(-10,-20,0),vertex(10,-20,0),vertex(-10,40,0),vertex(10,40,0)],15)
        out=repair_terrain([road,wall]);self.assertEqual(out[0],road)
        self.assertGreater(len(out),1)
        for f in out[1:]:
            for x,y,z,u,v in f[-1]:
                self.assertGreaterEqual(y,10-1e-5)
                self.assertAlmostEqual(u,x/100);self.assertAlmostEqual(v,y/100)
    def test_sign_repair_preserves_terrain_outside_panel_and_uvs(self):
        panel=face(2,[vertex(-1,0,0),vertex(1,0,0),vertex(-1,2,0),vertex(1,2,0)],-15)
        terrain=face(1,[vertex(-2,-1,-1),vertex(2,-1,1),vertex(-2,3,-1),vertex(2,3,1)],18)
        out=repair_panel_overlap(terrain,panel);self.assertGreater(len(out),1)
        front_outside=False
        for f in out:
            tri=f[-1][:3];x,y,z=[sum(v[i] for v in tri)/3 for i in range(3)]
            self.assertFalse(-1+1e-6<x<1-1e-6 and 0+1e-6<y<2-1e-6 and z<.5-1e-6)
            for x,y,z,u,v in tri:
                self.assertAlmostEqual(u,x/100);self.assertAlmostEqual(v,y/100)
                front_outside|=z<0 and (x<-1 or y<0 or y>2)
        self.assertTrue(front_outside)
        behind=face(1,[vertex(-1,0,2),vertex(1,0,2),vertex(-1,2,2),vertex(1,2,2)],18)
        self.assertEqual(repair_panel_overlap(behind,panel),[behind])

    def test_outside_road_and_foreground_geometry_stays_intact(self):
        road=face(1,[vertex(-20,0,-20),vertex(20,0,-20),vertex(-20,0,20),vertex(20,0,20)])
        outside=face(0,[vertex(30,-20,0),vertex(50,-20,0),vertex(30,40,0),vertex(50,40,0)],15)
        foreground=face(0,[vertex(-10,-20,0),vertex(10,-20,0),vertex(-10,40,0),vertex(10,40,0)],-1)
        below=face(0,[vertex(-10,20,0),vertex(10,20,0),vertex(-10,40,0),vertex(10,40,0)],15)
        roof=face(0,[vertex(-10,-30,-10),vertex(10,-30,-10),vertex(-10,-30,10),vertex(10,-30,10)],15)
        items=[road,outside,foreground,below,roof];self.assertEqual(repair_terrain(items),items)
if __name__=='__main__':unittest.main()
