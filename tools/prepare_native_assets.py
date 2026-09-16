#!/usr/bin/env python3
"""Prepare Revolution geometry/materials for the independent renderer."""
import array,io,json,struct
from pathlib import Path
from inspect_assets import inspect,model_bank
from export_scene_preview import decode
from texture_codec import decode_tms,texture_rgba
from terrain_occlusion import repair_terrain,repair_panel_overlap
ROOT=Path(__file__).resolve().parents[1]
def prepare(name,out):
    source=ROOT/'disc/files';d=(source/f'CRS_{name}.DAT').read_bytes();ofs=struct.unpack_from('<6I',d)
    sections=model_bank(d[ofs[0]:ofs[1]],True);models=model_bank((source/'CAR.RSO').read_bytes())+model_bank(d[ofs[1]:ofs[2]])
    def faces(records,course=False,origin=(0,0,0)):
        result=[]
        for kind,raw in records:
            xyz,uv,key,bias=decode(kind,raw,course)
            window=0
            if not course and kind in (1,4):
                x,y,w,h=struct.unpack_from('<4h',raw,len(raw)-8)
                window=(((-w)&255)>>3)|((((-h)&255)>>3)<<5)|(((x&255)>>3)<<10)|(((y&255)>>3)<<15)
            verts=[(*(v[i]+origin[i] for i in range(3)),*t) for v,t in zip(xyz,uv)]
            result.append((key,kind if course else 0,bias,window,verts))
        return result
    course=[]
    for cell,section in enumerate(struct.unpack_from('<1024H',d,24)):
        if section==65535:continue
        placed=faces(sections[section],True,((30-cell%32)*2048,0,cell//32*2048))
        # USA shared intermediate/expert hillside/chevron intersection confirmed by the
        # 2026-09-10 capture. Keep this correction local to the verified pair.
        if name in ('MID','HIGH') and cell==612:
            if section!={'MID':104,'HIGH':114}[name] or any(placed[i][1:3]!=(1,18) for i in (44,45)) or placed[61][1:3]!=(2,-15):
                raise ValueError('Unexpected shared-course sign geometry')
            placed[44:46]=repair_panel_overlap(placed[44],placed[61])+repair_panel_overlap(placed[45],placed[61])
        course+=placed
    course=repair_terrain(course)
    meshes=[faces(m) for m in models];keys=sorted({f[0] for group in [course,*meshes] for f in group});lookup={k:i for i,k in enumerate(keys)}
    vram=array.array('H',[0])*524288
    for i in range(5):decode_tms((source/f'BIG{i}.TMS').read_bytes(),vram)
    with out.open('wb') as f:
        f.write(struct.pack('<8sII',b'RRASSET7',len(keys),len(course)))
        for key in keys:f.write(texture_rgba(vram,*key))
        def write(group):
            for key,kind,bias,window,vertices in group:
                f.write(struct.pack('<IIiII',lookup[key],kind,bias,window,0))
                for vertex in vertices:f.write(struct.pack('<5f',*vertex))
        write(course);f.write(struct.pack('<I',len(meshes)))
        for m in meshes:f.write(struct.pack('<I',len(m)));write(m)
        f.write(vram.tobytes());f.write(bytes(128))
        for key in keys:f.write(struct.pack('<iI',*key))
        f.write(struct.pack('<I',0))
    return dict(course=name,quads=len(course),models=len(meshes),materials=len(keys),bytes=out.stat().st_size)
if __name__=='__main__':
    inspect(ROOT/'disc-images/Ridge Racer Revolution (USA) (Track 01).bin')
    destination=ROOT/'build-macos/native-scene';destination.mkdir(parents=True,exist_ok=True)
    for i,name in enumerate(['EASY','MID','HIGH','OLDE']):print(json.dumps(prepare(name,destination/f'course-{i}.rrassets')),flush=True)
