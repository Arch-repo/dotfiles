#!/usr/bin/env python3
"""Exercise real compositor sampling in an isolated nested Wayland session.

Requires installed artifacts, a running Hyprland session and python-pillow.
Only the test session receives synthetic notifications. The original workspace
is restored, and production wallpaper/settings/notification history stay intact.
"""
from pathlib import Path
from datetime import date
import json
import os
import subprocess
import tempfile
import time
from PIL import Image, ImageChops, ImageStat

ROOT = Path(__file__).resolve().parent.parent
LOCAL = Path.home() / '.local'


def run(*args, env=None):
    return subprocess.check_output([str(arg) for arg in args], env=env, text=True)


def main():
    folder = Path(tempfile.mkdtemp(prefix='anto-shell-glass-'))
    env_file = folder / 'environment.json'
    save = folder / 'saveenv.py'
    save.write_text('import json,os,pathlib\npathlib.Path(' + repr(str(env_file)) + ').write_text(json.dumps({key:os.environ.get(key, "") for key in ["WAYLAND_DISPLAY","HYPRLAND_INSTANCE_SIGNATURE","DBUS_SESSION_BUS_ADDRESS"]}))\n')
    config = folder / 'hyprland.conf'
    base = ('monitor = , 1280x800, 0x0, 1\ngeneral {\n border_size = 0\n}\n'
            'animations {\n enabled = false\n}\n')
    base += f'source = {Path.home()}/.config/anto426-local/hypr/glass-loader.conf\nsource = {ROOT}/.config/hypr/conf/glass.conf\nexec-once = python3 {save}\n'
    base += f'source = {LOCAL}/share/anto-desktop/terminal-hypr.conf\n'
    base += 'windowrule = float on, match:class ^com[.]mitchellh[.]ghostty$\nwindowrule = center on, match:class ^com[.]mitchellh[.]ghostty$\nwindowrule = size 900 580, match:class ^com[.]mitchellh[.]ghostty$\n'
    config.write_text(base)
    original = json.loads(run('hyprctl', '-j', 'activeworkspace'))['name']
    occupied = {item['id'] for item in json.loads(run('hyprctl', '-j', 'workspaces')) if item['windows']}
    workspace = next(item for item in range(51, 80) if item not in occupied)
    run('hyprctl', 'dispatch', 'workspace', str(workspace))
    processes = []
    report = {'session': 'nested-wayland', 'surfaces': {}}
    screenshots = ROOT / 'docs/screenshots'
    screenshots.mkdir(exist_ok=True)
    log = open(folder / 'runtime.log', 'w')
    try:
        nested = subprocess.Popen(['dbus-run-session', '--', 'Hyprland', '--config', str(config)],
                                  env=dict(os.environ, HYPRLAND_NO_RT='1'), stdout=log, stderr=log)
        processes.append(nested)
        deadline = time.monotonic() + 12
        while not env_file.exists() and nested.poll() is None and time.monotonic() < deadline:
            time.sleep(.1)
        assert env_file.exists(), f'Nested compositor failed: {folder}'
        child = dict(os.environ, **json.loads(env_file.read_text()), GDK_BACKEND='wayland', GTK_A11Y='none',
                     ANTO426_GLASS_TRACE='1', ANTO426_UNDERLAY_CONTROL=str(folder / 'phase'))
        def hypr(*args):
            return run('hyprctl', '-i', child['HYPRLAND_INSTANCE_SIGNATURE'], *args, env=child)
        time.sleep(2)
        assert not hypr('configerrors').strip(), hypr('configerrors')
        assert 'hyprglass' in hypr('plugin', 'list'), 'Renderer did not load automatically'
        monitors = json.loads(hypr('-j', 'monitors'))
        assert monitors, 'Nested output unavailable'
        connector = monitors[-1]['name']
        for output in monitors[:-1]:
            hypr('output', 'remove', output['name'])
        hypr('dispatch', 'focusmonitor', connector)
        hypr('dismissnotify')
        child['ANTO426_TARGET_MONITOR'] = connector
        (folder / 'phase').write_text('0')
        underlay = subprocess.Popen([str(ROOT / 'build/live-underlay-fixture')], env=child, stdout=log, stderr=log)
        processes.append(underlay)
        time.sleep(.4)
        def start(*args):
            process = subprocess.Popen([str(arg) for arg in args], env=child, stdout=log, stderr=log)
            processes.append(process)
            return process
        def stop(process):
            process.terminate()
            process.wait(timeout=3)
        lifecycle = start(ROOT / 'build/glass-lifecycle-fixture')
        assert lifecycle.wait(timeout=8) == 0, f'Glass lifecycle failed: {folder}'
        report['glass_lifecycle'] = 'realize/unrealize/remap/destroy passed on Wayland'
        geometry = start(ROOT / 'build/menu-geometry-fixture')
        assert geometry.wait(timeout=8) == 0, f'Menu geometry failed: {folder}'
        report['menu_geometry'] = 'fixed panel, sidebar and content across grid/list/custom on Wayland'
        # Backend notification queries must use this session's real daemon,
        # rather than activating a service with the parent display variables.
        notifications = start('swaync','-c',ROOT / '.config/swaync/config.json','-s',LOCAL / 'share/anto-desktop/swaync.css')
        time.sleep(.4)
        def stats():
            return json.loads(hypr('-j', 'hyprglass', 'stats'))
        def capture(name):
            path = folder / (name + '.png')
            subprocess.run(['grim', '-o', connector, str(path)], env=child, check=True)
            return path
        def verify(namespace, image_name, probe, report_name=None):
            deadline = time.monotonic() + 3
            while True:
                layers = json.loads(hypr('-j', 'layers'))
                levels = layers.get(connector, {}).get('levels', {})
                if any(item['namespace'] == namespace for items in levels.values() for item in items):
                    break
                if time.monotonic() >= deadline:
                    (folder / 'missing-layer.json').write_text(json.dumps(layers, indent=2))
                    raise AssertionError(f'{namespace}: layer not mapped on {connector}; evidence: {folder}')
                time.sleep(.05)
            hypr('hyprglass', 'stats', 'reset')
            colours = []
            for phase in (0, 1, 2):
                (folder / 'phase').write_text(str(phase))
                time.sleep(.25)
                path = capture(namespace + str(phase))
                image = Image.open(path).convert('RGB')
                colours.append(image.getpixel(probe(image)))
            counters = stats()
            draws = sum(item['layerGlassDraws'] for item in counters['monitors'])
            misses = sum(item['layerCacheMisses'] for item in counters['monitors'])
            if not (draws > 0 and misses >= 2):
                (folder / (namespace+'-items.json')).write_text(hypr('-j','hyprglass','items'))
                raise AssertionError((namespace,counters,'Evidence: '+str(folder)))
            assert max(abs(colours[0][i] - colours[1][i]) for i in range(3)) > 15, (namespace, colours)
            (screenshots / (image_name + '.png')).write_bytes(path.read_bytes())
            report['surfaces'][report_name or namespace] = {'pixel_colours': colours, 'glass_draws': draws, 'new_samples': misses}
            print(namespace, 'live sampling verified', flush=True)
        menu = start(LOCAL / 'bin/anto-menu', 'system')
        time.sleep(.8)
        verify('anto426-menu', 'system', lambda image: (image.width // 2, int(image.height * .81)))
        (folder / 'phase').write_text('3')
        hypr('hyprglass','stats','reset')
        time.sleep(.15)
        first = Image.open(capture('motion-start')).convert('RGB')
        time.sleep(.25)
        second = Image.open(capture('motion-end')).convert('RGB')
        region = (first.width//2-100,int(first.height*.80),first.width//2+100,int(first.height*.84))
        difference = ImageStat.Stat(ImageChops.difference(first.crop(region),second.crop(region))).mean
        motion_stats = stats()
        samples = sum(item['layerCacheMisses'] for item in motion_stats['monitors'])
        assert max(difference)>2 and samples>=3,(difference,motion_stats)
        report['continuous_motion'] = {'underlay_frequency_hz':20,'new_samples':samples,'mean_pixel_difference':difference}
        # All screen constructors must accept the shared primitives/styles.
        pages = ['apps','audio','wifi','bluetooth','brightness','display','capture','record','hardware',
                 'keyboard','notifications','clipboard','emoji','floating','background','shortcuts','widgets',
                 'settings','calendar','calendar-add','power']
        report['pages_opened'] = []
        for page in pages:
            run(LOCAL / 'bin/anto-menu', page, env=child)
            time.sleep(.14)
            assert menu.poll() is None, page
            report['pages_opened'].append(page)
            if page in ('settings', 'calendar-add', 'audio', 'emoji'):
                (screenshots / (page + '.png')).write_bytes(capture(page).read_bytes())
            elif page in ('wifi','bluetooth','display'):
                (screenshots / (page+'.png')).write_bytes(capture(page).read_bytes())
        stop(menu)
        # Document the day pane with synthetic events, without personal data.
        test_data = folder / 'calendar-data'
        calendar = test_data / 'anto426/calendar'
        calendar.mkdir(parents=True)
        today = date.today().isoformat()
        (calendar / 'events.json').write_text(json.dumps([
            {'id':'1','date':today,'title':'Progetto desktop','all_day':True},
            {'id':'2','date':today,'title':'Revisione del menu','start':'09:00','end':'10:00'},
            {'id':'3','date':today,'title':'Verifica dei componenti','start':'15:00','description':'Aggiornamento dei dati e tema'}]))
        calendar_menu = subprocess.Popen([str(LOCAL / 'bin/anto-menu'), 'calendar'],
            env=dict(child, XDG_DATA_HOME=str(test_data)), stdout=log, stderr=log)
        processes.append(calendar_menu)
        time.sleep(.6)
        assert calendar_menu.poll() is None
        (screenshots / 'calendar.png').write_bytes(capture('calendar-fixture').read_bytes())
        report['calendar_fixture'] = {'selected_day':today, 'events':3, 'personal_data':False}
        stop(calendar_menu)
        bar_config = folder / 'waybar.json'
        bar_config.write_text(json.dumps({'layer':'overlay','position':'top','height':46,'margin-top':12,
            'modules-left':['custom/launcher'], 'modules-center':['group/center-deck'], 'modules-right':['custom/power'],
            'group/center-deck':{'orientation':'horizontal','modules':['clock']},
            'custom/launcher':{'format':'⌘'}, 'clock':{'format':'{:%H:%M}'}, 'custom/power':{'format':'⏻'}}))
        bar = start('waybar','-c',bar_config,'-s',LOCAL / 'share/anto-desktop/waybar.css')
        time.sleep(.6)
        verify('waybar','waybar',lambda image:(int(image.width*.5),20))
        stop(bar)
        run('notify-send','-a','Anto Desktop Test','Materiale della shell','Campionamento reale dello sfondo',env=child)
        time.sleep(.3)
        verify('swaync-notification-window','notification',lambda image:(image.width-160,65))
        run('swaync-client','-op',env=child)
        time.sleep(.4)
        verify('swaync-control-center','notification-center',lambda image:(image.width-180,350))
        stop(notifications)
        osd = start(LOCAL / 'bin/anto-osd','brightness','63')
        time.sleep(.2)
        verify('anto426-osd','osd',lambda image:(image.width//2,int(image.height*.835)))
        if osd.poll() is None:
            stop(osd)
        # Probe the embedded page without covering the colour fixture with a
        # selected wallpaper. The dedicated wallpaper Wayland fixture tests
        # real preview changes and samples them through the menu glass.
        empty_collection = folder / 'empty-wallpapers'
        empty_collection.mkdir()
        child['ANTO426_WALLPAPERS_DIR'] = str(empty_collection)
        gallery = start(LOCAL / 'bin/anto-wallpaper')
        time.sleep(.5)
        verify('anto426-menu','wallpaper-empty',lambda image:(image.width//2,
               image.height//2 + min(740,image.height-80)//2 - 20),
               report_name='anto426-menu:wallpaper')
        report['frontend_routes'] = []
        for binary, page in [('anto-menu','apps'),('anto-wallpaper',None),
                             ('anto-menu','settings'),('anto-wallpaper',None),
                             ('anto-menu','wallpaper')]:
            args = [LOCAL / 'bin' / binary] + ([page] if page else [])
            run(*args, env=child)
            time.sleep(.25)
            assert gallery.poll() is None, 'Changing pages replaced the menu process'
            levels = json.loads(hypr('-j','layers')).get(connector,{}).get('levels',{})
            names = [item['namespace'] for items in levels.values() for item in items]
            assert names.count('anto426-menu') == 1 and 'anto426-wallpaper' not in names, names
            report['frontend_routes'].append({'command':binary,'page':page,
                                              'same_process':True,'one_menu_layer':True})
        run(LOCAL / 'bin/anto-menu','system',env=child)
        time.sleep(.2)
        stop(gallery)
        # Leave fullscreen so a normal floating terminal can cover the fixture.
        hypr('dispatch','focuswindow','class:com.anto426.GlassUnderlay')
        hypr('dispatch','fullscreen','0')
        terminal = start('ghostty','--gtk-single-instance=false','--window-decoration=none',
                         '-e','sh','-c',"printf '%s\\n' 'Anto Desktop' 'Materiale condiviso · testo opaco' ''; sleep 60")
        time.sleep(.8)
        hypr('hyprglass','stats','reset')
        colours = []
        for phase in (0,1,2):
            (folder/'phase').write_text(str(phase));time.sleep(.25)
            path = capture('terminal'+str(phase))
            image = Image.open(path).convert('RGB')
            colours.append(image.getpixel((image.width//2,image.height//2)))
        terminal_stats = stats()
        draws = sum(item['windowGlassDraws'] for item in terminal_stats['monitors'])
        samples = sum(item['windowCacheMisses'] for item in terminal_stats['monitors'])
        assert draws>0 and samples>=2,(terminal_stats,colours)
        assert max(abs(colours[0][i]-colours[1][i]) for i in range(3))>15,colours
        (screenshots/'terminal.png').write_bytes(path.read_bytes())
        report['surfaces']['ghostty'] = {'pixel_colours':colours,'glass_draws':draws,'new_samples':samples}
        print('ghostty live sampling verified',flush=True)
        stop(terminal)
        assert not hypr('configerrors').strip(), hypr('configerrors')
        report['passed'] = True
    finally:
        for process in reversed(processes):
            if process.poll() is None:
                process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill(); process.wait()
        run('hyprctl', 'dispatch', 'workspace', original)
        log.close()
    report['runtime_warnings'] = [line.strip() for line in (folder/'runtime.log').read_text().splitlines()
                                  if 'Gtk-WARNING' in line or 'CRITICAL' in line]
    assert not report['runtime_warnings'], report['runtime_warnings']
    (ROOT / 'docs/glass-live-verification.json').write_text(json.dumps(report, indent=2)+'\n')
    print('Evidence:', folder)

if __name__ == '__main__':
    main()
