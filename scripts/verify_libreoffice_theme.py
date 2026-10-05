#!/usr/bin/env python3
"""Verify Office palette updates on a real isolated profile, open and closed."""
from pathlib import Path
import importlib.util
import json
import os
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('office_theme', ROOT / 'support/compat/libreoffice_theme.py')
office = importlib.util.module_from_spec(spec)
spec.loader.exec_module(office)


def main():
    with tempfile.TemporaryDirectory(prefix='anto-office-theme-') as folder:
        resources = Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share')) / 'anto-desktop'
        output = Path(folder) / 'theme'
        subprocess.run(['python3', str(resources / 'gtk-theme/render.py'),
                        '--palette', str(resources / 'vscode-theme/default.json'),
                        '--material', str(resources / 'application-material.json'),
                        '--output', str(output)], check=True)
        artifact = output / 'libreoffice.json'
        expected = json.loads(artifact.read_text())
        profile = Path(folder) / 'profile'
        first = office.install(artifact, profile)
        assert first['changedProperties'] == 3, first
        # Keep the real process open while an independent installer forwards
        # its accept request. It must retain this process and unrelated colors.
        with office.office_configuration(profile) as (provider, uno):
            scheme = office.configuration(provider, uno, '/org.openoffice.Office.UI/ColorScheme', True)
            selected = 'Personal verification scheme'
            scheme.ColorSchemes.insertByName(selected, scheme.ColorSchemes.createInstance())
            scheme.CurrentColorScheme = selected
            current = scheme.ColorSchemes.getByName(selected)
            current.AppBackground.Color = expected['appearance']['AppBackground']
            current.DocColor.Color = 0xfaf0e6
            current.FontColor.Color = 0x221133
            scheme.commitChanges()
            provider.flush()
            repeated = office.install(artifact, profile)
            assert repeated['changedProperties'] == 0, repeated
            assert scheme.CurrentColorScheme == selected
            assert current.DocColor.Color == 0xfaf0e6
            assert current.FontColor.Color == 0x221133
            alternate = json.loads(artifact.read_text())
            alternate['startCenter']['StartCenterThumbnailsBackgroundColor'] = 0x183344
            alternate['startCenter']['StartCenterThumbnailsTextColor'] = 0xf0eedd
            alternate['appearance']['AppBackground'] = 0x183344
            changed = Path(folder) / 'alternate.json'
            changed.write_text(json.dumps(alternate))
            assert office.install(changed, profile)['changedProperties'] == 3
            center = office.configuration(provider, uno, '/org.openoffice.Office.Common/Help/StartCenter')
            assert center.StartCenterThumbnailsBackgroundColor == 0x183344
            assert center.StartCenterThumbnailsTextColor == 0xf0eedd
            assert current.AppBackground.Color == 0x183344
            assert current.DocColor.Color == 0xfaf0e6 and current.FontColor.Color == 0x221133
        # Both live and closed-instance paths persist and cleanly close their
        # owned helper. No direct registry-file write occurs in the installer.
        with office.office_configuration(profile) as (provider, uno):
            center = office.configuration(provider, uno, '/org.openoffice.Office.Common/Help/StartCenter')
            assert center.StartCenterThumbnailsBackgroundColor == 0x183344
            assert office.install(artifact, profile)['changedProperties'] == 3
        registry = ET.parse(profile / 'user/registrymodifications.xcu').getroot()
        values = [value.text for value in registry.iter('value')]
        assert str(0xfaf0e6) in values and str(0x221133) in values
        for value in expected['startCenter'].values():
            assert str(value) in values
    report = {'libreoffice': subprocess.check_output(['libreoffice', '--version'], text=True).strip(),
              'status': 'passed', 'closedProfileInstall': True, 'livePaletteChange': True,
              'idempotent': True, 'selectedSchemePreserved': True, 'documentColorsPreserved': True,
              'restartPersistence': True, 'nativeCanvasAlpha': False}
    (ROOT / 'docs/libreoffice-theme-verification.json').write_text(json.dumps(report, indent=2) + '\n')
    print('LibreOffice: palette persistence, live changes, selected scheme and document preferences passed')


if __name__ == '__main__':
    main()
