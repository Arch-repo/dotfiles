#!/usr/bin/env python3
"""Exercise the real backend against a private BlueZ-shaped D-Bus service."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
import time
import warnings

import gi
gi.require_version("Gio", "2.0")
from gi.repository import Gio, GLib
warnings.filterwarnings("ignore", category=DeprecationWarning, module=__name__)


XML = """<node>
<interface name="org.freedesktop.DBus.ObjectManager">
 <method name="GetManagedObjects"><arg type="a{oa{sa{sv}}}" direction="out"/></method>
</interface>
<interface name="org.freedesktop.DBus.Properties">
 <method name="Set"><arg type="s"/><arg type="s"/><arg type="v"/></method>
</interface>
<interface name="org.bluez.Adapter1">
 <method name="StartDiscovery"/><method name="StopDiscovery"/>
 <method name="RemoveDevice"><arg type="o"/></method>
</interface>
<interface name="org.bluez.Device1">
 <method name="Connect"/><method name="Disconnect"/><method name="Pair"/><method name="CancelPairing"/>
</interface>
<interface name="org.bluez.AgentManager1">
 <method name="RegisterAgent"><arg type="o"/><arg type="s"/></method>
 <method name="RequestDefaultAgent"><arg type="o"/></method>
 <method name="UnregisterAgent"><arg type="o"/></method>
</interface>
<interface name="com.anto426.BluetoothTest">
 <method name="Request"><arg type="s"/><arg type="v"/><arg type="v" direction="out"/></method>
