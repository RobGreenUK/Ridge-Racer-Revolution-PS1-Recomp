#!/usr/bin/env python3
"""Extract verified USA assets locally and validate Revolution's container layouts.

No disc bytes or extracted assets belong in Git. Geometry field semantics still
need runtime verification; this tool validates record boundaries, not rendering.
"""
import argparse
import array
import collections
import hashlib
import json
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'psxrecomp/tools'))
from new_project_layout.probe_disc import read_user_bin,parse_root_entries,read_file

def model_bank(data, course=False):
    if len(data)<4:raise ValueError('Short model bank')
    n=struct.unpack_from('<I',data)[0]
    if not 1<=n<=4096 or 4+4*n>len(data):raise ValueError('Invalid model count')
    offsets=list(struct.unpack_from(f'<{n}I',data,4))+[len(data)]
    if offsets[0]!=4+4*n or any(a>=b for a,b in zip(offsets,offsets[1:])):
        raise ValueError('Invalid model directory')
    models=[]
    for a,b in zip(offsets,offsets[1:]):
        cursor=a;records=[];terminated=False
        while cursor+4<=b:
            kind,count=struct.unpack_from('<HH',data,cursor);cursor+=4
            if kind==count==0:
                terminated=True;break
            if kind>= (3 if course else 6) or count==0:raise ValueError('Unknown primitive group')
            size=40 if course else [40,48,32,64,72,56][kind]
            end=cursor+size*count
            if end>b:raise ValueError('Truncated primitives')
            records.extend((kind,data[p:p+size]) for p in range(cursor,end,size));cursor=end
        if not terminated or cursor!=b:raise ValueError('Invalid model terminator')
        models.append(records)
    return models

def course_bank(data):
    if len(data)<2072:raise ValueError('Short course')
    offsets=list(struct.unpack_from('<6I',data))+[len(data)]
    if offsets[0]!=2072 or any(a>=b for a,b in zip(offsets,offsets[1:])):raise ValueError('Invalid course directory')
    parts=[data[a:b] for a,b in zip(offsets,offsets[1:])]
    sections=model_bank(parts[0],course=True);models=model_bank(parts[1])
    grid=struct.unpack_from('<1024H',data,24);placed=[i for i in grid if i!=65535]
    if sorted(placed)!=list(range(len(sections))):raise ValueError('Invalid section placements')
    return dict(sections=len(sections),quads=sum(map(len,sections)),models=len(models),model_quads=sum(map(len,models)),
                section_kinds=dict(collections.Counter(k for group in sections for k,_ in group)),part_sizes=list(map(len,parts)))

def tms_images(data):
    if len(data)<8 or struct.unpack_from('<I',data)[0]!=256:raise ValueError('Invalid TMS')
    cursor=4;count=0
    while cursor+4<=len(data):
        size=struct.unpack_from('<I',data,cursor)[0];cursor+=4
        if size==0:
            if cursor!=len(data):raise ValueError('TMS trailing bytes')
            return count
        image=data[cursor:cursor+size];cursor+=size
        if len(image)!=size or size<20:raise ValueError('Short TIM')
        magic,flags=struct.unpack_from('<II',image)
        if magic!=16 or flags&~15:raise ValueError('Invalid TIM')
        p=8
        for _ in range(2 if flags&8 else 1):
            if p+12>size:raise ValueError('Short rectangle')
            claimed,x,y,w,h=struct.unpack_from('<I4H',image,p);p+=12+w*h*2
            if p>size or x+w>1024 or y+h>512 or claimed not in (12+w*h*2,12+w*h*4):raise ValueError('Invalid upload')
        if p!=size:raise ValueError('TIM trailing bytes')
        count+=1
    raise ValueError('Missing TMS terminator')

def inspect(track):
    raw=track.read_bytes();probe=json.loads((ROOT/'disc_probe.json').read_text())
    if hashlib.sha1(raw).hexdigest()!=probe['data_track_sha1']:raise ValueError('Unsupported disc revision')
    pvd=read_user_bin(raw,16);root=read_file(read_user_bin,raw,struct.unpack_from('<I',pvd,158)[0],struct.unpack_from('<I',pvd,166)[0])
    directory=parse_root_entries(root);destination=ROOT/'disc/files';destination.mkdir(parents=True,exist_ok=True)
    summary={}
    # Deliberately select root files; CDDA/MOVIE are directory records.
    for name,(lba,size) in sorted(directory.items()):
        if not name.endswith(('.TMS','.DAT','.RSO','.EXE','.14','.CNF','.VH','.VB','.SEQ')):continue
        data=read_file(read_user_bin,raw,lba,size);(destination/name).write_bytes(data)
        entry=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
        if name.startswith('CRS_'):entry.update(course_bank(data))
        elif name.endswith('.RSO'):
            models=model_bank(data);entry.update(models=len(models),quads=sum(map(len,models)))
        elif name.endswith('.TMS'):entry['images']=tms_images(data)
        summary[name]=entry
    return summary
if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--track',type=Path,default=ROOT/'disc-images/Ridge Racer Revolution (USA) (Track 01).bin')
    args=parser.parse_args();print(json.dumps(inspect(args.track),indent=2))
