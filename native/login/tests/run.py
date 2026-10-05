from pathlib import Path
import os
import shutil
import subprocess
import sys
import tempfile

with tempfile.TemporaryDirectory(prefix='anto-login-test-') as temporary:
    root=Path(temporary)
    theme=root/'theme'
    shutil.copytree(sys.argv[2], theme, ignore=shutil.ignore_patterns('tests'))
    shutil.copyfile(sys.argv[3],theme/'Tokens.qml')
    shots=Path(sys.argv[4]) if len(sys.argv)>4 else root/'screenshots'
    shots.mkdir(parents=True,exist_ok=True)
    subprocess.run(['magick','-size','1280x800','gradient:#233e65-#ad607d',str(shots/'background.png')],check=True)
    env=dict(os.environ,QT_QPA_PLATFORM='offscreen',QT_QUICK_BACKEND='software',QT_QUICK_CONTROLS_STYLE='Basic',QT_LOGGING_RULES='*.debug=false;*.warning=true;*.critical=true', QT_LOGGING_TO_CONSOLE='1')
    result=subprocess.run([sys.argv[1],str(theme),str(shots)],env=env,capture_output=True,text=True,timeout=15)
    print(result.stdout,end='');print(result.stderr,end='',file=sys.stderr)
    sys.exit(result.returncode)
