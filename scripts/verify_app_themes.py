#!/usr/bin/env python3
"""Check real applications in a private Wayland session with synthetic data."""
from pathlib import Path
import json
import os
import shutil
import socket
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]


def run(*args, env=None):
    return subprocess.check_output(list(map(str, args)), env=env, text=True)


def main():
    report = {}
    original = json.loads(run("hyprctl", "-j", "activeworkspace"))["name"]
    occupied = {w["id"] for w in json.loads(run("hyprctl", "-j", "workspaces")) if w["windows"]}
    workspace = next(item for item in range(51, 80) if item not in occupied)
    with tempfile.TemporaryDirectory(prefix="anto-editor-wayland-") as temporary:
        folder = Path(temporary)
        saved = folder / "environment.json"
        save = folder / "save.py"
        save.write_text("import os,json,pathlib\npathlib.Path(" + repr(str(saved)) + ").write_text(json.dumps({k:os.environ.get(k,'') for k in ['WAYLAND_DISPLAY','HYPRLAND_INSTANCE_SIGNATURE','DBUS_SESSION_BUS_ADDRESS']}))\n")
        config = folder / "hyprland.conf"
        config.write_text("monitor = , 1280x800, 0x0, 1\nanimations {\n enabled = false\n}\ngeneral {\n border_size = 0\n}\n"
                          + f"source = {Path.home()}/.config/anto426-local/hypr/glass-loader.conf\nsource = {ROOT}/.config/hypr/conf/glass.conf\nsource = {ROOT}/.config/hypr/conf/windowrule.conf\n"
                          + "exec-once = python3 " + str(save) + "\n")
        compositor = application = None
        with (ROOT / "build/app-theme-runtime.log").open("w") as log:
            run("hyprctl", "dispatch", "workspace", workspace)
            try:
                compositor = subprocess.Popen(["dbus-run-session", "--", "Hyprland", "--config", str(config)], env=dict(os.environ, HYPRLAND_NO_RT="1"), stdout=log, stderr=log)
                deadline = time.monotonic() + 12
                while not saved.exists() and compositor.poll() is None and time.monotonic() < deadline:
                    time.sleep(.1)
                assert saved.exists(), "Private compositor did not start"
                child = dict(os.environ, **json.loads(saved.read_text()), HOME=str(folder), XDG_CONFIG_HOME=str(folder / ".config"), XDG_DATA_HOME=str(folder / ".local/share"), XDG_STATE_HOME=str(folder / ".local/state"), XDG_CACHE_HOME=str(folder / ".cache"))
                child.pop("DISPLAY", None)
                child.update(VSCODE_EXECUTABLE="/usr/share/code/code", ANTO_THEME_HOST_REPORT=str(folder / "host-report.json"), ANTO_THEME_TEST_USER_DATA=str(folder / "code-data"), ANTO_THEME_TEST_EXTENSIONS=str(folder / "code-extensions"), ANTO_THEME_TEST_GLASS="1")
                subprocess.run(["node", str(ROOT / "build/upstreams/vscodetheme/tests/run-host.cjs")], env=child, stdout=log, stderr=log, check=True, timeout=45)
                report["vscode"] = json.loads((folder / "host-report.json").read_text())
                vault = folder / "vault"
                vault.mkdir()
                (vault / "Welcome.md").write_text("# Anto426 Monet\n\nA synthetic note for the appearance test.\n\n- Shared wallpaper palette\n- Transparent surfaces\n- Solid, readable text\n\n```python\nprint('Hello')\n```\n")
                registry = folder / ".config/obsidian/obsidian.json"
                registry.parent.mkdir(parents=True)
                registry.write_text(json.dumps({"vaults": {"aade991378bfe6fd": {"path": str(vault), "open": True, "ts": int(time.time() * 1000)}}}))
                app_data = folder / "obsidian-data"
                app_data.mkdir()
                shutil.copyfile(registry, app_data / "obsidian.json")
                palette = ROOT / "build/upstreams/vscodetheme/palette/default.json"
                resources = folder / "theme"
                resources.mkdir()
                for name in ("manifest.json", "theme.css"):
                    shutil.copyfile(ROOT / "build/upstreams/obsidian-theme" / name, resources / name)
                shutil.copyfile(ROOT / "build/upstreams/obsidian-theme/palette/template.css", resources / "template.css")
                subprocess.run(["python3", str(ROOT / "support/compat/app_themes.py"), "obsidian", str(resources), str(palette)], env=child, check=True, stdout=log, stderr=log)
                (vault / ".obsidian/workspace.json").write_text(json.dumps({"main": {"id": "main", "type": "split", "children": [{"id": "leaf", "type": "leaf", "state": {"type": "markdown", "state": {"file": "Welcome.md", "mode": "preview", "source": False}}}], "direction": "vertical"}, "active": "leaf", "lastOpenFiles": ["Welcome.md"]}))
                with socket.socket() as server:
                    server.bind(("127.0.0.1", 0))
                    port = server.getsockname()[1]
                application = subprocess.Popen(["obsidian", "--ozone-platform=wayland", "--user-data-dir=" + str(folder / "obsidian-data"), "--remote-debugging-address=127.0.0.1", "--remote-debugging-port=" + str(port)], env=child, stdout=log, stderr=log)
                script = ROOT / "build/app-theme-dom.cjs"
                script.write_text('''const fs=require('node:fs')
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const {spawnSync}=require('node:child_process');
(async()=>{
 let target
 for(let i=0;i<150;i++){
  try { const pages=await (await fetch('http://127.0.0.1:'+process.argv[2]+'/json/list')).json();target=pages.find(p=>p.type==='page'&&p.url.includes('index.html'));if(target)break }catch{}
  await sleep(100)
 }
 if(!target)throw new Error('Obsidian app page was not ready')
 const ws=new WebSocket(target.webSocketDebuggerUrl);await new Promise(r=>ws.addEventListener('open',r,{once:true}))
 let sequence=0;const pending=new Map();ws.addEventListener('message',e=>{const p=JSON.parse(e.data);if(p.id){const callback=pending.get(p.id);pending.delete(p.id);callback(p.result)}})
 const call=(method,params)=>new Promise(resolve=>{const id=++sequence;pending.set(id,resolve);ws.send(JSON.stringify({id,method,params}))})
 let data
 for(let i=0;i<120;i++){
  const value=await call('Runtime.evaluate',{expression:`JSON.stringify({dark:document.body.classList.contains('theme-dark'),accent:getComputedStyle(document.body).getPropertyValue('--interactive-accent').trim(),background:getComputedStyle(document.body).getPropertyValue('--background-primary').trim(),note:!!document.querySelector('.markdown-preview-view')})`,returnByValue:true})
  try{data=JSON.parse(value.result.value)}catch{ if(i===0)console.error(JSON.stringify(value)) }
  if(data&&data.accent==='#ed94b8'&&data.note)break
  await sleep(100)
 }
 if(!data||data.accent!=='#ed94b8'||!data.dark||!data.note)throw new Error('Theme was not active: '+JSON.stringify(data))
 const changedPalette=JSON.parse(fs.readFileSync(process.argv[5],'utf8'));changedPalette.accent='#66aacc'
 const changedFile=process.argv[3]+'.palette';fs.writeFileSync(changedFile,JSON.stringify(changedPalette))
 const install=spawnSync('python3',[process.argv[6],'obsidian',process.argv[7],changedFile],{encoding:'utf8'})
 if(install.status!==0)throw new Error(install.stderr)
 let liveAccent
 for(let i=0;i<60;i++){
  const value=await call('Runtime.evaluate',{expression:"getComputedStyle(document.body).getPropertyValue('--interactive-accent').trim()",returnByValue:true})
  liveAccent=value.result.value;if(liveAccent==='#66aacc')break;await sleep(100)
 }
 if(liveAccent!=='#66aacc')throw new Error('Palette did not update live: '+liveAccent)
 data.liveUpdate=true;data.updatedAccent=liveAccent
 const shot=await call('Page.captureScreenshot',{format:'png'});fs.writeFileSync(process.argv[4],Buffer.from(shot.data,'base64'))
 fs.writeFileSync(process.argv[3],JSON.stringify({...data,passed:true},null,2));ws.close()
})().catch(e=>{console.error(e);process.exit(1)})
''')
                screenshots = ROOT / "docs/screenshots"
                screenshots.mkdir(exist_ok=True)
                subprocess.run(["node", str(script), str(port), str(folder / "obsidian-report.json"), str(screenshots / "obsidian-theme.png"), str(palette), str(ROOT / "support/compat/app_themes.py"), str(resources)], env=child, stdout=log, stderr=log, check=True, timeout=35)
                report["obsidian"] = json.loads((folder / "obsidian-report.json").read_text())
                report["obsidian"]["version"] = run("pacman", "-Q", "obsidian").strip().split()[1]
                (ROOT / "docs/app-themes-verification.json").write_text(json.dumps(report, indent=2) + "\n")
                print(json.dumps(report))
            finally:
                if application and application.poll() is None:
                    application.terminate()
                    application.wait(timeout=4)
                if compositor and compositor.poll() is None:
                    compositor.terminate()
                    compositor.wait(timeout=4)
                if json.loads(run("hyprctl", "-j", "activeworkspace"))["id"] == workspace:
                    run("hyprctl", "dispatch", "workspace", original)


if __name__ == "__main__":
    main()