</interface>
</node>"""


def variant_props(**values):
    result = {}
    for key, value in values.items():
        signature = "b" if isinstance(value, bool) else "o" if key == "Adapter" else "s"
        result[key] = GLib.Variant(signature, value)
    return result


class BlueZ:
    def __init__(self, address):
        flags = Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT | Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION
        self.bus = Gio.DBusConnection.new_for_address_sync(address, flags, None, None)
        self.bus.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                           "RequestName", GLib.Variant("(su)", ("org.bluez", 0)), None, 0, 5000, None)
        self.objects = {}
        self.calls = []
        self.failure = None
        self.agent = None
        self.registrations = []
        info = Gio.DBusNodeInfo.new_for_xml(XML)
        self.info = {item.name: item for item in info.interfaces}
        self.register("/", "org.freedesktop.DBus.ObjectManager")
        self.register("/org/bluez", "org.bluez.AgentManager1")
        self.register("/test", "com.anto426.BluetoothTest")
        for index, address in enumerate(("AA:BB:CC:DD:EE:01", "AA:BB:CC:DD:EE:02")):
            path = f"/org/bluez/hci{index}"
            self.objects[path] = {"org.bluez.Adapter1": variant_props(
                Address=address, Alias=f"Adapter {index}", Name=f"Radio {index}",
                Powered=True, Pairable=False, Discoverable=False, Discovering=False)}
            self.register(path, "org.bluez.Adapter1")
            self.register(path, "org.freedesktop.DBus.Properties")
            device = path + "/dev_11_22_33_44_55_66"
            self.objects[device] = {"org.bluez.Device1": variant_props(
                Address="11:22:33:44:55:66", Adapter=path, Name="Not available headphones",
                Alias="Cuffie\tTest\n", Icon="audio-headset", Paired=True,
                Trusted=True, Blocked=False, Connected=False),
                "org.bluez.Battery1": {"Percentage": GLib.Variant("y", 85)}}
            self.objects[device]["org.bluez.Device1"]["RSSI"] = GLib.Variant("n", -42)
            self.register(device, "org.bluez.Device1")
            self.register(device, "org.freedesktop.DBus.Properties")

    def register(self, path, interface):
        self.registrations.append(self.bus.register_object(path, self.info[interface], self.method, None, None))

    def method(self, bus, sender, path, interface, method, args, invocation):
        self.calls.append((path, method, args.unpack()))
        if self.failure == method:
            invocation.return_dbus_error("org.bluez.Error.NotReady", "Radio spenta nella fixture")
            return
        if method == "GetManagedObjects":
            invocation.return_value(GLib.Variant("(a{oa{sa{sv}}})", (self.objects,)))
            return
        if method == "Set":
            target, key, _ = args.unpack()
            self.objects[path][target][key] = args.get_child_value(2).get_variant()
        elif method in ("Connect", "Disconnect", "Pair"):
            key = "Paired" if method == "Pair" else "Connected"
            self.objects[path]["org.bluez.Device1"][key] = GLib.Variant("b", method != "Disconnect")
        elif method == "StartDiscovery":
            self.objects[path]["org.bluez.Adapter1"]["Discovering"] = GLib.Variant("b", True)
        elif method == "StopDiscovery":
            self.objects[path]["org.bluez.Adapter1"]["Discovering"] = GLib.Variant("b", False)
        elif method == "RemoveDevice":
            self.objects.pop(args.unpack()[0], None)
        elif method == "RegisterAgent":
            agent_path, capability = args.unpack()
            assert capability == "KeyboardDisplay"
            self.agent = (sender, agent_path)
        elif method == "UnregisterAgent":
            self.agent = None
        elif method == "Request":
            requested, _ = args.unpack()
            parameters = args.get_child_value(1).get_variant()
            if not self.agent:
                invocation.return_dbus_error("org.bluez.Error.Failed", "Agente mancante")
                return

            def finished(connection, result):
                try:
                    reply = connection.call_finish(result)
                    value = GLib.Variant("b", True) if reply.get_type_string() == "()" else reply
                    invocation.return_value(GLib.Variant("(v)", (value,)))
                except GLib.Error as error:
                    invocation.return_dbus_error("org.bluez.Error.Canceled", error.message)

            self.bus.call(*self.agent, "org.bluez.Agent1", requested, parameters, None,
                          Gio.DBusCallFlags.NONE, 5000, None, finished)
            return
        invocation.return_value(None)


def main():
    backend = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory(prefix="anto-bluetooth-") as directory:
        temporary = Path(directory)
        daemon = subprocess.Popen(["dbus-daemon", "--session", "--nofork", "--print-address=1"],
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True)
        address = daemon.stdout.readline().strip()
        assert address
        loop = GLib.MainLoop()
        server = BlueZ(address)
        thread = threading.Thread(target=loop.run, daemon=True)
        thread.start()
        environment = os.environ | {"DBUS_SYSTEM_BUS_ADDRESS": address,
            "XDG_RUNTIME_DIR": directory, "ANTO_LOCAL_CONFIG_ROOT": str(temporary / "local"),
            "ANTO_MENU_NO_NOTIFY": "1"}
        try:
            def run(*args, ok=True):
                start = time.monotonic()
                result = subprocess.run([str(backend), "bluetooth", *args], env=environment,
                                        text=True, capture_output=True, timeout=8)
                assert (result.returncode == 0) == ok, (args, result.stdout, result.stderr)
                assert time.monotonic() - start < 3, ("Operazione lenta", args)
                return result.stdout if ok else result.stderr

            selected = temporary / "local/bluetooth/controller"
            selected.parent.mkdir(parents=True)
            selected.write_text("AA:BB:CC:DD:EE:02\n")
            second = "/org/bluez/hci1"
            device = second + "/dev_11_22_33_44_55_66"
            snapshot = run("snapshot")
            assert "STATUS\tyes\tAA:BB:CC:DD:EE:02\tAdapter 1\tyes\tno\tno\tno\t1\t0\t1" in snapshot
            assert snapshot.count("DEVICE\t") == 1
            assert "Cuffie Test" in snapshot and "85\t-42" in snapshot
            run("power", "off")
            assert not server.objects[second]["org.bluez.Adapter1"]["Powered"].unpack()
            assert server.objects["/org/bluez/hci0"]["org.bluez.Adapter1"]["Powered"].unpack()
            run("power", "toggle")
            run("pairable", "on")
            run("pairable", "off")
            run("discoverable", "on", "60")
            assert server.objects[second]["org.bluez.Adapter1"]["DiscoverableTimeout"].unpack() == 60
            run("discoverable", "off")
            run("discoverable", "on", "bad", ok=False)
            run("power", "invalid", ok=False)
            run("connect", "11:22:33:44:55:66")
            assert "\t1\t1\t1\n" in run("snapshot")
            run("disconnect", "11:22:33:44:55:66")
            run("pair", "11:22:33:44:55:66")
            for action in ("untrust", "trust", "block", "unblock", "cancel-pairing"):
                run(action, "11:22:33:44:55:66")
            run("device-alias", "11:22:33:44:55:66", "Nome 'quotato' $(); test")
            assert server.objects[device]["org.bluez.Device1"]["Alias"].unpack() == "Nome 'quotato' $(); test"
            run("device-alias-reset", "11:22:33:44:55:66")
            run("controller-alias", "Computer di prova")
            run("controller-alias-reset")
            run("connect", "00:00:00:00:00:00", ok=False)
            server.failure = "Connect"
            assert "Radio spenta" in run("connect", "11:22:33:44:55:66", ok=False)
            server.failure = None
            run("scan", "start", "5")
            assert server.objects[second]["org.bluez.Adapter1"]["Discovering"].unpack()
            assert "running" in run("scan", "start", "5")
            run("scan", "stop")
            assert not server.objects[second]["org.bluez.Adapter1"]["Discovering"].unpack()
            server.failure = "StartDiscovery"
            run("scan", "start", "5", ok=False)
            server.failure = None
            run("remove", "11:22:33:44:55:66")
            assert device not in server.objects
            server.failure = "GetManagedObjects"
            assert "Radio spenta" in run("snapshot", ok=False)
            server.failure = None
            if len(sys.argv) > 2:
                ui_environment = environment | {"GDK_BACKEND": "broadway", "BROADWAY_DISPLAY": ":1",
                    "GTK_A11Y": "none", "GTK_THEME": "Adwaita:dark", "DBUS_SESSION_BUS_ADDRESS": address}
                broadway = subprocess.Popen(["gtk4-broadwayd", "-a", "127.0.0.1", "-p", "0", ":1"],
                    env=ui_environment, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True)
                try:
                    for _ in range(100):
                        if (temporary / "broadway1.socket").exists():
                            break
                        assert broadway.poll() is None, broadway.stderr.read()
                        time.sleep(0.01)
                    result = subprocess.run([sys.argv[2], sys.argv[3]], env=ui_environment, timeout=30)
                    assert result.returncode == 0, "Fixture GTK Bluetooth fallita"
                finally:
                    broadway.terminate()
                    broadway.wait(timeout=3)
            server.objects.clear()
            assert run("snapshot").startswith("STATUS\tno\t")
            print("bluetooth BlueZ fixture: ok (snapshot, controller, dispositivi, errori e ricerca)")
        finally:
            subprocess.run([str(backend), "bluetooth", "scan", "stop"], env=environment,
                           capture_output=True, timeout=8)
            loop.quit()
            thread.join(timeout=2)
            daemon.terminate()
            daemon.wait(timeout=3)


if __name__ == "__main__":
    main()
