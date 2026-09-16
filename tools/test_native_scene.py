#!/usr/bin/env python3
"""Run the live native renderer with a private game instance and scripted pad."""
import argparse,json,os,shutil,subprocess,tempfile,time
from pathlib import Path
from smoke_boot import request
ROOT=Path(__file__).resolve().parents[1]
def stop(p):
    if p is not None and p.poll() is None:
        p.terminate()
        try:p.wait(timeout=5)
        except subprocess.TimeoutExpired:p.kill();p.wait()
def main():
    p=argparse.ArgumentParser();p.add_argument('name');p.add_argument('--manual-input',action='store_true',help='Use the enhanced window keyboard instead of scripted input');p.add_argument('--vsync',choices=['on','off'],default='off');p.add_argument('--frame-graph',action='store_true');p.add_argument('--fullscreen',action='store_true');p.add_argument('--fps',type=int,default=60);p.add_argument('--seconds',type=int,default=90);p.add_argument('--width',type=int,default=1280);p.add_argument('--height',type=int,default=960);a=p.parse_args()
    if Path(a.name).name!=a.name:raise ValueError('Invalid run name')
    out=ROOT/'diagnostics'/a.name;out.mkdir(exist_ok=False)
    exe=out/'RidgeRacerRevolution';shutil.copy2(ROOT/'build-macos/RidgeRacerRevolution',exe)
    for name in ['assets','bios']:(out/name).symlink_to(ROOT/'build-macos'/name,target_is_directory=True)
    shutil.copytree(ROOT/'build-macos/mods/bundled',out/'mods/bundled')
    (out/'settings.toml').write_text('[video]\nrenderer="opengl"\nsupersampling=1\nwindow_width=640\nfullscreen=0\nframe_interpolation=false\n')
    game=native=None
    with tempfile.TemporaryDirectory(prefix='rrv-',dir='/tmp') as tmp,(out/'game.log').open('w') as glog,(out/'native.log').open('w') as nlog:
        path=str(Path(tmp)/'scene');env=dict(os.environ,REVOLUTION_SCENE_FILE=path,REVOLUTION_TEST_INPUT='1',PSX_HIDDEN_COMPANION='1',SDL_RENDER_DRIVER='opengl')
        if a.manual_input:env.pop('REVOLUTION_TEST_INPUT',None)
        try:
            game=subprocess.Popen([str(exe),'--game',str(ROOT/'game.toml'),'--disc',str(ROOT/'disc-images/Ridge Racer Revolution (USA).cue'),'--debug-port','9947','--no-launcher','--memcard-dir',str(out/'saves')],cwd=ROOT,env=env,stdout=glog,stderr=glog)
            native=subprocess.Popen([str(ROOT/'build-macos/RevolutionNative'),'--shared',path,'--assets',str(ROOT/'build-macos/native-scene'),'--fps',str(a.fps),'--width',str(a.width),'--height',str(a.height),'--vsync',a.vsync,*(['--frame-graph'] if a.frame_graph else []),*(['--fullscreen'] if a.fullscreen else []),'--seconds',str(a.seconds),'--metrics',str(out/'frames.csv'),'--shot',str(out/'native.bmp'),'--shot-at',str(a.seconds-10)],cwd=ROOT,stdout=nlog,stderr=nlog)
            start=time.monotonic();shot=False;boot=False
            while native.poll() is None and time.monotonic()-start<a.seconds+20:
                if game.poll() is not None:raise RuntimeError(f'Game exited: {game.returncode}')
                if not a.manual_input and not boot and time.monotonic()-start>3:
                    for command,args in [('input_route_clear',{}),('input_route_append',dict(frames=20,buttons=65527)),('input_route_append',dict(frames=20,buttons=65535)),('input_route_start',{})]:request(9947,command,**args)
                    boot=True
                if not shot and time.monotonic()-start>a.seconds-10:
                    (out/'runtime-metrics.json').write_text(json.dumps(dict(screenshot=request(9947,'screenshot',path=str(out/'original.bmp')),perf=request(9947,'frame_perf')),indent=2));shot=True
                time.sleep(.1)
            if native.poll()!=0:raise RuntimeError(f'Native renderer failed: {native.poll()}')
        finally:stop(native);stop(game)
    if not (out/'native.bmp').is_file():raise RuntimeError("No native screenshot was produced")
    print(out)
if __name__=='__main__':main()
