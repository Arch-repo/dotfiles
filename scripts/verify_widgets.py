"""Prove native widget interaction and unchanged keyboard focus in nested Wayland."""
from datetime import datetime
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import time

root=Path(__file__).resolve().parents[1]
def run(*args,env=None):return subprocess.check_output(list(map(str,args)),env=env,text=True)
original=json.loads(run('hyprctl','-j','activeworkspace'))['name']
occupied={x['id'] for x in json.loads(run('hyprctl','-j','workspaces')) if x['windows']}
workspace=next(x for x in range(51,80) if x not in occupied)
with tempfile.TemporaryDirectory(prefix='anto-widgets-wayland-') as temporary:
    folder=Path(temporary);home=folder/'home';assets=home/'.local/share/anto-desktop';assets.mkdir(parents=True)
    for source in (root/'build/design').glob('*.css'):shutil.copyfile(source,assets/source.name)
    shutil.copyfile(root/'native/menu/assets/tokens.css',assets/'tokens.css')
    # Numeric DSP fixture exercises the real async reader and GTK graph;
    # the real PipeWire/CAVA path is verified separately on a private sink.
    dsp=folder/'bin';dsp.mkdir();fake=dsp/'cava'
    fake.write_text('#!/usr/bin/python3\nimport math,time\nwhile True:\n print(";".join(str(int(100+800*abs(math.sin(i*.3+time.monotonic())))) for i in range(32)),flush=True)\n time.sleep(.033)\n')
    fake.chmod(0o700)
    env_file=folder/'environment.json';save=folder/'save.py'
    save.write_text('import json,os,pathlib\npathlib.Path('+repr(str(env_file))+').write_text(json.dumps({key:os.environ.get(key,"") for key in ["WAYLAND_DISPLAY","HYPRLAND_INSTANCE_SIGNATURE","DBUS_SESSION_BUS_ADDRESS"]}))\n')
    config=folder/'hyprland.conf';config.write_text('monitor = , 1280x800, 0x0, 1\nanimations {\n enabled = false\n}\n'
        f'source = {Path.home()}/.config/anto426-local/hypr/glass-loader.conf\nsource = {root}/.config/hypr/conf/glass.conf\n'
        'windowrule = float on, match:title ^Widget focus probe$\nwindowrule = size 420 100, match:title ^Widget focus probe$\nwindowrule = move 700 670, match:title ^Widget focus probe$\n'
        f'exec-once = python3 {save}\n')
    base=dict(os.environ,HOME=str(home),XDG_CONFIG_HOME=str(folder/'config'),XDG_CACHE_HOME=str(folder/'cache'),XDG_DATA_HOME=str(folder/'data'),XDG_STATE_HOME=str(folder/'state'),ANTO_LOCAL_CONFIG_ROOT=str(folder/'config/anto426-local'),HYPRLAND_NO_RT='1',GIO_USE_VFS='local')
    compositor=fixture=None;run('hyprctl','dispatch','workspace',workspace)
    with (folder/'runtime.log').open('w') as log:
        try:
            compositor=subprocess.Popen(['dbus-run-session','--','Hyprland','--config',str(config)],env=base,stdout=log,stderr=log)
            deadline=time.monotonic()+12
            while not env_file.exists() and compositor.poll() is None and time.monotonic()<deadline:time.sleep(.1)
            assert env_file.exists(),'Nested compositor did not start'
            child=dict(base,**json.loads(env_file.read_text()),GDK_BACKEND='wayland',GTK_A11Y='none',ANTO_WIDGET_TEST_AUDIO='1',PATH=str(dsp)+':'+os.environ['PATH'],ANTO_WIDGET_READY=str(folder/'ready'),ANTO_WIDGET_CAPTURE=str(folder/'widgets.png'))
            def hypr(*args):return run('hyprctl','-i',child['HYPRLAND_INSTANCE_SIGNATURE'],*args,env=child)
            time.sleep(.4);assert not hypr('configerrors').strip()
            deadline=time.monotonic()+4
            monitors=json.loads(hypr('-j','monitors'))
            while not monitors and time.monotonic()<deadline:
                time.sleep(.05);monitors=json.loads(hypr('-j','monitors'))
            assert monitors,'Nested Wayland output did not appear'
            connector=monitors[-1]['name']
            for item in monitors[:-1]:hypr('output','remove',item['name'])
            child['ANTO426_TARGET_MONITOR']=connector;hypr('dismissnotify')
            fixture=subprocess.Popen([str(root/'build/widget-ui-fixture')],env=child,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True)
            deadline=time.monotonic()+6
            while not (folder/'ready').exists() and fixture.poll() is None and time.monotonic()<deadline:time.sleep(.02)
            assert (folder/'ready').exists(),fixture.communicate(timeout=2)
            focused_before=json.loads(hypr('-j','activewindow'));assert focused_before.get('title')=='Widget focus probe',focused_before
            layers=json.loads(hypr('-j','layers'));cards=[item for screen in layers.values() for level in screen.get('levels',{}).values() for item in level if item['namespace']=='anto426-widget'];assert len(cards)==4,cards
            x,y=map(int,(folder/'ready').read_text().split())
            shutil.copyfile(folder/'widgets.png',root/'build/widgets-probe.png')
            screen=monitors[-1]
            run(root/'build/widget-pointer-fixture',x,y,screen['width'],screen['height'],env=child)
            focused_after=json.loads(hypr('-j','activewindow'));assert focused_after.get('address')==focused_before['address'],focused_after
            stdout,stderr=fixture.communicate(timeout=8);assert fixture.returncode==0,stdout+stderr
            assert 'WARNING' not in stderr and 'CRITICAL' not in stderr,stderr
            shutil.copyfile(folder/'widgets.png',root/'docs/screenshots/widgets.png')
            report=dict(verified_at=datetime.now().astimezone().isoformat(),passed=True,session='nested-wayland',personal_data=False,widget_count=len(cards),native_bottom_layer=True,real_pointer_click=True,mpris_play_pause=True,keyboard_focus_preserved=True,source='native/widgets/tests/ui.c',fixture_output=stdout.strip())
            (root/'docs/widgets-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
        finally:
            if fixture and fixture.poll() is None:fixture.terminate();fixture.wait(timeout=3)
            if compositor:compositor.terminate();compositor.wait(timeout=3)
            if json.loads(run('hyprctl','-j','activeworkspace'))['id']==workspace:run('hyprctl','dispatch','workspace',original)
