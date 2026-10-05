#!/usr/bin/env python3
"""Exercise shipped Zen chrome and internal pages without touching real profiles."""
from pathlib import Path
import json
import os
import socket
import subprocess
import tempfile
import time
from PIL import Image, ImageChops, ImageStat
from marionette_driver.marionette import Marionette

ROOT = Path(__file__).resolve().parents[1]


def run(*args, env=None):
    return subprocess.check_output(list(map(str, args)), env=env, text=True, timeout=10)


def main():
    processes = []
    with tempfile.TemporaryDirectory(prefix='anto-zen-wayland-') as directory:
        temp = Path(directory)
        saved = temp / 'environment.json'
        save = temp / 'save.py'
        save.write_text('import os,json,pathlib\npathlib.Path(' + repr(str(saved)) + ').write_text(json.dumps({k:os.environ.get(k,"") for k in ["WAYLAND_DISPLAY","HYPRLAND_INSTANCE_SIGNATURE","DBUS_SESSION_BUS_ADDRESS"]}))\n')
        config = temp / 'hyprland.conf'
        config.write_text('monitor = ,1280x800,0x0,1\nanimations {\n enabled = false\n}\ngeneral {\n border_size = 0\n}\n'
                          + f'source = {Path.home()}/.config/anto426-local/hypr/glass-loader.conf\nsource = {ROOT}/.config/hypr/conf/glass.conf\nsource = {ROOT}/.config/hypr/conf/windowrule.conf\n'
                          + f'exec-once = python3 {save}\n')
        with (ROOT / 'build/zen-theme-runtime.log').open('w') as log:
            try:
                compositor = subprocess.Popen(['dbus-run-session', '--', 'Hyprland', '--config', str(config)], env=dict(os.environ, HYPRLAND_NO_RT='1', HYPRLAND_NO_SD_VARS='1'), stdout=log, stderr=log)
                processes.append(compositor)
                deadline = time.monotonic() + 12
                while not saved.exists() and compositor.poll() is None and time.monotonic() < deadline:
                    time.sleep(.1)
                assert saved.exists(), 'Private compositor did not start'
                child = dict(os.environ, **json.loads(saved.read_text()), HOME=str(temp), XDG_CONFIG_HOME=str(temp / '.config'), XDG_DATA_HOME=str(temp / '.local/share'), XDG_CACHE_HOME=str(temp / '.cache'), XDG_STATE_HOME=str(temp / '.local/state'), MOZ_ENABLE_WAYLAND='1', GDK_BACKEND='wayland', GTK_A11Y='none', NO_AT_BRIDGE='1', MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1', ANTO426_UNDERLAY_CONTROL=str(temp / 'phase'))
                child.pop('DISPLAY', None)
                child.pop('GTK_THEME', None)
                def hypr(*args): return run('hyprctl', *args, env=child)
                connector = 'ANTO-ZEN-TEST'
                hypr('output', 'create', 'headless', connector)
                for monitor in json.loads(hypr('-j', 'monitors')):
                    if monitor['name'] != connector: hypr('output', 'remove', monitor['name'])
                hypr('dispatch', 'focusmonitor', connector)
                hypr('dismissnotify')
                child['ANTO426_TARGET_MONITOR'] = connector
                assert not hypr('configerrors').strip(), hypr('configerrors')
                profile = temp / '.config/zen/synthetic'
                profile.mkdir(parents=True)
                (profile.parent / 'profiles.ini').write_text('[Profile0]\nName=Synthetic\nIsRelative=1\nPath=synthetic\nDefault=1\n')
                resources = Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share')) / 'anto-desktop'
                palette = resources / 'vscode-theme/default.json'
                expected = json.loads(palette.read_text())
                artifact = temp / 'theme'
                subprocess.run(['python3', str(resources / 'zen-browser/render.py'), '--palette', str(palette), '--material', str(resources / 'application-material.json'), '--output', str(artifact)], env=child, check=True)
                subprocess.run(['python3', str(ROOT / 'support/compat/app_themes.py'), 'zen', str(artifact), str(palette)], env=child, check=True)
                with socket.socket() as listener:
                    listener.bind(('127.0.0.1', 0)); port = listener.getsockname()[1]
                with (profile / 'user.js').open('a') as prefs:
                    for key, value in {'marionette.port': port, 'zen.welcome-screen.seen': True, 'browser.shell.checkDefaultBrowser': False, 'browser.aboutwelcome.enabled': False, 'browser.startup.homepage': 'about:blank', 'browser.startup.page': 0}.items():
                        prefs.write(f'user_pref({json.dumps(key)}, {json.dumps(value)});\n')
                (temp / 'phase').write_text('0')
                processes.append(subprocess.Popen([str(ROOT / 'build/live-underlay-fixture')], env=child, stdout=log, stderr=log))
                browser = subprocess.Popen(['zen-browser', '--new-instance', '--profile', str(profile), '--marionette', '--remote-allow-system-access', 'about:blank'], env=child, stdout=log, stderr=log)
                processes.append(browser)
                client = Marionette(host='127.0.0.1', port=port, socket_timeout=15)
                client.raise_for_port(timeout=20)
                client.start_session()
                client.set_context('chrome')
                deadline = time.monotonic() + 10
                while time.monotonic() < deadline:
                    ready = client.execute_script('return !!document.querySelector("#zen-browser-background") && !document.documentElement.hasAttribute("zen-before-loaded");')
                    if ready: break
                    time.sleep(.2)
                assert ready, 'Zen chrome did not finish loading'
                chrome = client.execute_script('''
const root=getComputedStyle(document.documentElement);
const bg=document.querySelector('#zen-browser-background');
const tab=document.querySelector('.tabbrowser-tab[visuallyselected] .tab-background');
return {accent:root.getPropertyValue('--zen-primary-color').trim(),
 material:root.getPropertyValue('--anto-ui-panel').trim(),
 background:getComputedStyle(bg,'::after').backgroundColor,
 text:root.getPropertyValue('--toolbox-textcolor').trim(),
 fieldRadius:getComputedStyle(document.querySelector('.urlbar-background')).borderRadius,
 tabRadius:tab&&getComputedStyle(tab).borderRadius,
 notification:root.getPropertyValue('--zen-sidebar-notification-bg').trim(),
 linuxTransparency:Services.prefs.getBoolPref('zen.widget.linux.transparency'),
 transparentBrowser:Services.prefs.getBoolPref('browser.tabs.allow_transparent_browser')};
''')
                assert chrome['accent'] == expected['accent'] and chrome['text'] == expected['foreground'], chrome
                assert chrome['fieldRadius'] == '12px' and chrome['tabRadius'] == '12px', chrome
                assert chrome['linuxTransparency'] and chrome['transparentBrowser'], chrome
                assert chrome['background'].endswith(', 0.46)'), chrome
                assert chrome['notification'] == expected['surface'], chrome
                windows = json.loads(hypr('-j', 'clients'))
                window = next(c for c in windows if c['pid'] == browser.pid or c['class'] == 'zen')
                address = 'address:' + window['address']
                hypr('dispatch', 'setfloating', address)
                hypr('dispatch', 'resizewindowpixel', 'exact 1100 700,' + address)
                hypr('dispatch', 'movewindowpixel', 'exact 90 50,' + address)
                (temp / 'phase').write_text('1')
                time.sleep(.4)
                hypr('hyprglass', 'stats', 'reset')
                images = []
                for phase in ('0', '1'):
                    (temp / 'phase').write_text(phase); time.sleep(.4)
                    shot = temp / (phase + '.png')
                    subprocess.run(['grim', '-o', connector, str(shot)], env=child, check=True, timeout=10)
                    images.append(Image.open(shot).convert('RGB'))
                difference = ImageStat.Stat(ImageChops.difference(images[0].crop((120, 350, 180, 450)), images[1].crop((120, 350, 180, 450)))).mean
                chrome['backgroundResponse'] = round(sum(difference) / 3, 2)
                stats = json.loads(hypr('-j', 'hyprglass', 'stats'))
                chrome['glassDraws'] = sum(m['windowGlassDraws'] for m in stats['monitors'])
                chrome['liveSamples'] = sum(m['windowCacheMisses'] for m in stats['monitors'])
                assert chrome['backgroundResponse'] > 6 and chrome['glassDraws'] > 0 and chrome['liveSamples'] >= 2, chrome
                images[0].crop((90, 50, 1190, 750)).save(ROOT / 'docs/screenshots/zen-theme.png')
                client.execute_script('PanelUI.show();')
                time.sleep(.3)
                popup = client.execute_script('''
const p=document.querySelector('#appMenu-popup');
return {state:p.state,background:getComputedStyle(p).getPropertyValue('--panel-background-color').trim(),
 text:getComputedStyle(p).getPropertyValue('--panel-color').trim(),radius:getComputedStyle(p).getPropertyValue('--panel-border-radius').trim()};
''')
                assert popup['state'] == 'open' and popup['text'] == expected['foreground'] and popup['radius'] == '18px', popup
                client.execute_script('PanelUI.hide();')
                client.set_context('content')
                client.navigate('about:preferences')
                time.sleep(1)
                preferences = client.execute_script('''
const s=getComputedStyle(document.documentElement);
const input=document.querySelector('input')||document.querySelector('moz-input');
return {accent:s.getPropertyValue('--color-accent-primary').trim(),
 text:s.getPropertyValue('--text-color').trim(),
 cardRadius:s.getPropertyValue('--anto-radius-card').trim(),
 primaryText:s.getPropertyValue('--button-text-color-primary').trim(),
 input:!!input};
''')
                assert preferences['accent'] == expected['accent'] and preferences['text'] == expected['foreground'] and preferences['cardRadius'] == '18px' and preferences['primaryText'] == expected['selected_fg'], preferences
                screenshot = temp / 'preferences.png'
                subprocess.run(['grim', '-o', connector, str(screenshot)], env=child, check=True, timeout=10)
                Image.open(screenshot).crop((90, 50, 1190, 750)).save(ROOT / 'docs/screenshots/zen-preferences.png')
                client.navigate('data:text/html,<html><body style="background:rgb(250,240,230);color:rgb(34,17,51)">Ordinary synthetic website</body></html>')
                website = client.execute_script('return {background:getComputedStyle(document.body).backgroundColor,text:getComputedStyle(document.body).color,token:getComputedStyle(document.documentElement).getPropertyValue("--anto-accent")};')
                assert website == {'background': 'rgb(250, 240, 230)', 'text': 'rgb(34, 17, 51)', 'token': ''}, website
                report = {'version': client.session_capabilities['browserVersion'], 'chrome': chrome, 'popup': popup, 'preferences': preferences, 'websiteColorsPreserved': True, 'privateProfile': True, 'passed': True}
                (ROOT / 'docs/zen-theme-verification.json').write_text(json.dumps(report, indent=2) + '\n')
                print(json.dumps(report))
                client.delete_session()
            finally:
                for process in reversed(processes):
                    if process.poll() is None:
                        process.terminate()
                        try:
                            process.wait(timeout=5)
                        except subprocess.TimeoutExpired:
                            process.kill(); process.wait(timeout=5)


if __name__ == '__main__':
    main()
