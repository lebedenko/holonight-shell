"""Private read-only systemd session protocol. Never parse or evaluate shell output."""
import json
import os
from pathlib import Path
import re
import socket
import stat
import struct
import subprocess

KEYS = frozenset('PATH HOME WAYLAND_DISPLAY DISPLAY XDG_SESSION_ID XDG_RUNTIME_DIR XDG_CURRENT_DESKTOP XDG_SESSION_DESKTOP XDG_SESSION_TYPE XDG_MENU_PREFIX XDG_DATA_DIRS XDG_CONFIG_DIRS XDG_CONFIG_HOME XDG_DATA_HOME DBUS_SESSION_BUS_ADDRESS HYPRLAND_INSTANCE_SIGNATURE HYPRLAND_CMD SWAYSOCK I3SOCK LABWC_PID QT_QPA_PLATFORM QT_QPA_PLATFORMTHEME QT_QUICK_CONTROLS_STYLE XCURSOR_THEME HOLONIGHT_APPEARANCE_FILE HOLONIGHT_LABWC_CONFIG_DIR HOLONIGHT_SESSION_READY'.split())
SERVICE = 'org.freedesktop.systemd1'
ROOT = '/org/freedesktop/systemd1'


def bus(user, *args):
    try:
        result = subprocess.run(['busctl', '--user' if user else '--system', '--json=short',
                                 '--timeout=3', '--auto-start=no', *args],
                                capture_output=True, text=True, timeout=4)
    except subprocess.TimeoutExpired as error:
        raise ValueError('systemd query timed out') from error
    if result.returncode:
        raise ValueError(result.stderr.strip() or 'systemd query failed')
    return json.loads(result.stdout)['data']


def property_value(user, path, interface, name, service=SERVICE):
    return bus(user, 'get-property', service, path, interface, name)


def manager_environment():
    entries = property_value(True, ROOT, SERVICE + '.Manager', 'Environment')
    if not isinstance(entries, list) or not all(isinstance(e, str) and '=' in e and '\0' not in e for e in entries):
        raise ValueError('invalid manager environment')
    result = {}
    for entry in entries:
        key, value = entry.split('=', 1)
        if key in KEYS:
            if key in result:
                raise ValueError('duplicate environment key: ' + key)
            result[key] = value
    return result


def active_units():
    rows = bus(True, 'call', SERVICE, ROOT, SERVICE + '.Manager', 'ListUnits')[0]
    return sorted((row[0], row[6]) for row in rows
                  if row[0].startswith(('wayland-wm@', 'wayland-session-bindpid@'))
                  and row[3] not in ('inactive', 'failed'))


def process_identity(pid, uid, group):
    process = Path('/proc') / str(pid)
    if pid <= 1 or process.stat().st_uid != uid or not group or group == '/':
        raise ValueError('invalid process owner or group')
    groups = [line.split(':', 2)[2] for line in (process / 'cgroup').read_text().splitlines()]
    if not any(g == group or g.startswith(group + '/') for g in groups):
        raise ValueError('process does not belong to expected session/unit')
    # starttime detects PID reuse across the verification window.
    fields = (process / 'stat').read_text().rsplit(')', 1)[1].split()
    if fields[0] == 'Z':
        raise ValueError('stale process')
    return fields[19]


def login(session, uid):
    if not re.fullmatch(r'[A-Za-z0-9_.-]+', session):
        raise ValueError('invalid logind session')
    service = 'org.freedesktop.login1'
    path = bus(False, 'call', service, '/org/freedesktop/login1', service + '.Manager',
               'GetSession', 's', session)[0]
    get = lambda key: property_value(False, path, service + '.Session', key, service)
    identity = (get('User'), get('Active'), get('State'), get('Type'), get('Scope'))
    if identity[0][0] != uid or identity[1:4] != (True, 'active', 'wayland'):
        raise ValueError('logind owner/session is not active Wayland')
    scope_path = bus(False, 'call', SERVICE, ROOT, SERVICE + '.Manager', 'GetUnit', 's', identity[4])[0]
    group = property_value(False, scope_path, SERVICE + '.Scope', 'ControlGroup')
    return identity, group


def snapshot(session, runtime):
    uid = os.getuid()
    units = active_units()
    compositors = [(name, path) for name, path in units if name.startswith('wayland-wm@')]
    if not compositors:
        if any(name.startswith('wayland-session-bindpid@') for name, _ in units):
            raise ValueError('UWSM session is transitioning')
        return None
    if len(compositors) != 1:
        raise ValueError('ambiguous UWSM compositor units')
    name, path = compositors[0]
    if property_value(True, path, SERVICE + '.Unit', 'ActiveState') != 'active':
        raise ValueError('UWSM compositor is transitioning')
    binds = [(n, p) for n, p in units if n.startswith('wayland-session-bindpid@')]
    if len(binds) != 1:
        raise ValueError('expected one active UWSM login binding')
    match = re.fullmatch(r'wayland-session-bindpid@(\d+)\.service', binds[0][0])
    if not match or property_value(True, binds[0][1], SERVICE + '.Unit', 'ActiveState') != 'active':
        raise ValueError('invalid UWSM login binding')
    bind_pid = int(match[1])
    login_service = 'org.freedesktop.login1'
    login_path = bus(False, 'call', login_service, '/org/freedesktop/login1',
                     login_service + '.Manager', 'GetSessionByPID', 'u', str(bind_pid))[0]
    bound_session = property_value(False, login_path, login_service + '.Session', 'Id', login_service)
    if session and session != bound_session:
        raise ValueError('UWSM login binding belongs to another logind session')
    session = bound_session
    identity, scope = login(session, uid)
    bind_identity = process_identity(bind_pid, uid, scope)
    environment = manager_environment()
    if environment.get('XDG_SESSION_ID', session) != session or environment.get('XDG_RUNTIME_DIR') != str(runtime):
        raise ValueError('manager environment conflicts with logind session')
    environment['XDG_SESSION_ID'] = session
    group = property_value(True, path, SERVICE + '.Service', 'ControlGroup')
    pid = property_value(True, path, SERVICE + '.Service', 'MainPID')
    main_identity = process_identity(pid, uid, group)
    return (units, identity, scope, bind_pid, bind_identity, group, pid, main_identity, environment)


def managed_environment(session, runtime):
    before = snapshot(session, runtime)
    if before is None:
        return None
    uid = os.getuid()
    runtime = Path(runtime)
    if not runtime.is_absolute() or runtime.stat().st_uid != uid or not runtime.is_dir():
        raise ValueError('invalid runtime directory')
    environment = before[-1]
    display = environment.get('WAYLAND_DISPLAY', '')
    if not re.fullmatch(r'wayland-[A-Za-z0-9_.-]+', display):
        raise ValueError('invalid manager Wayland display')
    path = runtime / display
    metadata = path.lstat()
    if not stat.S_ISSOCK(metadata.st_mode) or metadata.st_uid != uid:
        raise ValueError('invalid Wayland socket owner')
    with socket.socket(socket.AF_UNIX) as connection:
        connection.settimeout(0.5)
        connection.connect(str(path))
        pid, peer_uid, _ = struct.unpack('3i', connection.getsockopt(socket.SOL_SOCKET, socket.SO_PEERCRED, 12))
        if peer_uid != uid:
            raise ValueError('Wayland peer owner mismatch')
        peer_identity = process_identity(pid, uid, before[5])
        if snapshot(session, runtime) != before or process_identity(pid, uid, before[5]) != peer_identity:
            raise ValueError('session changed during verification')
        if path.lstat() != metadata:
            raise ValueError('Wayland socket changed during verification')
    return environment
