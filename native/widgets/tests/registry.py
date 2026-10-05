from pathlib import Path
import subprocess
import sys
import tempfile

script=Path(sys.argv[1])
with tempfile.TemporaryDirectory() as folder:
    root=Path(folder);source=root/'views';source.mkdir();output=root/'registry.c'
    (source/'new_widget.c').write_text('WIDGET_DECLARE(weather, "Meteo", "weather", "", "system", "", 320, 200, 0, 0, 80, 0, create);')
    subprocess.run([sys.executable,str(script),str(source),str(output)],check=True)
    assert '&widget_definition_weather' in output.read_text()
    (source/'another.c').write_text('WIDGET_DECLARE(second, "", "", "", "", "", 1,1,0,0,0,0,create);')
    subprocess.run([sys.executable,str(script),str(source),str(output)],check=True)
    assert '&widget_definition_second' in output.read_text()
    (source/'duplicate.c').write_text((source/'another.c').read_text())
    result=subprocess.run([sys.executable,str(script),str(source),str(output)],capture_output=True)
    assert result.returncode!=0
print('registry: a new widget source is discovered automatically; duplicate definitions rejected')
