"""Revolution's launch bridge. Settings and saves are separate from Ridge Racer."""
import argparse
import datetime
import fcntl
import json
import math
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import tomllib
DEFAULTS=dict(width=1280,scale=3,fullscreen=0,filtering='nearest',antialiasing=True,vsync='on',lowLatency=True,nativeScene=False,nativeFps=60,nativeAspect='4:3',nativeWidth=1280,nativeHeight=960,perspective=True,frameGraph=False,fullScene=True,nativeCarDistance=0,rewind=False)
KEYS=dict(width='window_width',scale='supersampling',fullscreen='fullscreen',filtering='texture_filtering',antialiasing='antialiasing',vsync='vsync',lowLatency='low_latency_input',rewind='rewind')
CHOICES=dict(nativeCarDistance=[0,1,2,3,4,5],width=[640,960,1280,1600,1920,2560],scale=[1,2,3,4],fullscreen=[0,1,2],filtering=['nearest','bilinear'],vsync=['on','off','adaptive'],nativeAspect=['4:3','16:9'])
def validate(values):
    if set(values)!=set(DEFAULTS):raise ValueError('Incomplete or unknown settings')
    for k,v in values.items():
        if type(v) is not type(DEFAULTS[k]) or (k in CHOICES and v not in CHOICES[k]):raise ValueError(f'Invalid {k}')
    a,b={'4:3':(4,3),'16:9':(16,9)}[values['nativeAspect']]
    if values['nativeFps']!=0 and not 30<=values['nativeFps']<=360:raise ValueError('Native FPS must be 30–360, or 0 to match the display')
    if not 320<=values['nativeWidth']<=7680 or not 240<=values['nativeHeight']<=4320 or values['nativeWidth']*b!=values['nativeHeight']*a:raise ValueError('Native resolution must match the aspect ratio')
    return values

def document(path):return tomllib.loads(path.read_text()) if path.exists() else {}
def read(path):
    data=document(path);video=data.get('video',{});values=DEFAULTS.copy()
    for key in DEFAULTS.keys()-KEYS.keys():
        if key in data.get('revolution_native',{}):values[key]=data['revolution_native'][key]
    for k,v in KEYS.items():
        if v in video:values[k]=video[v]
    values['fullscreen']=int(values['fullscreen'])
    # Advanced settings may use a window width outside the service presets.
    if values['width'] not in CHOICES['width']:values['width']=min(CHOICES['width'],key=lambda w:abs(w-values['width']))
    return validate(values)
def literal(v):
    if isinstance(v,bool):return 'true' if v else 'false'
    if isinstance(v,str):return json.dumps(v,ensure_ascii=False)
    if isinstance(v,int):return str(v)
    if isinstance(v,float) and math.isfinite(v):return repr(v)
    if isinstance(v,(datetime.datetime,datetime.date,datetime.time)):return v.isoformat()
    if isinstance(v,list):return '['+', '.join(map(literal,v))+']'
    if isinstance(v,dict):return '{'+', '.join(json.dumps(k)+' = '+literal(x) for k,x in v.items())+'}'
    raise ValueError('Unsupported TOML value')
def save(path,values):
    # A launcher opened before a rebuild can still send the previous schema.
    # Preserve saved values for newly introduced controls it cannot display.
    if set(DEFAULTS)-set(values) and set(DEFAULTS)-set(values)<= {'frameGraph','fullScene','nativeCarDistance','rewind'}:
        current=read(path)
        values={**{k:current[k] for k in set(DEFAULTS)-set(values)},**values}
    validate(values);data=document(path);video=data.setdefault('video',{})
    for k,v in KEYS.items():video[v]=values[k]
    data['revolution_native']={k:values[k] for k in DEFAULTS.keys()-KEYS.keys()}
    video.update(renderer='opengl',aspect_ratio='4:3',adaptive_view=False,frame_interpolation=False,frame_interpolation_fps=0,crt_filter='raw',scanlines=False,perspective_texturing=False)
    content='# Saved by Ridge Racer Revolution Service Menu\n'+'\n'.join(json.dumps(k)+' = '+literal(v) for k,v in data.items())+'\n'
    if tomllib.loads(content)!=data:raise ValueError('Settings serialization failed')
    path.parent.mkdir(parents=True,exist_ok=True);fd,tmp=tempfile.mkstemp(dir=path.parent,prefix='.revolution-settings-')
    try:
        with os.fdopen(fd,'w') as f:f.write(content);f.flush();os.fsync(f.fileno())
        os.replace(tmp,path)
    finally:
        if os.path.exists(tmp):os.unlink(tmp)

def main():
    p=argparse.ArgumentParser();p.add_argument('action',choices=['read','save','launch','advanced']);p.add_argument('--root',type=Path,required=True);args=p.parse_args()
    root=args.root.resolve();settings=root/'build-macos/settings.toml'
    if args.action=='read':print(json.dumps(read(settings)));return
    settings.parent.mkdir(exist_ok=True)
    with (settings.parent/'.service-menu.lock').open('a') as lock:
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:raise RuntimeError('Close the running Revolution game before saving or launching again.')
        if args.action=='save':save(settings,json.load(sys.stdin));return
        binary=root/'build-macos/RidgeRacerRevolution';cue=root/'disc-images/Ridge Racer Revolution (USA).cue'
        if not binary.is_file() or not cue.is_file():raise RuntimeError('Build the project and check disc-images first.')
        log=root/'diagnostics/launcher-game.log';log.parent.mkdir(exist_ok=True)
        env=os.environ.copy()
        for name in ['REVOLUTION_TEST_INPUT','REVOLUTION_SCENE_CAPTURE','REVOLUTION_SCENE_DUMP_PREFIX','REVOLUTION_SCENE_FILE']:env.pop(name,None)
        env['RIDGERACERREVOLUTION_BUILD_DIR']=str(root/'build-macos')
        with log.open('a') as out:
            values=read(settings)
            if args.action=='launch' and values['nativeScene']:
                from native_scene import launch
                launch(root,values,env,out);return
            result=subprocess.run([str(binary),'--game',str(root/'game.toml'),'--disc',str(cue),'--debug-port','9946','--memcard-dir',str(root/'saves'),'--launcher' if args.action=='advanced' else '--no-launcher'],cwd=root,env=env,stdout=out,stderr=out)
        if result.returncode:raise RuntimeError(f'Game exited with code {result.returncode}. See {log}')
if __name__=='__main__':
    try:main()
    except Exception as e:print(str(e),file=sys.stderr);sys.exit(1)
