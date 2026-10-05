#!/usr/bin/env python3
"""Apply LibreOffice's application canvas roles through its live configuration API.

Never edit registrymodifications.xcu behind a running process. A temporary local
UNO pipe updates the current profile; when Office is closed an instance without
initial windows performs the same update and exits. Documents and preferences are
outside this helper's ownership.
"""
from pathlib import Path
import argparse
from contextlib import contextmanager
import json
import os
import shutil
import subprocess
import time
import uuid


def validate(payload):
    required = {'startCenter': {'StartCenterThumbnailsBackgroundColor', 'StartCenterThumbnailsTextColor'},
                'appearance': {'AppBackground'}}
    if not isinstance(payload, dict) or payload.keys() != required.keys():
        raise ValueError('Unexpected LibreOffice palette groups')
    for group, keys in required.items():
        if not isinstance(payload[group], dict) or payload[group].keys() != keys:
            raise ValueError('Unexpected LibreOffice palette properties')
        if any(type(value) is not int or not 0 <= value <= 0xffffff for value in payload[group].values()):
            raise ValueError('Invalid LibreOffice RGB color')


def configuration(provider, uno, path, update=False):
    node = uno.createUnoStruct('com.sun.star.beans.PropertyValue')
    node.Name, node.Value = 'nodepath', path
    service = 'com.sun.star.configuration.Configuration' + ('UpdateAccess' if update else 'Access')
    return provider.createInstanceWithArguments(service, (node,))


def apply_configuration(provider, uno, payload):
    """Change only owned properties, retaining the selected scheme and its colors."""
    validate(payload)
    count = 0
    center = configuration(provider, uno, '/org.openoffice.Office.Common/Help/StartCenter', True)
    for key, value in payload['startCenter'].items():
        if center.getPropertyValue(key) != value:
            center.setPropertyValue(key, value)
            count += 1
    center.commitChanges()
    # Using the selected scheme avoids discarding user-specific document colors
    # or forcing a separate theme. The empty historical name means defaults.
    scheme = configuration(provider, uno, '/org.openoffice.Office.UI/ColorScheme', True)
    name = scheme.getPropertyValue('CurrentColorScheme')
    schemes = scheme.getByName('ColorSchemes')
    if name and schemes.hasByName(name):
        current = schemes.getByName(name)
        for key, value in payload['appearance'].items():
            color = current.getByName(key)
            if color.getPropertyValue('Color') != value:
                color.setPropertyValue('Color', value)
                count += 1
        scheme.commitChanges()
    return count


def owned_instance(accept):
    """A reused office never receives our accept string in its original argv."""
    for entry in Path('/proc').iterdir():
        if not entry.name.isdecimal():
            continue
        try:
            if entry.joinpath('exe').resolve().name == 'soffice.bin':
                arguments = entry.joinpath('cmdline').read_bytes().split(b'\0')
                if ('--accept=' + accept).encode() in arguments:
                    return True
        except OSError:
            continue
    return False


@contextmanager
def office_configuration(profile=None):
    binary = shutil.which('libreoffice') or shutil.which('soffice')
    if not binary:
        raise FileNotFoundError('LibreOffice is not installed')
    try:
        import uno
    except ImportError as error:
        raise RuntimeError('LibreOffice Python UNO bindings are required for its palette') from error
    accept = 'pipe,name=anto_theme_' + uuid.uuid4().hex + ';urp;StarOffice.ComponentContext'
    # Keep the graphical backend: if the user opens a document during this
    # brief job, that document must appear normally rather than be headless.
    command = [binary, '--nodefault', '--nologo', '--norestore']
    if profile:
        command.append('-env:UserInstallation=' + Path(profile).resolve().as_uri())
    environment = dict(os.environ)
    environment.pop('GTK_THEME', None)
    process = subprocess.Popen([*command, '--accept=' + accept], env=environment,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    context = None
    try:
        local = uno.getComponentContext()
        resolver = local.ServiceManager.createInstanceWithContext('com.sun.star.bridge.UnoUrlResolver', local)
        deadline = time.monotonic() + 6
        while True:
            try:
                context = resolver.resolve('uno:' + accept)
                break
            except uno.getClass('com.sun.star.connection.NoConnectException'):
                if time.monotonic() >= deadline:
                    raise RuntimeError('LibreOffice theme connection timed out')
                time.sleep(.05)
        manager = context.ServiceManager
        provider = manager.createInstanceWithContext('com.sun.star.configuration.ConfigurationProvider', context)
        yield provider, uno
    finally:
        closed = False
        if context is not None and owned_instance(accept):
            desktop = context.ServiceManager.createInstanceWithContext('com.sun.star.frame.Desktop', context)
            # Never close a pre-existing session, even if its forwarding
            # launcher has not exited yet, or documents opened during startup.
            if not desktop.getComponents().hasElements():
                closed = desktop.terminate()
            if closed:
                process.wait(timeout=4)
        if not closed:
            subprocess.run([*command, '--unaccept=' + accept], env=environment,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=4, check=True)


def install(artifact, profile=None):
    payload = json.loads(Path(artifact).read_text())
    validate(payload)
    if not (shutil.which('libreoffice') or shutil.which('soffice')):
        return {'status': 'not-installed'}
    with office_configuration(profile) as (provider, uno):
        changes = apply_configuration(provider, uno, payload)
        # Flush through the application, preserving preferences changed in
        # memory as well as on disk before the palette update.
        provider.flush()
    return {'status': 'applied', 'changedProperties': changes}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('artifact', type=Path)
    parser.add_argument('--profile', type=Path, help='Isolated profile for verification')
    arguments = parser.parse_args()
    print(json.dumps(install(arguments.artifact, arguments.profile)))
