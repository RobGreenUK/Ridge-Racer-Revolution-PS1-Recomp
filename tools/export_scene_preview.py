#!/usr/bin/env python3
"""Export a captured Revolution frame to a diagnostic native geometry preview.

Course transforms/materials are under validation. Never used to alter game RAM.
"""
import argparse,array,collections,json,struct
from pathlib import Path
from inspect_assets import model_bank
from texture_codec import texture_rgba
ROOT=Path(__file__).resolve().parents[1]
def signed(n):return n-(1<<32) if n&(1<<31) else n
def matrix(raw):return [v/4096 for v in struct.unpack('<9h',bytes.fromhex(raw)[:18])]
def transform(m,p):return tuple(sum(m[r*3+c]*p[c] for c in range(3)) for r in range(3))
def decode(kind,data,course=False):
    h=struct.unpack_from('<12h',data)
    xyz=[(h[i*2]/4,h[i*2+1]/4,h[8+i]/4) for i in range(4)]
    tail=24 if course or kind<3 else 48;flat=not course and kind in (2,5)
    if flat:key=(-1,int.from_bytes(data[tail:tail+3],'little'));uv=[(.5,.5)]*4
    else:
        key=(struct.unpack_from('<H',data,tail+6)[0],struct.unpack_from('<H',data,tail+2)[0])
        uv=[(data[tail+i*4]/256,data[tail+i*4+1]/256) for i in range(4)]
    bias=struct.unpack_from('<h',data,tail+(4 if flat else 10))[0]
    return xyz,uv,key,bias

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('capture',type=Path);p.add_argument('output',type=Path);p.add_argument('--course',default='EASY');p.add_argument('--sequence',type=int);p.add_argument('--objects',action='store_true');a=p.parse_args()
    rows=[json.loads(l) for l in (a.capture/'scene.jsonl').read_text().splitlines()]
    seq=a.sequence if a.sequence is not None else next(r['sequence'] for r in rows if r['type']=='snapshot')
    frame=next(r for r in rows if r['type']=='frame' and r['sequence']==seq)
    cam=tuple(signed(v) for v in frame['camera'][:3]);rotation=matrix(frame['matrix'])
    file=ROOT/'disc/files'/f'CRS_{a.course}.DAT';data=file.read_bytes();ofs=struct.unpack_from('<6I',data)
    sections=model_bank(data[ofs[0]:ofs[1]],True);objects=model_bank(data[ofs[1]:ofs[2]]);cars=model_bank((ROOT/'disc/files/CAR.RSO').read_bytes())
    faces=[]
    def add(records,co=False,origin=(0,0,0),m=(1,0,0,0,1,0,0,0,1),camera_space=False):
        for kind,raw in records:
            xyz,uv,key,bias=decode(kind,raw,co)
            verts=[]
            for v,t in zip(xyz,uv):
                v=transform(m,v);v=tuple(v[i]+origin[i] for i in range(3))
                if not camera_space:v=transform(rotation,tuple(v[i]-cam[i] for i in range(3)))
                verts.append((*v,*t))
            faces.append((key,bias,verts))
    for cell,section in enumerate(struct.unpack_from('<1024H',data,24)):
        if section!=65535:add(sections[section],True,((30-cell%32)*2048,0,cell//32*2048))
    matched=collections.Counter()
    if a.objects:
        for row in rows:
            if row['type']!='model' or row['sequence']!=seq or row['context'][12]:continue
            prefix=bytes.fromhex(row['prefix']);idx=row['model'];models=None
            # Match the immutable primitive coordinates, not a hard-coded RAM bank.
            for name,bank in [('objects',objects),('cars',cars)]:
                if idx<len(bank) and bank[idx] and bank[idx][0][1][:24]==prefix[4:28]:models=bank;matched[name]+=1;break
            if models is None:continue
            packed=b''.join(struct.pack('<I',v) for v in row['gte'][:5]);m=[x/4096 for x in struct.unpack('<9h',packed[:18])]
            t=[signed(v)/4 for v in row['gte'][5:8]];add(models[idx],origin=t,m=m,camera_space=True)
    keys=sorted(set(f[0] for f in faces));lookup={key:i for i,key in enumerate(keys)}
    raw=(a.capture/'snapshot.vram').read_bytes()
    if len(raw)!=1048576:raise ValueError('Invalid VRAM snapshot')
    vram=array.array('H');vram.frombytes(raw)
    with a.output.open('wb') as out:
        out.write(struct.pack('<8sII',b'RRVMESH1',len(keys),len(faces)))
        for key in keys:out.write(texture_rgba(vram,*key))
        for key,bias,verts in faces:
            out.write(struct.pack('<Ii',lookup[key],bias))
            for v in verts:out.write(struct.pack('<5f',*v))
    print(json.dumps(dict(sequence=seq,camera=cam,rotation=rotation,quads=len(faces),textures=len(keys),matched=dict(matched))))
if __name__=='__main__':main()
