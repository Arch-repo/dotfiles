#!/usr/bin/env python3
"""Validate GTK3/GTK4/libadwaita and Qt5/Qt6/Kvantum with private synthetic windows."""
from pathlib import Path
import json
import os
import shlex
import subprocess
import tempfile
import time
from PIL import Image, ImageChops, ImageStat

ROOT = Path(__file__).resolve().parents[1]


def run(*args, env=None):
    return subprocess.check_output(list(map(str, args)), env=env, text=True, timeout=10)


def main():
    binaries = ROOT / 'build/toolkit-fixtures'
    binaries.mkdir(exist_ok=True)
    for name, packages, compiler, source in (
        ('gtk3', ['gtk+-3.0'], 'cc', 'gtk.c'),
        ('gtk4', ['gtk4', 'libadwaita-1'], 'cc', 'gtk.c'),
        ('gtk4plain', ['gtk4'], 'cc', 'gtk.c'),
        ('qt5', ['Qt5Widgets', 'Qt5Test'], 'c++', 'qt.cpp'),
        ('qt6', ['Qt6Widgets', 'Qt6Test'], 'c++', 'qt.cpp'),
        ('quick5', ['Qt5Quick', 'Qt5QuickControls2', 'Qt5Test'], 'c++', 'quick.cpp'),
        ('quick', ['Qt6Quick', 'Qt6QuickControls2', 'Qt6Test'], 'c++', 'quick.cpp'),
    ):
        flags = shlex.split(run('pkg-config', '--cflags', '--libs', *packages))
        if name == 'gtk4plain':
            flags.append('-DANTO_PLAIN_GTK4')
        subprocess.run([compiler, '-w', '-fPIC', str(ROOT / 'support/tests/toolkit' / source), '-o', str(binaries / ('anto-toolkit-' + name)), *flags], check=True)
    report = {}
    processes = []
    with tempfile.TemporaryDirectory(prefix='anto-toolkit-wayland-') as folder:
        temp = Path(folder)
        saved = temp / 'environment.json'
        save = temp / 'save.py'
        save.write_text('import os,json,pathlib\npathlib.Path(' + repr(str(saved)) + ').write_text(json.dumps({k:os.environ.get(k,"") for k in ["WAYLAND_DISPLAY","HYPRLAND_INSTANCE_SIGNATURE","DBUS_SESSION_BUS_ADDRESS"]}))\n')
        config = temp / 'hyprland.conf'
        config.write_text('monitor = ,1280x800,0x0,1\nanimations {\n enabled = false\n}\ngeneral {\n border_size = 0\n}\n'
                          + f'source = {Path.home()}/.config/anto426-local/hypr/glass-loader.conf\nsource = {ROOT}/.config/hypr/conf/glass.conf\nsource = {ROOT}/.config/hypr/conf/windowrule.conf\n'
                          + 'windowrule = float on, match:class ^anto-toolkit-.*$\nwindowrule = center on, match:class ^anto-toolkit-.*$\nwindowrule = size 900 580, match:class ^anto-toolkit-.*$\n'
                          + f'exec-once = python3 {save}\n')
        with (ROOT / 'build/toolkit-theme-runtime.log').open('w') as log:
            try:
                compositor = subprocess.Popen(['dbus-run-session', '--', 'Hyprland', '--config', str(config)], env=dict(os.environ, HYPRLAND_NO_RT='1', HYPRLAND_NO_SD_VARS='1'), stdout=log, stderr=log)
                processes.append(compositor)
                deadline = time.monotonic() + 12
                while not saved.exists() and compositor.poll() is None and time.monotonic() < deadline:
                    time.sleep(.1)
                assert saved.exists(), 'Private compositor did not start'
                child = dict(os.environ, **json.loads(saved.read_text()), HOME=str(temp), XDG_CONFIG_HOME=str(temp / '.config'), XDG_DATA_HOME=str(temp / '.local/share'), XDG_CACHE_HOME=str(temp / '.cache'), XDG_STATE_HOME=str(temp / '.local/state'), GDK_BACKEND='wayland', GTK_A11Y='none', GSETTINGS_BACKEND='keyfile', QT_QPA_PLATFORM='wayland', QT_QPA_PLATFORMTHEME='qt5ct', QT_STYLE_OVERRIDE='kvantum', ANTO426_UNDERLAY_CONTROL=str(temp / 'phase'))
                child.pop('DISPLAY', None)
                child.pop('QT_STYLE_OVERRIDE', None)
                child.pop('GTK_THEME', None)
                child['QT_QUICK_CONTROLS_CONF'] = str(temp / '.config/anto426-local/theme/qtquickcontrols2.conf')
                child['QML_IMPORT_PATH'] = str(temp / '.local/share/anto-desktop/qml')
                child['QML2_IMPORT_PATH'] = child['QML_IMPORT_PATH']
                child['QT_QUICK_CONTROLS_STYLE_PATH'] = child['QML_IMPORT_PATH']
                def hypr(*args):
                    return run('hyprctl', *args, env=child)
                # An invisible outer workspace stops Wayland frame callbacks.
                # A private headless output keeps actual GPU composition running
                # while the user works, without touching the user's workspaces.
                connector = 'ANTO-THEME-TEST'
                hypr('output', 'create', 'headless', connector)
                for monitor in json.loads(hypr('-j', 'monitors')):
                    if monitor['name'] != connector:
                        hypr('output', 'remove', monitor['name'])
                hypr('dispatch', 'focusmonitor', connector)
                child['ANTO426_TARGET_MONITOR'] = connector
                palette = ROOT / 'build/upstreams/vscodetheme/palette/default.json'
                for kind in ('gtk', 'qt'):
                    artifact = temp / (kind + '-theme')
                    subprocess.run(['python3', str(ROOT / f'build/upstreams/{kind}-theme/palette/render.py'), '--palette', str(palette), '--material', str(ROOT / 'build/design/application-material.json'), '--output', str(artifact)], env=child, stdout=log, stderr=log, check=True)
                    subprocess.run(['python3', str(ROOT / 'support/compat/app_themes.py'), kind, str(artifact), str(palette)], env=child, stdout=log, stderr=log, check=True)
                run('gsettings', 'set', 'org.gnome.desktop.interface', 'gtk-theme', 'anto426', env=child)
                run('gsettings', 'set', 'org.gnome.desktop.interface', 'color-scheme', 'prefer-dark', env=child)
                time.sleep(.5)
                assert not hypr('configerrors').strip(), hypr('configerrors')
                hypr('dismissnotify')
                (temp / 'phase').write_text('0')
                underlay = subprocess.Popen([str(ROOT / 'build/live-underlay-fixture')], env=child, stdout=log, stderr=log)
                processes.append(underlay)
                time.sleep(.3)
                sample = temp / 'sample-files'
                sample.mkdir()
                for label in ('Documents', 'Pictures', 'Music', 'Projects'):
                    (sample / label).mkdir()
                for name in ('gtk3', 'gtk4', 'gtk4plain', 'qt5', 'qt6', 'quick5', 'quick', 'nemo', 'libreoffice'):
                    output = temp / (name + '.json')
                    app_env = dict(child, ANTO_TOOLKIT_REPORT=str(output))
                    command = [str(binaries / ('anto-toolkit-' + name))]
                    if name == 'nemo':
                        command = ['nemo', '--no-desktop', str(sample)]
                    if name == 'libreoffice':
                        profile = temp / '.config/libreoffice/4'
                        command = ['libreoffice', '--nologo', '--norestore', '-env:UserInstallation=' + profile.as_uri()]
                    if name.startswith('gtk'):
                        command.append(str(temp / '.local/share/themes/anto426' / ('gtk-3.0' if name == 'gtk3' else 'gtk-4.0') / 'gtk.css'))
                    application = subprocess.Popen(command, env=app_env, stdout=log, stderr=log)
                    processes.append(application)
                    deadline = time.monotonic() + 8
                    while name not in ('nemo', 'libreoffice') and not output.exists() and application.poll() is None and time.monotonic() < deadline:
                        time.sleep(.1)
                    if name in ('nemo', 'libreoffice'):
                        time.sleep(1)
                        values = {'toolkit': 'GTK3/Nemo' if name == 'nemo' else 'GTK3/LibreOffice VCL'}
                        if name == 'libreoffice':
                            values.update(nativeCanvasAlpha=False, compositorTranslucency=True)
                    else:
                        assert output.exists(), f'{name} exited {application.poll()} without report: inspect build/toolkit-theme-runtime.log'
                        values = json.loads(output.read_text())
                    assert values.get('cssErrors', 0) == 0, values
                    assert values.get('foregroundAlpha', 1) == 1, values
                    if name not in ('nemo', 'libreoffice'):
                        assert values.get('accent', values.get('highlight')) == '#ed94b8', values
                    if name.startswith('gtk'):
                        expected = json.loads(palette.read_text())
                        assert values['actionForeground'] == expected['selected_fg'], values
                        assert values['error'] == expected['red'], values
                        if name != 'gtk4':
                            assert values['themeName'] == 'anto426', values
                    if name.startswith('qt'):
                        assert values['placeholder'] == '#b7acbe' and values['entryPlaceholder'] == '#b7acbe', values
                        assert values['translucentBackground'], values
                        assert values['sharedGeometryInstalled'] and values['buttonKeyboard'] and values['fieldKeyboard'] and values['checkboxPointer'] and values['spinKeyboard'] and values['comboPopup'], values
                        assert values['paintedFocusBorder'] == '#ed94b8', values
                    if name.startswith('quick'):
                        assert .45 < values['windowAlpha'] < .47 and values['placeholder'] == '#b7acbe', values
                        assert values['style'] == 'Anto426', values
                        assert values['buttonRadius'] == 12 and values['fieldRadius'] == 12, values
                        assert values['buttonKeyboard'] and values['fieldKeyboard'] and values['switchPointer'] and values['comboPopup'] and values['comboKeyboard'], values
                        assert values['qmlWarnings'] == 0 and values['fieldFocusBorder'] == '#ed94b8', values
                    clients = json.loads(hypr('-j', 'clients'))
                    def own_window(c):
                        return c['class'] == 'libreoffice-startcenter' if name == 'libreoffice' else c['pid'] == application.pid
                    window = next(c for c in clients if own_window(c))
                    # Fixtures deliberately have unknown application classes:
                    # glass must work without adding them to a class allowlist.
                    address = 'address:' + window['address']
                    hypr('dispatch', 'setfloating', address)
                    hypr('dispatch', 'resizewindowpixel', 'exact 900 580,' + address)
                    current_monitor = next(m for m in json.loads(hypr('-j', 'monitors')) if m['name'] == connector)
                    left = current_monitor['x'] + int((current_monitor['width'] / current_monitor['scale'] - 900) / 2)
                    top = current_monitor['y'] + int((current_monitor['height'] / current_monitor['scale'] - 580) / 2)
                    hypr('dispatch', 'movewindowpixel', f'exact {left} {top},' + address)
                    time.sleep(.2)
                    window = next(c for c in json.loads(hypr('-j', 'clients')) if own_window(c))
                    x, y = window['at']; width, height = window['size']
                    x -= current_monitor['x']; y -= current_monitor['y']
                    probe = (int(x + width * .7), int(y + height * .75), int(x + width * .8), int(y + height * .85))
                    images = []
                    hypr('hyprglass', 'stats', 'reset')
                    for phase in ('0', '1'):
                        (temp / 'phase').write_text(phase)
                        time.sleep(.35)
                        screenshot = temp / (name + phase + '.png')
                        subprocess.run(['grim', '-o', connector, str(screenshot)], env=child, check=True, stdout=log, stderr=log, timeout=10)
                        images.append(Image.open(screenshot).convert('RGB'))
                    difference = ImageStat.Stat(ImageChops.difference(images[0].crop(probe), images[1].crop(probe))).mean
                    values['backgroundResponse'] = round(sum(difference) / 3, 2)
                    counters = json.loads(hypr('-j', 'hyprglass', 'stats'))
                    values['glassDraws'] = sum(m['windowGlassDraws'] for m in counters['monitors'])
                    values['liveSamples'] = sum(m['windowCacheMisses'] for m in counters['monitors'])
                    assert values['glassDraws'] > 0 and values['liveSamples'] >= 2, values
                    if name in ('gtk4', 'gtk4plain', 'qt6', 'quick5', 'quick', 'nemo', 'libreoffice'):
                        # Publish only the synthetic client; the nested host's
                        # transparent monitor margins can expose the outer desktop.
                        images[0].crop((int(x), int(y), int(x + width), int(y + height))).save(ROOT / 'docs/screenshots' / (name + '-theme.png'))
                    assert 0 <= probe[0] < probe[2] <= images[0].width and 0 <= probe[1] < probe[3] <= images[0].height, (probe, images[0].size)
                    assert values['backgroundResponse'] > 6, values
                    values['passed'] = True
                    report[name] = values
                    if name == 'libreoffice':
                        hypr('dispatch', 'closewindow', address)
                    else:
                        application.terminate()
                    application.wait(timeout=4)
                report['versions'] = {package: run('pkg-config', '--modversion', package).strip() for package in ('gtk+-3.0', 'gtk4', 'libadwaita-1', 'Qt5Widgets', 'Qt6Widgets')}
                (ROOT / 'docs/toolkit-themes-verification.json').write_text(json.dumps(report, indent=2) + '\n')
                print(json.dumps(report))
            finally:
                for process in reversed(processes):
                    if process.poll() is None:
                        process.terminate(); process.wait(timeout=4)


if __name__ == '__main__':
    main()
