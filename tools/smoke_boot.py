#!/usr/bin/env python3
"""Isolated boot/input smoke capture; owns and terminates only its child runtime."""
import argparse,json,os,shutil,socket,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
def request(port,cmd,**args):
    with socket.create_connection(('127.0.0.1',port),timeout=8) as connection:
        connection.sendall((json.dumps(dict(id=1,cmd=cmd,**args))+'\n').encode())
        return json.loads(connection.makefile().readline())
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('name');p.add_argument('--seconds',type=int,default=90);p.add_argument('--port',type=int,default=9947);p.add_argument('--headless',action='store_true');p.add_argument('--scene',action='store_true');p.add_argument('--guest-frames',action='store_true');a=p.parse_args()
    if Path(a.name).name!=a.name:raise ValueError('Use a single run name')
    directory=ROOT/'diagnostics'/a.name;directory.mkdir(exist_ok=False,parents=True)
    # Private exe directory also isolates settings.toml and mod state.
    binary=directory/'RidgeRacerRevolution';shutil.copy2(ROOT/'build-macos/RidgeRacerRevolution',binary)
    for name in ['assets','bios']:(directory/name).symlink_to(ROOT/'build-macos'/name,target_is_directory=True)
    shutil.copytree(ROOT/'build-macos/mods/bundled',directory/'mods/bundled')
    if (ROOT/'build-macos/settings.toml').exists():shutil.copy2(ROOT/'build-macos/settings.toml',directory/'settings.toml')
    cmd=[str(binary),'--game',str(ROOT/'game.toml'),'--disc',str(ROOT/'disc-images/Ridge Racer Revolution (USA).cue'),'--no-launcher','--debug-port',str(a.port),'--memcard-dir',str(directory/'state')]
    if a.headless:cmd.append('--headless')
    with (directory/'runtime.log').open('w') as log:
        env=os.environ.copy()
        if a.scene:env.update(REVOLUTION_SCENE_CAPTURE=str(directory/'scene.jsonl'),REVOLUTION_SCENE_DUMP_PREFIX=str(directory/'snapshot'))
        child=subprocess.Popen(cmd,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT)
        try:
            start=time.monotonic();pressed=set();captured=set()
            elapsed=0
            while elapsed<a.seconds and time.monotonic()-start<max(300,a.seconds*3):
                if a.guest_frames:
                    try:elapsed=request(a.port,'frame')['frame']/59.94
                    except (ConnectionError,OSError):time.sleep(.1);continue
                else:elapsed=time.monotonic()-start
                if child.poll() is not None:raise RuntimeError(f'Runtime exited: {child.returncode}')
                event=next((t for t in [3,12,24,38,48,58] if elapsed>=t and t not in pressed),None)
                if event is not None:
                    for command,args in [('input_route_clear',{}),('input_route_append',dict(frames=8,buttons=65527)),('input_route_append',dict(frames=20,buttons=65535)),('input_route_start',{})]:
                        request(a.port,command,**args)
                    pressed.add(event)
                shot=next((t for t in [16,30,44,54,64,80] if elapsed>=t and t not in captured),None)
                if shot is not None:
                    result=dict(seconds=elapsed,frame=request(a.port,'frame'),registers=request(a.port,'get_registers'),screenshot=request(a.port,'screenshot',path=str(directory/f'{shot}.bmp')))
                    print(json.dumps(result),flush=True);captured.add(shot)
                time.sleep(.15)
        finally:
            if child.poll() is None:
                child.terminate()
                try:child.wait(timeout=5)
                except subprocess.TimeoutExpired:child.kill();child.wait()
if __name__=='__main__':main()
