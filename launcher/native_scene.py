"""Own the independent renderer and its private runtime as one launch."""
import datetime,json,os,shutil,subprocess,tempfile,time
from pathlib import Path
from settings import document,literal

def stop(process):
    if process is not None and process.poll() is None:
        process.terminate()
        try:process.wait(timeout=5)
        except subprocess.TimeoutExpired:process.kill();process.wait()

def launch(root,values,env,log):
    viewer=root/'build-macos/RevolutionNative';assets=root/'build-macos/native-scene'
    if not viewer.is_file() or any(not (assets/f'course-{i}.rrassets').is_file() for i in range(4)):
        raise RuntimeError('Native renderer assets are missing. Run sh scripts/build-macos.sh.')
    recording=root/'diagnostics/frame-times'/datetime.datetime.now().strftime('%Y%m%d-%H%M%S-%f');recording.mkdir(parents=True)
    (recording/'settings.json').write_text(json.dumps(values,indent=2)+'\n')
    with tempfile.TemporaryDirectory(prefix='revolution-',dir='/tmp') as directory:
        host=Path(directory);binary=host/'RidgeRacerRevolution';shutil.copy2(root/'build-macos/RidgeRacerRevolution',binary)
        for name in ['assets','bios']:(host/name).symlink_to(root/'build-macos'/name,target_is_directory=True)
        shutil.copytree(root/'build-macos/mods/bundled',host/'mods/bundled')
        if (root/'build-macos/mods/state.toml').exists():shutil.copy2(root/'build-macos/mods/state.toml',host/'mods/state.toml')
        config=document(root/'build-macos/settings.toml');config.setdefault('video',{}).update(supersampling=1,window_width=640,fullscreen=0,frame_interpolation=False,frame_interpolation_fps=0)
        (host/'settings.toml').write_text('\n'.join(json.dumps(k)+' = '+literal(v) for k,v in config.items())+'\n')
        child_env=dict(env,REVOLUTION_SCENE_FILE=str(host/'scene'),REVOLUTION_FULL_SCENE='1' if values['fullScene'] else '0',REVOLUTION_CAR_DISTANCE=str(values['nativeCarDistance']))
        for name in ['REVOLUTION_TEST_INPUT','REVOLUTION_SCENE_CAPTURE','REVOLUTION_SCENE_DUMP_PREFIX']:child_env.pop(name,None)
        command=[str(viewer),'--shared',str(host/'scene'),'--assets',str(assets),'--metrics',str(recording/'frames.csv'),'--fps',str(values['nativeFps']),'--width',str(values['nativeWidth']),'--height',str(values['nativeHeight']),'--vsync',values['vsync'],'--filter',values['filtering']]
        if values['frameGraph']:command.append('--frame-graph')
        if values['fullscreen']:command.append('--fullscreen')
        if not values['perspective']:command.append('--affine')
        game=native=None
        try:
            game=subprocess.Popen([str(binary),'--game',str(root/'game.toml'),'--disc',str(root/'disc-images/Ridge Racer Revolution (USA).cue'),'--debug-port','9946','--memcard-dir',str(root/'saves'),'--no-launcher'],cwd=root,env=child_env,stdout=log,stderr=log)
            native=subprocess.Popen(command,cwd=root,env=child_env,stdout=log,stderr=log)
            while game.poll() is None and native.poll() is None:time.sleep(.1)
            for process,name in [(game,'Game'),(native,'Native renderer')]:
                if process.poll() not in (None,0):raise RuntimeError(f'{name} exited with code {process.returncode}; see diagnostics/launcher-game.log')
        finally:
            stop(native);stop(game)
            if (recording/'frames.csv').exists():
                try:subprocess.run([os.sys.executable,str(root/'tools/analyze_frame_times.py'),str(recording/'frames.csv'),'--fps',str(values['nativeFps'] or 60)],cwd=root,stdout=log,stderr=log,timeout=20)
                except (OSError,subprocess.TimeoutExpired):pass
