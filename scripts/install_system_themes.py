#!/usr/bin/env python3
"""Install versioned theme files without touching authentication or boot entries."""
from pathlib import Path
import argparse
import os
import pwd
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]

def install(grub, sddm, owner, activate=False):
    generated=ROOT/'build/design/Tokens.qml'
    if not generated.is_file():
        raise SystemExit('Build the dotfiles first: python3 scripts/install.py')
    account=pwd.getpwnam(owner)
    grub.mkdir(parents=True,exist_ok=True)
    sddm.mkdir(parents=True,exist_ok=True)
    (sddm/'Backgrounds').mkdir(exist_ok=True)
    for source in (ROOT/'native/login').rglob('*'):
        relative=source.relative_to(ROOT/'native/login')
        if 'tests' in relative.parts or not source.is_file():continue
        target=sddm/relative
        # Keep preferences, including fingerprint/empty-password PAM initiation.
        if source.name=='theme.conf' and target.exists():continue
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(source,target);target.chmod(0o644)
    shutil.copyfile(generated,sddm/'Tokens.qml')
    if os.geteuid()==0:
        for directory in (grub,sddm):
            for item in [directory,*directory.rglob('*')]:
                if not item.is_symlink():os.chown(item,account.pw_uid,account.pw_gid)
    if activate:
        if os.geteuid()!=0:raise SystemExit('--activate requires root')
        directory=Path('/etc/sddm.conf.d');directory.mkdir(exist_ok=True)
        (directory/'10-anto426-theme.conf').write_text('[Theme]\nCurrent=anto426-sddm\nCursorTheme=macOS\n')
    print('Qt6 login theme installed; authentication and boot entries preserved.')

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--user',default=os.environ.get('SUDO_USER') or pwd.getpwuid(os.getuid()).pw_name)
    parser.add_argument('--grub-dir',type=Path,default=Path('/usr/share/grub/themes/anto426'))
    parser.add_argument('--sddm-dir',type=Path,default=Path('/usr/share/sddm/themes/anto426-sddm'))
    parser.add_argument('--activate',action='store_true')
    args=parser.parse_args();install(args.grub_dir,args.sddm_dir,args.user,args.activate)
