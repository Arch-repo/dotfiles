#!/usr/bin/env python3
"""Render the real greeter/locker in a private nested compositor, never the session."""
from datetime import datetime
from pathlib import Path
import json
import os
import shutil
import subprocess
import tempfile
import time

ROOT=Path(__file__).resolve().parents[1]
def run(*args,env=None):
    return subprocess.check_output(list(map(str,args)),env=env,text=True)

def main():
    original=json.loads(run('hyprctl','-j','activeworkspace'))['name']
    occupied={item['id'] for item in json.loads(run('hyprctl','-j','workspaces')) if item['windows']}
    workspace=next(item for item in range(51,80) if item not in occupied)
    before={item['address'] for item in json.loads(run('hyprctl','-j','clients'))}
    with tempfile.TemporaryDirectory(prefix='anto-auth-wayland-') as temporary:
        folder=Path(temporary);saved=folder/'environment.json';save=folder/'save.py'
        save.write_text('import os,json,pathlib\npathlib.Path('+repr(str(saved))+').write_text(json.dumps({k:os.environ.get(k,"") for k in ["WAYLAND_DISPLAY","HYPRLAND_INSTANCE_SIGNATURE","DBUS_SESSION_BUS_ADDRESS"]}))\n')
        config=folder/'hyprland.conf';config.write_text('monitor = , 1280x800, 0x0, 1\nanimations {\n enabled = false\n}\ngeneral {\n border_size = 0\n}\n'+f'exec-once = python3 {save}\n')
        log=(folder/'compositor.log').open('w');child_log=(folder/'auth.log').open('w')
        compositor=greeter=locker=None
        run('hyprctl','dispatch','workspace',workspace)
        try:
            compositor=subprocess.Popen(['dbus-run-session','--','Hyprland','--config',str(config)],env=dict(os.environ,HYPRLAND_NO_RT='1'),stdout=log,stderr=log)
            deadline=time.monotonic()+12
            while not saved.exists() and compositor.poll() is None and time.monotonic()<deadline:time.sleep(.1)
            assert saved.exists(),'Nested compositor failed to start'
            child=dict(os.environ,**json.loads(saved.read_text()),QT_QPA_PLATFORM='wayland',QT_QUICK_CONTROLS_STYLE='Basic',QT_QPA_PLATFORMTHEME='',QT_STYLE_OVERRIDE='',QT_FORCE_STDERR_LOGGING='1')
            child['QT_LOGGING_RULES']='*.debug=false;*.warning=true;*.critical=true'
            def hypr(*args):return run('hyprctl','-i',child['HYPRLAND_INSTANCE_SIGNATURE'],*args,env=child)
            time.sleep(.6)
            hypr('dismissnotify')
            monitors=json.loads(hypr('-j','monitors'));assert monitors
            for monitor in monitors[:-1]:hypr('output','remove',monitor['name'])
            assert not hypr('configerrors').strip()
            theme=folder/'login';shutil.copytree(ROOT/'native/login',theme,ignore=shutil.ignore_patterns('tests'))
            shutil.copyfile(ROOT/'build/design/Tokens.qml',theme/'Tokens.qml')
            bg=folder/'background.png';run('magick','-size','1280x800','gradient:#233e65-#ad607d',bg)
            (theme/'theme.conf.user').write_text(f'[General]\nBackground={bg}\n')
            greeter=subprocess.Popen(['sddm-greeter-qt6','--test-mode','--theme',str(theme)],env=child,stdout=child_log,stderr=child_log)
            time.sleep(2);assert greeter.poll() is None,'The real SDDM greeter exited'
            screenshots=ROOT/'docs/screenshots';screenshots.mkdir(exist_ok=True)
            outer=[item for item in json.loads(run('hyprctl','-j','clients')) if item['address'] not in before ]
            assert outer, 'Nested window not found'
            window=outer[-1]
            def capture(name):
                current=next(item for item in json.loads(run('hyprctl','-j','clients')) if item['address']==window['address'])
                run('grim','-g',f"{current['at'][0]},{current['at'][1]} {current['size'][0]}x{current['size'][1]}",screenshots/name)
            capture('login.png')
            greeter.terminate();greeter.wait(timeout=3);time.sleep(.2)
            # The real PAM and fingerprint backends remain enabled. Do not
            # enter credentials or disable authentication for a screenshot.
            palette=folder/'palette.conf';palette.write_text((ROOT/'build/design/hyprlock-defaults.conf').read_text()+f'\n$anto426_wallpaper = {bg}\n')
            lockconf=folder/'hyprlock.conf';text=(ROOT/'build/design/hyprlock.conf').read_text()
            text=text.replace('~/.local/share/anto-desktop/hyprlock-defaults.conf',str(ROOT/'build/design/hyprlock-defaults.conf')).replace('~/.config/anto426-local/hypr/theme.generated.conf',str(palette)).replace('$USER','demo')
            lockconf.write_text(text)
            locker=subprocess.Popen(['hyprlock','--config',str(lockconf),'--display',child['WAYLAND_DISPLAY'],'--verbose'],env=child,stdout=child_log,stderr=child_log)
            time.sleep(2);assert locker.poll() is None,'Hyprlock exited during the render test'
            capture('lock.png')
            child_log.flush();logs=(folder/'auth.log').read_text()
            (ROOT/'build/auth-runtime.log').write_text(logs)
            assert 'Config has errors' not in logs and 'QML Error' not in logs and 'ReferenceError' not in logs and 'Binding loop' not in logs, 'Authentication render errors: see build/auth-runtime.log'
            report=dict(verified_at=datetime.now().astimezone().isoformat(),passed=True,session='nested-wayland',sddm_qt6_test_mode=True,hyprlock_pam_enabled=True,fingerprint_parallel_enabled=True,fingerprint_device_ready=('fprint: started verifying' in logs),real_authentication_tested=False,real_boot_tested=False)
            (ROOT/'docs/login-lock-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report))
        finally:
            # First terminate the private compositor, so the locker cannot
            # leave even this test surface locked. Never signal live services.
            if compositor and compositor.poll() is None:compositor.terminate();compositor.wait(timeout=4)
            for process in (greeter,locker):
                if process and process.poll() is None:process.terminate();process.wait(timeout=3)
            log.close();child_log.close()
            if json.loads(run('hyprctl','-j','activeworkspace'))['id']==workspace:run('hyprctl','dispatch','workspace',original)

if __name__=='__main__':main()
