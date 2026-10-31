#!/usr/bin/env python3
## @file pymonitor.py
#  @brief Web dashboard to monitor and control a running aplus system, using only the Python standard library.

import argparse
import collections
import errno
import functools
import hmac
import html
import itertools
import json
import logging
import os
import re
import secrets
import signal
import socket
import stat
import subprocess
import sys
import threading
import time
import urllib.parse
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

try:
    import grp
except ImportError:
    grp = None

try:
    import pwd
except ImportError:
    pwd = None


USER_HZ = 100
HISTORY_LEN = 120
MAX_BODY = 64 * 1024
MAX_OUTPUT = 256 * 1024
MAX_LOG_TAIL = 1024 * 1024
MAX_PREVIEW = 1024 * 1024
MAX_ENTRIES = 2000
HEXDUMP_BYTES = 4096
INITD_DIR = "/etc/init.d"
INITD_RUN_DIR = "/var/run/init.d"
LOG_DIR = "/var/log"
DEFAULT_PATH = "/usr/sbin:/usr/bin:/sbin:/bin"

## @brief Signals init (pid 1) turns into reboot, power off and halt, as apps/core/shutdown sends them.
POWER_SIGNALS = {
    "reboot": signal.SIGTERM,
    "poweroff": signal.SIGUSR2,
    "halt": signal.SIGUSR1,
}

## @brief Signals the dashboard lets a user send to a process.
PROCESS_SIGNALS = ("TERM", "INT", "HUP", "KILL", "STOP", "CONT", "USR1", "USR2")

SERVICE_FIELD = re.compile(r"^\s*(EXEC_NAME|EXEC_DESC)=[\"']?([^\"'\n]*)", re.M)

## @brief Octal escape /proc/mounts writes for a space, tab, newline or backslash in a field.
MOUNT_ESCAPE = re.compile(r"\\([0-7]{3})")

CSP = "default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; img-src 'self' data:; frame-ancestors 'none'"

log = logging.getLogger("pymonitor")

## @brief HTTP status for each errno an explorer request can fail with.
ERRNO_STATUS = {
    errno.ENOENT: HTTPStatus.NOT_FOUND,
    errno.EACCES: HTTPStatus.FORBIDDEN,
    errno.EPERM: HTTPStatus.FORBIDDEN,
    errno.ENOTDIR: HTTPStatus.BAD_REQUEST,
    errno.EISDIR: HTTPStatus.BAD_REQUEST,
    errno.ELOOP: HTTPStatus.BAD_REQUEST,
}

## @brief File types the explorer tells apart, by st_mode test.
FILE_KINDS = (
    (stat.S_ISDIR, "dir"),
    (stat.S_ISREG, "file"),
    (stat.S_ISLNK, "link"),
    (stat.S_ISCHR, "char"),
    (stat.S_ISBLK, "block"),
    (stat.S_ISFIFO, "fifo"),
    (stat.S_ISSOCK, "socket"),
)


## @brief Error raised by a route, carrying the HTTP status to answer with.
class ApiError(Exception):

    ## @brief Creates the error.
    #  @param status HTTP status code.
    #  @param message Text sent back to the client.
    def __init__(self, status, message):
        super().__init__(message)
        self.status = status
        self.message = message


## @brief Reads a whole text file.
#  @param path File to read.
#  @return Its contents, or None when it cannot be read.
def read_text(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return None


## @brief Parses "Key: value" lines, as in /proc/meminfo and /proc/<pid>/status.
#  @param text File contents, or None.
#  @return A dict from key to stripped value, in file order.
def parse_fields(text):
    fields = {}
    for line in (text or "").splitlines():
        key, sep, value = line.partition(":")
        if sep:
            fields[key.strip()] = value.strip()
    return fields


## @brief Converts a "123 kB" field to an integer.
#  @param value Field value, or None.
#  @return The number of kB, or 0 when the field is missing.
def kb(value):
    try:
        return int((value or "0").split()[0])
    except (ValueError, IndexError):
        return 0


## @brief Clamps a number into a range.
#  @param value Number to clamp.
#  @param low Smallest allowed value.
#  @param high Largest allowed value.
#  @return The clamped number.
def clamp(value, low, high):
    return max(low, min(high, value))


## @brief Looks up a user name.
#  @param uid Numeric user id.
#  @return The name from /etc/passwd, or the id as text.
@functools.lru_cache(maxsize=256)
def user_name(uid):
    if pwd is not None:
        try:
            return pwd.getpwuid(uid).pw_name
        except KeyError:
            pass
    return str(uid)


## @brief Looks up a group name.
#  @param gid Numeric group id.
#  @return The name from /etc/group, or the id as text.
@functools.lru_cache(maxsize=256)
def group_name(gid):
    if grp is not None:
        try:
            return grp.getgrgid(gid).gr_name
        except KeyError:
            pass
    return str(gid)


## @brief Reads the busy and idle ticks of the machine and of every CPU from /proc/stat.
#  @return A list of (busy, idle) pairs, the total first and then one per CPU.
def read_cpu_ticks():
    ticks = []
    for line in (read_text("/proc/stat") or "").splitlines():
        parts = line.split()
        if not parts or not parts[0].startswith("cpu"):
            continue
        values = [int(v) for v in parts[1:] if v.isdigit()]
        idle = sum(values[3:5])
        ticks.append((sum(values) - idle, idle))
    return ticks


## @brief Computes the CPU use between two (busy, idle) samples.
#  @param current The newer sample.
#  @param previous The older sample.
#  @return The busy share in percent.
def cpu_percent(current, previous):
    busy = current[0] - previous[0]
    total = busy + current[1] - previous[1]
    return round(clamp(100.0 * busy / total, 0.0, 100.0), 1) if total > 0 else 0.0


## @brief Reads memory use from /proc/meminfo.
#  @return A dict with total, free, used and slab (kernel heap), all in kB.
def read_meminfo():
    fields = parse_fields(read_text("/proc/meminfo"))
    total = kb(fields.get("MemTotal"))
    free = kb(fields.get("MemAvailable", fields.get("MemFree")))
    return {"total": total, "free": free, "used": max(total - free, 0), "slab": kb(fields.get("Slab"))}


## @brief Lists the processes /proc knows about.
#  @return Their pids.
def list_pids():
    try:
        return [int(name) for name in os.listdir("/proc") if name.isdigit()]
    except OSError:
        return []


## @brief Reads a process's arguments.
#  @param pid Process id.
#  @return The argument list, empty for kernel tasks or when it cannot be read.
def read_argv(pid):
    try:
        with open(f"/proc/{pid}/cmdline", "rb") as f:
            raw = f.read()
    except OSError:
        return []
    return [arg.decode("utf-8", "replace") for arg in raw.split(b"\0") if arg]


## @brief Reads one row of the process table.
#  @param pid Process id.
#  @return A dict describing the process, or None when it is gone or unreadable.
def read_process(pid):
    base = f"/proc/{pid}"
    stat = read_text(f"{base}/stat")
    status = parse_fields(read_text(f"{base}/status"))
    if not stat or not status:
        return None
    head, _, tail = stat.rpartition(")")
    fields = tail.split()
    comm = head.partition("(")[2]
    io = parse_fields(read_text(f"{base}/io"))
    argv = read_argv(pid)
    try:
        fds = len(os.listdir(f"{base}/fd"))
    except OSError:
        fds = None
    try:
        uid = int(status.get("Uid", "0").split()[0])
        return {
            "pid": pid,
            "ppid": int(fields[1]),
            "name": status.get("Name", comm),
            "exe": os.path.basename(argv[0]) if argv else comm,
            "script": os.path.basename(argv[1]) if len(argv) > 1 and "/" in argv[1] else None,
            "state": fields[0],
            "priority": int(fields[15]),
            "threads": int(status.get("Threads", fields[17])),
            "ticks": int(fields[11]) + int(fields[12]),
            "rss": kb(status.get("VmRSS")),
            "vsz": kb(status.get("VmSize")),
            "uid": uid,
            "user": user_name(uid),
            "fds": fds,
            "rchar": int(io.get("rchar", 0)),
            "wchar": int(io.get("wchar", 0)),
            "cmdline": " ".join(argv) if argv else f"[{comm}]",
        }
    except (ValueError, IndexError):
        return None


## @brief Reads everything the dashboard shows about one process.
#  @param pid Process id.
#  @return A dict with the status fields, arguments, I/O counters and open files.
def read_process_detail(pid):
    base = f"/proc/{pid}"
    status = read_text(f"{base}/status")
    if status is None:
        raise ApiError(HTTPStatus.NOT_FOUND, f"process {pid} does not exist")
    files = []
    try:
        names = sorted(os.listdir(f"{base}/fd"), key=lambda n: int(n) if n.isdigit() else -1)
    except OSError:
        names = []
    for name in names:
        try:
            target = os.readlink(f"{base}/fd/{name}")
        except OSError:
            target = "?"
        files.append({"fd": name, "target": target})
    return {
        "pid": pid,
        "status": list(parse_fields(status).items()),
        "io": list(parse_fields(read_text(f"{base}/io")).items()),
        "argv": read_argv(pid),
        "files": files,
    }


## @brief Process table that remembers CPU time between scans so it can report CPU use.
class ProcessTable:

    ## @brief Creates an empty table.
    def __init__(self):
        self.lock = threading.Lock()
        self.previous = {}

    ## @brief Reads every process and its CPU use since the last scan of at least half a second ago.
    #  @return The process rows.
    def scan(self):
        now = time.monotonic()
        rows = [row for row in map(read_process, list_pids()) if row is not None]
        current = {}
        with self.lock:
            for row in rows:
                ticks, then, cpu = self.previous.get(row["pid"], (row["ticks"], now, 0.0))
                elapsed = now - then
                if elapsed >= 0.5:
                    cpu = round(clamp(100.0 * (row["ticks"] - ticks) / USER_HZ / elapsed, 0.0, 100.0 * (os.cpu_count() or 1)), 1)
                    ticks, then = row["ticks"], now
                row["cpu"] = cpu
                current[row["pid"]] = (ticks, then, cpu)
            self.previous = current
        return rows


## @brief Background thread that samples CPU and memory use into a rolling history.
class Sampler(threading.Thread):

    ## @brief Creates the sampler.
    #  @param interval Seconds between samples.
    def __init__(self, interval):
        super().__init__(name="sampler", daemon=True)
        self.interval = interval
        self.lock = threading.Lock()
        self.history = collections.deque(maxlen=HISTORY_LEN)
        self.cpus = []
        self.previous = read_cpu_ticks()

    ## @brief Takes one sample per interval for as long as the dashboard runs.
    def run(self):
        while True:
            time.sleep(self.interval)
            try:
                self.sample()
            except Exception:
                log.exception("sampling failed")

    ## @brief Records the CPU use since the previous sample and the memory in use now.
    def sample(self):
        current = read_cpu_ticks()
        usage = [cpu_percent(c, p) for c, p in zip(current, self.previous)]
        self.previous = current
        point = {"t": time.time(), "cpu": usage[0] if usage else 0.0, "mem": read_meminfo()["used"]}
        with self.lock:
            self.cpus = usage[1:]
            self.history.append(point)

    ## @brief Copies the latest per-CPU use and the history.
    #  @return A dict with "cpus" and "history".
    def snapshot(self):
        with self.lock:
            return {"cpus": list(self.cpus), "history": list(self.history)}


## @brief Reads the CPU model and count from /proc/cpuinfo.
#  @return A (model, count) pair.
def read_cpuinfo():
    model = None
    count = 0
    for line in (read_text("/proc/cpuinfo") or "").splitlines():
        key, _, value = line.partition(":")
        key = key.strip()
        if key == "processor":
            count += 1
        elif key == "model name" and model is None:
            model = value.strip()
    return model, count or os.cpu_count() or 1


## @brief Lists the filesystem types the kernel supports.
#  @return One dict per type from /proc/filesystems, with its name and whether it mounts without a source.
def read_filesystems():
    types = []
    for line in (read_text("/proc/filesystems") or "").splitlines():
        parts = line.split()
        if parts:
            types.append({"name": parts[-1], "nodev": parts[0] == "nodev" and len(parts) > 1})
    return types


## @brief Decodes the octal escapes in a /proc/mounts field.
#  @param field The field as /proc/mounts prints it.
#  @return The string mount() was given.
def unmangle(field):
    return MOUNT_ESCAPE.sub(lambda m: chr(int(m.group(1), 8)), field)


## @brief Reads the mount table as the dashboard sees it from its root.
#  @return One dict per mount, in mount order, with its source, mountpoint, type and options.
def read_mounts():
    mounts = []
    for line in (read_text("/proc/mounts") or "").splitlines():
        fields = line.split()
        if len(fields) < 4:
            continue
        options = fields[3].split(",")
        mounts.append({"source": unmangle(fields[0]), "target": unmangle(fields[1]), "type": unmangle(fields[2]), "options": options, "readonly": "ro" in options})
    return mounts


## @brief Measures the filesystem a path lives on with statfs.
#  @param path Any path on that filesystem.
#  @return A dict with sizes in bytes, block and inode counts, and the name limit; raises OSError when statfs fails.
def fs_usage(path):
    st = os.statvfs(path)
    size = st.f_blocks * st.f_frsize
    free = st.f_bfree * st.f_frsize
    return {
        "block_size": st.f_bsize,
        "fragment_size": st.f_frsize,
        "blocks": st.f_blocks,
        "size": size,
        "used": size - free,
        "free": free,
        "avail": st.f_bavail * st.f_frsize,
        "inodes": st.f_files,
        "inodes_free": st.f_ffree,
        "namemax": st.f_namemax,
    }


## @brief Lists every mount with the usage of its filesystem.
#  @return The mounts in mount order, each with "usage" or "error", and "covered" when a later mount on the same path hides it.
def list_mounts():
    mounts = read_mounts()
    for i, mount in enumerate(mounts):
        mount["covered"] = any(later["target"] == mount["target"] for later in mounts[i + 1:])
        mount["usage"] = None
        mount["error"] = None
        if mount["covered"]:
            continue
        try:
            mount["usage"] = fs_usage(mount["target"])
        except OSError as e:
            mount["error"] = e.strerror or str(e)
    return mounts


## @brief Measures the root filesystem.
#  @return Its fs_usage() with the source and type of the mount on /, or None when statfs fails.
def root_disk():
    try:
        usage = fs_usage("/")
    except OSError:
        return None
    root = next((m for m in reversed(read_mounts()) if m["target"] == "/"), {})
    return dict(usage, source=root.get("source"), type=root.get("type"))


## @brief Finds the address of the interface holding the default route.
#  @return The IPv4 address, or None when the stack does not report one, as lwIP does not for UDP.
def primary_address():
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
            s.connect(("192.0.2.1", 9))
            address = s.getsockname()[0]
    except OSError:
        return None
    return None if address == "0.0.0.0" else address


## @brief Collects the system summary shown on the overview tab.
#  @param sampler The running sampler.
#  @param local Address the request arrived on, used when the default route's address is unknown.
#  @return The overview payload.
def read_overview(sampler, local):
    uname = os.uname()
    stat = {}
    for line in (read_text("/proc/stat") or "").splitlines():
        key, _, value = line.partition(" ")
        if not key.startswith("cpu"):
            stat[key] = value.strip()
    uptime = (read_text("/proc/uptime") or "0").split()
    loadavg = (read_text("/proc/loadavg") or "").split()
    model, count = read_cpuinfo()
    snapshot = sampler.snapshot()
    return {
        "hostname": socket.gethostname(),
        "uname": {"sysname": uname.sysname, "release": uname.release, "version": uname.version, "machine": uname.machine},
        "version": (read_text("/proc/version") or "").strip(),
        "cmdline": (read_text("/proc/cmdline") or "").strip(),
        "uptime": float(uptime[0]) if uptime else 0.0,
        "boot_time": int(stat["btime"]) if stat.get("btime", "").isdigit() else None,
        "cpu_model": model,
        "cpu_count": count,
        "memory": read_meminfo(),
        "disk": root_disk(),
        "address": primary_address() or local,
        "filesystems": read_filesystems(),
        "processes": len(list_pids()),
        "last_pid": int(loadavg[4]) if len(loadavg) > 4 and loadavg[4].isdigit() else None,
        "cpus": snapshot["cpus"],
        "history": snapshot["history"],
        "interval": sampler.interval,
        "history_len": HISTORY_LEN,
        "dashboard": {"pid": os.getpid(), "user": user_name(os.geteuid()), "python": sys.version.split()[0]},
    }


## @brief Builds the environment commands run with.
#  @return A copy of the dashboard's environment with PATH, HOME and TERM set.
def command_env():
    env = dict(os.environ)
    env.setdefault("PATH", DEFAULT_PATH)
    env.setdefault("HOME", "/home/root")
    env["TERM"] = "dumb"
    return env


## @brief Kills a child and everything in its process group.
#  @param child The Popen whose group to kill.
def kill_group(child):
    try:
        os.killpg(child.pid, signal.SIGKILL)
    except OSError:
        child.kill()


## @brief Runs a command to completion, capturing stdout and stderr together.
#  @param argv Program and arguments.
#  @param timeout Seconds before the command's process group is killed.
#  @param cwd Working directory, or None for the dashboard's own.
#  @return A dict with the exit code, output, elapsed seconds and whether it timed out.
def run_command(argv, timeout, cwd=None):
    started = time.monotonic()
    try:
        child = subprocess.Popen(argv, cwd=cwd, env=command_env(), stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True)
    except OSError as e:
        return {"code": None, "output": f"{e}\n", "elapsed": 0.0, "timed_out": False, "truncated": False}
    timed_out = False
    try:
        out, _ = child.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        timed_out = True
        kill_group(child)
        try:
            out, _ = child.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            out = b""
    return {
        "code": child.returncode,
        "output": out[:MAX_OUTPUT].decode("utf-8", "replace"),
        "elapsed": round(time.monotonic() - started, 3),
        "timed_out": timed_out,
        "truncated": len(out) > MAX_OUTPUT,
    }


## @brief Resolves a service name to its /etc/init.d script.
#  @param name Script file name.
#  @return The script's path.
def service_path(name):
    if not name or "/" in name or name.startswith("."):
        raise ApiError(HTTPStatus.BAD_REQUEST, "invalid service name")
    path = os.path.join(INITD_DIR, name)
    if not os.path.isfile(path):
        raise ApiError(HTTPStatus.NOT_FOUND, f"no service {name}")
    return path


## @brief Returns the log file init.sh and the dashboard send a service's output to.
#  @param name Script file name.
#  @return The log's path.
def service_log(name):
    return os.path.join(LOG_DIR, "init.d", name + ".log")


## @brief Returns the pid file 00-init.sh tracks a service's program with.
#  @param name Script file name.
#  @return The pid file's path.
def service_pid_file(name):
    return os.path.join(INITD_RUN_DIR, name + ".pid")


## @brief Reads the pid a service's pid file records.
#  @param name Script file name.
#  @return The pid, or None when the file is missing or does not hold a valid pid.
def read_service_pid(name):
    text = (read_text(service_pid_file(name)) or "").strip()
    return int(text) if text.isdigit() and int(text) > 0 else None


## @brief Lists the /etc/init.d services and their state from the pid files in /var/run/init.d.
#  @return One dict per service.
def list_services():
    try:
        entries = sorted(os.listdir(INITD_DIR))
    except OSError:
        entries = []
    services = []
    for name in entries:
        path = os.path.join(INITD_DIR, name)
        if not os.path.isfile(path):
            continue
        fields = dict(SERVICE_FIELD.findall(read_text(path) or ""))
        if "EXEC_NAME" not in fields:
            continue
        pid = read_service_pid(name)
        running = pid is not None and os.path.isdir(f"/proc/{pid}")
        log = service_log(name)
        services.append({
            "name": name,
            "program": fields["EXEC_NAME"],
            "description": fields.get("EXEC_DESC") or name,
            "enabled": os.access(path, os.X_OK),
            "running": running,
            "pid": pid if running else None,
            "stale": pid is not None and not running,
            "log": log if os.path.isfile(log) else None,
        })
    return services


## @brief Runs a service's start script with its output appended to the service log.
#  @param name Script file name.
#  @param path Script path.
#  @return The text the script and its program wrote while it ran.
def start_service(name, path):
    log_path = service_log(name)
    os.makedirs(os.path.dirname(log_path), exist_ok=True)
    with open(log_path, "ab") as log:
        offset = log.tell()
        child = subprocess.Popen([path, "start"], env=command_env(), stdin=subprocess.DEVNULL, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
    try:
        code = child.wait(timeout=15)
    except subprocess.TimeoutExpired:
        threading.Thread(target=child.wait, name=f"reap-{child.pid}", daemon=True).start()
        return f"{name}: start script still running after 15 s, output in {log_path}\n"
    with open(log_path, "rb") as log:
        log.seek(offset)
        text = log.read(MAX_OUTPUT).decode("utf-8", "replace")
    return text if code == 0 else f"{text}{name}: start exited with status {code}\n"


## @brief Starts, stops or restarts an /etc/init.d service.
#  @param name Script file name.
#  @param action "start", "stop" or "restart".
#  @return The text the scripts printed.
def service_action(name, action):
    if action not in ("start", "stop", "restart"):
        raise ApiError(HTTPStatus.BAD_REQUEST, f"unknown action {action}")
    path = service_path(name)
    output = ""
    if action in ("stop", "restart"):
        result = run_command([path, "stop"], 15)
        output += result["output"]
        if result["code"] != 0:
            return output
    if action in ("start", "restart"):
        output += start_service(name, path)
    return output


## @brief Lists the files under the log directory.
#  @return One dict per file with its path, size and modification time.
def list_logs():
    logs = []
    for root, dirs, files in os.walk(LOG_DIR):
        dirs.sort()
        for name in sorted(files):
            path = os.path.join(root, name)
            try:
                st = os.stat(path)
            except OSError:
                continue
            logs.append({"path": path, "size": st.st_size, "mtime": st.st_mtime})
    return logs


## @brief Checks that a path names a file inside the log directory.
#  @param path Requested path.
#  @return The resolved path.
def resolve_log(path):
    real = os.path.realpath(path or "")
    if not real.startswith(os.path.realpath(LOG_DIR) + os.sep) or not os.path.isfile(real):
        raise ApiError(HTTPStatus.NOT_FOUND, f"no log {path}")
    return real


## @brief Reads the end of a file.
#  @param path File to read.
#  @param size Maximum number of bytes to return.
#  @return A (text, file size) pair.
def tail_file(path, size):
    with open(path, "rb") as f:
        f.seek(0, os.SEEK_END)
        end = f.tell()
        f.seek(max(end - size, 0))
        data = f.read()
    return data.decode("utf-8", "replace"), end


## @brief Turns a failed file operation into an API error.
#  @param e The OSError.
#  @param path Path the operation was on.
#  @return The ApiError to raise.
def os_error(e, path):
    return ApiError(ERRNO_STATUS.get(e.errno, HTTPStatus.INTERNAL_SERVER_ERROR), f"{path}: {e.strerror or e}")


## @brief Names the type of a file.
#  @param mode The file's st_mode.
#  @return A FILE_KINDS name, or "unknown".
def file_kind(mode):
    for test, name in FILE_KINDS:
        if test(mode):
            return name
    return "unknown"


## @brief Resolves a path's symlinks and stats what it points to.
#  @param path Absolute path.
#  @return A (resolved path, stat result) pair.
def resolve_path(path):
    if not path or not path.startswith("/"):
        raise ApiError(HTTPStatus.BAD_REQUEST, "path must be absolute")
    resolved = os.path.realpath(path)
    try:
        return resolved, os.stat(resolved)
    except OSError as e:
        raise os_error(e, resolved) from None


## @brief Resolves a path that must name a regular file, the only kind the explorer reads.
#  @param path Absolute path.
#  @return A (resolved path, stat result) pair.
def resolve_file(path):
    resolved, st = resolve_path(path)
    kind = file_kind(st.st_mode)
    if kind == "dir":
        raise ApiError(HTTPStatus.BAD_REQUEST, f"{resolved} is a directory")
    if kind != "file":
        raise ApiError(HTTPStatus.BAD_REQUEST, f"{resolved} is a {kind} file, and the explorer reads only regular files")
    return resolved, st


## @brief Describes one directory entry without following it.
#  @param entry An os.DirEntry.
#  @return A dict with the name, kind, size, mode, owner, time and, for a symlink, its target.
def describe_entry(entry):
    try:
        st = entry.stat(follow_symlinks=False)
    except OSError:
        return {"name": entry.name, "kind": "unknown"}
    row = {
        "name": entry.name,
        "kind": file_kind(st.st_mode),
        "size": st.st_size,
        "mode": stat.filemode(st.st_mode),
        "user": user_name(st.st_uid),
        "group": group_name(st.st_gid),
        "mtime": st.st_mtime,
    }
    if row["kind"] == "link":
        try:
            row["target"] = os.readlink(entry.path)
        except OSError:
            row["target"] = "?"
    return row


## @brief Lists a directory for the explorer, directories first.
#  @param path Resolved directory path.
#  @return The listing payload.
def list_directory(path):
    try:
        with os.scandir(path) as entries:
            rows = [describe_entry(entry) for entry in itertools.islice(entries, MAX_ENTRIES + 1)]
    except OSError as e:
        raise os_error(e, path) from None
    truncated = len(rows) > MAX_ENTRIES
    rows = sorted(rows[:MAX_ENTRIES], key=lambda row: (row["kind"] != "dir", row["name"].lower()))
    return {"path": path, "kind": "dir", "parent": os.path.dirname(path) if path != "/" else None, "entries": rows, "truncated": truncated}


## @brief Formats bytes as a hex dump with an ASCII column.
#  @param data Bytes to format.
#  @return The dump, 16 bytes per line.
def hexdump(data):
    lines = []
    for offset in range(0, len(data), 16):
        chunk = data[offset:offset + 16]
        hexes = " ".join(f"{b:02x}" for b in chunk)
        text = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        lines.append(f"{offset:08x}  {hexes:<47}  |{text}|")
    return "\n".join(lines) + "\n" if lines else ""


## @brief Reads the start of a regular file for the explorer preview.
#  @param path Requested path.
#  @param size Maximum number of bytes to read.
#  @return The preview payload, text or a hex dump.
def preview_file(path, size):
    path, st = resolve_file(path)
    try:
        with open(path, "rb") as f:
            data = f.read(size + 1)
    except OSError as e:
        raise os_error(e, path) from None
    truncated = len(data) > size
    data = data[:size]
    binary = b"\0" in data
    return {
        "path": path,
        "size": st.st_size,
        "read": len(data),
        "mode": stat.filemode(st.st_mode),
        "user": user_name(st.st_uid),
        "group": group_name(st.st_gid),
        "mtime": st.st_mtime,
        "binary": binary,
        "text": hexdump(data[:HEXDUMP_BYTES]) if binary else data.decode("utf-8", "replace"),
        "truncated": truncated or (binary and len(data) > HEXDUMP_BYTES),
    }


KMSG_PATH = "/dev/kmsg"
KMSG_RECORD = re.compile(rb"^(\d+),(\d+),(\d+),[^;]*;(.*)$", re.M)
KMSG_READS = 256
ANSI_ESCAPE = re.compile(r"\x1b\[[0-9;?]*[ -/]*[@-~]|\x1b[@-_]|\r")

SYSFS_BUS = "/sys/bus"
ID_DIRS = ("/usr/share/hwdata", "/usr/share/misc", "/usr/share")

PCI_CLASSES = {
    0x00: "Unclassified device", 0x01: "Mass storage controller", 0x02: "Network controller", 0x03: "Display controller",
    0x04: "Multimedia controller", 0x05: "Memory controller", 0x06: "Bridge", 0x07: "Communication controller",
    0x08: "Generic system peripheral", 0x09: "Input device controller", 0x0A: "Docking station", 0x0B: "Processor",
    0x0C: "Serial bus controller", 0x0D: "Wireless controller", 0x0E: "Intelligent controller", 0x0F: "Satellite controller",
    0x10: "Encryption controller", 0x11: "Signal processing controller", 0x12: "Processing accelerator", 0x13: "Non-essential instrumentation",
    0xFF: "Unassigned class",
}

PCI_SUBCLASSES = {
    0x0100: "SCSI storage controller", 0x0101: "IDE interface", 0x0105: "ATA controller", 0x0106: "SATA controller", 0x0107: "SAS controller",
    0x0108: "NVMe controller", 0x0180: "Mass storage controller", 0x0200: "Ethernet controller", 0x0280: "Network controller",
    0x0300: "VGA compatible controller", 0x0302: "3D controller", 0x0380: "Display controller", 0x0400: "Video controller",
    0x0401: "Audio controller", 0x0403: "Audio device", 0x0480: "Multimedia controller", 0x0500: "RAM memory",
    0x0600: "Host bridge", 0x0601: "ISA bridge", 0x0604: "PCI bridge", 0x0680: "Bridge", 0x0700: "Serial controller",
    0x0780: "Communication controller", 0x0800: "PIC", 0x0805: "SD host controller", 0x0880: "System peripheral",
    0x0900: "Keyboard controller", 0x0902: "Mouse controller", 0x0C03: "USB controller", 0x0C05: "SMBus", 0x0D80: "Wireless controller",
}

PCI_USB_INTERFACES = {0x00: "UHCI", 0x10: "OHCI", 0x20: "EHCI", 0x30: "xHCI", 0xFE: "USB device"}

PCI_VENDORS = {
    "1022": "AMD", "1002": "AMD/ATI", "10de": "NVIDIA", "10ec": "Realtek", "1234": "QEMU", "15ad": "VMware",
    "1af4": "Red Hat (virtio)", "1b36": "Red Hat (QEMU)", "8086": "Intel", "80ee": "VirtualBox",
}

PCI_DEVICES = {
    "1234:1111": "Bochs/QEMU VGA",
    "1af4:1000": "virtio network device", "1af4:1001": "virtio block device", "1af4:1002": "virtio memory balloon",
    "1af4:1003": "virtio console", "1af4:1005": "virtio RNG", "1af4:1009": "virtio filesystem",
    "1af4:1041": "virtio 1.0 network device", "1af4:1042": "virtio 1.0 block device", "1af4:1043": "virtio 1.0 console",
    "1af4:1044": "virtio 1.0 RNG", "1af4:1045": "virtio 1.0 memory balloon", "1af4:1048": "virtio 1.0 SCSI",
    "1af4:1050": "virtio 1.0 GPU", "1af4:1052": "virtio 1.0 input",
    "1b36:0001": "QEMU PCI-PCI bridge", "1b36:0008": "QEMU PCIe host bridge", "1b36:000c": "QEMU PCIe root port",
    "1b36:000d": "QEMU xHCI host controller", "1b36:0100": "QXL paravirtual graphic card",
    "8086:100e": "82540EM Gigabit Ethernet", "8086:10d3": "82574L Gigabit Ethernet", "8086:1237": "440FX host bridge",
    "8086:2415": "82801AA AC'97 audio", "8086:2668": "ICH6 HD audio", "8086:2918": "ICH9 LPC interface", "8086:2922": "ICH9 AHCI controller",
    "8086:2930": "ICH9 SMBus controller", "8086:293e": "ICH9 HD audio", "8086:29c0": "82G33/G31/P35/P31 DRAM controller",
    "8086:7000": "82371SB PIIX3 ISA", "8086:7010": "82371SB PIIX3 IDE", "8086:7020": "82371SB PIIX3 USB",
    "8086:7113": "82371AB/EB/MB PIIX4 ACPI",
    "80ee:beef": "VirtualBox graphics adapter", "80ee:cafe": "VirtualBox guest service",
}

USB_CLASSES = {
    0x01: "Audio", 0x02: "Communications", 0x03: "Human interface device", 0x05: "Physical interface", 0x06: "Imaging",
    0x07: "Printer", 0x08: "Mass storage", 0x09: "Hub", 0x0A: "CDC data", 0x0B: "Smart card", 0x0D: "Content security",
    0x0E: "Video", 0x0F: "Personal healthcare", 0x10: "Audio/video", 0x11: "Billboard", 0xDC: "Diagnostic",
    0xE0: "Wireless controller", 0xEF: "Miscellaneous", 0xFE: "Application specific", 0xFF: "Vendor specific",
}

USB_HID_PROTOCOLS = {1: "Keyboard", 2: "Mouse"}

USB_VENDORS = {"0627": "QEMU", "1d6b": "Linux Foundation", "46f4": "QEMU", "80ee": "VirtualBox", "046d": "Logitech"}

USB_PRODUCTS = {
    "0627:0001": "QEMU USB HID device", "1d6b:0001": "1.1 root hub", "1d6b:0002": "2.0 root hub", "1d6b:0003": "3.0 root hub",
    "46f4:0001": "QEMU USB storage", "80ee:0021": "VirtualBox USB tablet",
}


class KernelLog(threading.Thread):
    def __init__(self, path=KMSG_PATH, limit=MAX_LOG_TAIL):
        super().__init__(name="kmsg", daemon=True)
        self.path = path
        self.limit = limit
        self.data = bytearray()
        self.lock = threading.Lock()
        self.mtime = time.time()
        self.error = None
        self.present = os.path.exists(path)
        try:
            self.fd = os.open(path, os.O_RDONLY | os.O_NONBLOCK)
        except OSError as e:
            self.fd = None
            self.error = e.strerror or str(e)

    def run(self):
        while self.fd is not None:
            self.drain()
            time.sleep(1.0)

    def drain(self):
        chunks = []
        for _ in range(KMSG_READS):
            try:
                chunk = os.read(self.fd, 8192)
            except (BlockingIOError, InterruptedError):
                break
            except OSError as e:
                if e.errno == errno.EPIPE:
                    continue
                self.error = e.strerror or str(e)
                os.close(self.fd)
                self.fd = None
                break
            if not chunk:
                break
            chunks.append(self.record(chunk))
        if not chunks:
            return
        with self.lock:
            self.data += b"".join(chunks)
            if len(self.data) > self.limit:
                cut = len(self.data) - self.limit
                newline = self.data.find(b"\n", cut)
                del self.data[:newline + 1 if 0 <= newline < cut + 4096 else cut]
            self.mtime = time.time()

    def record(self, chunk):
        m = KMSG_RECORD.match(chunk)
        if m is None:
            return chunk
        usec = int(m.group(3))
        return b"[%5d.%06d] %s\n" % (usec // 1000000, usec % 1000000, m.group(4))

    def entry(self):
        if not self.present:
            return None
        with self.lock:
            size = len(self.data)
        return {"path": self.path, "size": size, "mtime": self.mtime, "kernel": True, "error": self.error}

    def tail(self, size):
        with self.lock:
            data = bytes(self.data[-size:])
            total = len(self.data)
        text = ANSI_ESCAPE.sub("", data.decode("utf-8", "replace"))
        if not text and self.error:
            text = f"cannot read {self.path}: {self.error}\n"
        return text, total


def read_uevent(path):
    fields = {}
    for line in (read_text(path) or "").splitlines():
        key, sep, value = line.partition("=")
        if sep:
            fields[key] = value
    return fields


def bus_devices(bus):
    root = os.path.join(SYSFS_BUS, bus, "devices")
    try:
        names = sorted(os.listdir(root))
    except OSError:
        return []
    return [(name, os.path.join(root, name), read_uevent(os.path.join(root, name, "uevent"))) for name in names]


@functools.lru_cache(maxsize=None)
def id_database(name):
    vendors = {}
    for directory in ID_DIRS:
        try:
            f = open(os.path.join(directory, name), encoding="utf-8", errors="replace")
        except OSError:
            continue
        with f:
            vendor = None
            for line in f:
                if line.startswith("C "):
                    break
                if not line.strip() or line[0] == "#":
                    continue
                if line[0] != "\t":
                    code, _, text = line.partition(" ")
                    vendor = vendors.setdefault(code.lower(), (text.strip(), {}))
                elif line[1] != "\t" and vendor is not None:
                    code, _, text = line[1:].partition(" ")
                    vendor[1][code.lower()] = text.strip()
        break
    return vendors


def id_names(database, vendors, products, vendor, product):
    entry = id_database(database).get(vendor)
    vendor_name = entry[0] if entry else vendors.get(vendor)
    product_name = (entry[1].get(product) if entry else None) or products.get(f"{vendor}:{product}")
    return vendor_name, product_name


def pci_class_name(code):
    name = PCI_SUBCLASSES.get(code >> 8) or PCI_CLASSES.get(code >> 16) or f"Class {code >> 16:02x}"
    if code >> 8 == 0x0C03 and code & 0xFF in PCI_USB_INTERFACES:
        name += f" ({PCI_USB_INTERFACES[code & 0xFF]})"
    return name


def usb_class_name(cls, subclass, protocol):
    if cls == 0x03 and subclass == 1 and protocol in USB_HID_PROTOCOLS:
        return f"HID {USB_HID_PROTOCOLS[protocol].lower()}"
    return USB_CLASSES.get(cls) or f"Class {cls:02x}"


def split_numbers(value, base):
    try:
        return [int(part, base) for part in value.split("/")]
    except ValueError:
        return []


def list_pci():
    devices = []
    for name, _, ev in bus_devices("pci"):
        try:
            code = int(ev.get("PCI_CLASS", "0"), 16)
        except ValueError:
            code = 0
        vendor, _, device = ev.get("PCI_ID", "").lower().partition(":")
        vendor_name, device_name = id_names("pci.ids", PCI_VENDORS, PCI_DEVICES, vendor, device)
        devices.append({
            "slot": ev.get("PCI_SLOT_NAME", name),
            "class": f"{code:06x}",
            "class_name": pci_class_name(code),
            "vendor": vendor,
            "device": device,
            "subsystem": ev.get("PCI_SUBSYS_ID", "").lower(),
            "vendor_name": vendor_name,
            "device_name": device_name,
            "driver": ev.get("DRIVER"),
            "uevent": ev,
        })
    return devices


def usb_parent(name):
    parent = name.partition(":")[0]
    return "usb" + parent[:-2] if parent.endswith("-0") else parent


def list_usb():
    devices, interfaces = {}, []
    for name, path, ev in bus_devices("usb"):
        if ev.get("DEVTYPE") == "usb_interface" or ":" in name:
            interfaces.append((name, ev))
            continue
        product = split_numbers(ev.get("PRODUCT", ""), 16)
        kind = split_numbers(ev.get("TYPE", ""), 10)
        vendor, product_id, release = (f"{product[0]:04x}", f"{product[1]:04x}", f"{product[2] >> 8:x}.{product[2] & 0xFF:02x}") if len(product) == 3 else ("", "", None)
        vendor_name, product_name = id_names("usb.ids", USB_VENDORS, USB_PRODUCTS, vendor, product_id)
        devices[name] = {
            "name": name,
            "bus": int(ev.get("BUSNUM", "0") or 0),
            "address": int(ev.get("DEVNUM", "0") or 0),
            "vendor": vendor,
            "product": product_id,
            "release": release,
            "class": kind[0] if kind else 0,
            "vendor_name": (read_text(os.path.join(path, "manufacturer")) or "").strip() or vendor_name,
            "product_name": (read_text(os.path.join(path, "product")) or "").strip() or product_name,
            "driver": ev.get("DRIVER"),
            "interfaces": [],
            "uevent": ev,
        }
    for name, ev in interfaces:
        kind = split_numbers(ev.get("INTERFACE", ""), 10)
        cls, subclass, protocol = kind if len(kind) == 3 else (0, 0, 0)
        entry = {"name": name, "number": name.rpartition(".")[2], "class": cls, "subclass": subclass, "protocol": protocol, "class_name": usb_class_name(cls, subclass, protocol), "driver": ev.get("DRIVER"), "uevent": ev}
        parent = devices.get(usb_parent(name))
        if parent is not None:
            parent["interfaces"].append(entry)
    for device in devices.values():
        if device["class"]:
            kind = split_numbers(device["uevent"].get("TYPE", ""), 10)
            device["class_name"] = usb_class_name(*(kind + [0, 0])[:3])
        else:
            names = list(dict.fromkeys(i["class_name"] for i in device["interfaces"]))
            device["class_name"] = ", ".join(names) or "Defined per interface"
    return sorted(devices.values(), key=lambda d: (d["bus"], d["address"], d["name"]))


def list_device_nodes():
    try:
        entries = list(os.scandir("/dev"))
    except OSError:
        return []
    nodes = []
    for entry in entries:
        try:
            st = entry.stat(follow_symlinks=False)
        except OSError:
            continue
        if stat.S_ISCHR(st.st_mode) or stat.S_ISBLK(st.st_mode):
            nodes.append({
                "path": entry.path,
                "kind": "block" if stat.S_ISBLK(st.st_mode) else "char",
                "major": os.major(st.st_rdev),
                "minor": os.minor(st.st_rdev),
                "mode": stat.filemode(st.st_mode),
                "owner": user_name(st.st_uid),
                "group": group_name(st.st_gid),
            })
    return sorted(nodes, key=lambda n: (n["kind"] != "block", n["path"]))


def read_devices():
    return {"sysfs": os.path.isdir(SYSFS_BUS), "pci": list_pci(), "usb": list_usb(), "nodes": list_device_nodes()}


## @brief Reads an integer request argument.
#  @param args Query or body arguments.
#  @param key Argument name.
#  @return Its value.
def int_arg(args, key):
    try:
        return int(args[key])
    except (KeyError, TypeError, ValueError):
        raise ApiError(HTTPStatus.BAD_REQUEST, f"{key} must be an integer") from None


## @brief GET /api/overview.
#  @param handler The request being answered.
#  @param args Query arguments.
#  @return The system summary.
def route_overview(handler, args):
    return read_overview(handler.server.sampler, handler.connection.getsockname()[0])


## @brief GET /api/processes.
#  @param handler The request being answered.
#  @param args Query arguments.
#  @return The process table.
def route_processes(handler, args):
    return {"processes": handler.server.processes.scan()}


## @brief GET /api/process?pid=N.
#  @param handler The request being answered.
#  @param args Query arguments.
#  @return The process's details.
def route_process(handler, args):
    return read_process_detail(int_arg(args, "pid"))


## @brief GET /api/services.
#  @param handler The request being answered.
#  @param args Query arguments.
#  @return The services and their state.
def route_services(handler, args):
    return {"services": list_services()}


## @brief GET /api/mounts.
#  @param handler The request being answered.
#  @param args Query arguments.
#  @return The mount table with each filesystem's usage, and the filesystem types the kernel supports.
def route_mounts(handler, args):
    return {"mounts": list_mounts(), "filesystems": read_filesystems()}


## @brief GET /api/logs.
#  @param handler The request being answered.
#  @param args Query arguments.
#  @return The log files.
def route_logs(handler, args):
    kernel = handler.server.kmsg.entry()
    return {"logs": ([kernel] if kernel else []) + list_logs()}


## @brief GET /api/log?path=P&bytes=N.
#  @param handler The request being answered.
#  @param args Query arguments.
#  @return The end of the log.
def route_log(handler, args):
    size = clamp(int_arg(args, "bytes") if "bytes" in args else 65536, 1024, MAX_LOG_TAIL)
    if args.get("path") == KMSG_PATH:
        path = KMSG_PATH
        text, total = handler.server.kmsg.tail(size)
    else:
        path = resolve_log(args.get("path"))
        text, total = tail_file(path, size)
    return {"path": path, "size": total, "text": text, "truncated": total > size}


def route_devices(handler, args):
    return read_devices()


## @brief GET /api/files?path=P: lists a directory, or names the kind a path resolves to.
#  @param handler The request being answered.
#  @param args Query arguments.
#  @return The listing, or the resolved path and kind when it is not a directory.
def route_files(handler, args):
    path, st = resolve_path(args.get("path") or "/")
    if not stat.S_ISDIR(st.st_mode):
        return {"path": path, "kind": file_kind(st.st_mode)}
    return list_directory(path)


## @brief GET /api/file?path=P&bytes=N: previews the start of a regular file.
#  @param handler The request being answered.
#  @param args Query arguments.
#  @return The preview.
def route_file(handler, args):
    size = clamp(int_arg(args, "bytes") if "bytes" in args else 65536, 1024, MAX_PREVIEW)
    return preview_file(args.get("path"), size)


## @brief POST /api/signal {pid, signal}.
#  @param handler The request being answered.
#  @param args Body arguments.
#  @return A line describing what was sent.
def route_signal(handler, args):
    pid = int_arg(args, "pid")
    name = str(args.get("signal", "TERM")).upper()
    if name not in PROCESS_SIGNALS:
        raise ApiError(HTTPStatus.BAD_REQUEST, f"signal {name} is not allowed")
    if pid <= 1:
        raise ApiError(HTTPStatus.BAD_REQUEST, "init only takes signals through the power controls")
    try:
        os.kill(pid, getattr(signal, "SIG" + name))
    except ProcessLookupError:
        raise ApiError(HTTPStatus.NOT_FOUND, f"process {pid} does not exist") from None
    except PermissionError:
        raise ApiError(HTTPStatus.FORBIDDEN, f"not allowed to signal process {pid}") from None
    log.info("%s sent SIG%s to %d", handler.client_address[0], name, pid)
    return {"output": f"sent SIG{name} to {pid}\n"}


## @brief POST /api/service {name, action}.
#  @param handler The request being answered.
#  @param args Body arguments.
#  @return The scripts' output.
def route_service(handler, args):
    name, action = str(args.get("name", "")), str(args.get("action", ""))
    output = service_action(name, action)
    log.info("%s ran %s %s", handler.client_address[0], name, action)
    return {"output": output}


## @brief POST /api/exec {command, cwd, timeout}.
#  @param handler The request being answered.
#  @param args Body arguments.
#  @return The command's exit code and output.
def route_exec(handler, args):
    command = str(args.get("command", "")).strip()
    if not command:
        raise ApiError(HTTPStatus.BAD_REQUEST, "empty command")
    cwd = str(args.get("cwd") or "/")
    if not os.path.isdir(cwd):
        raise ApiError(HTTPStatus.BAD_REQUEST, f"{cwd} is not a directory")
    try:
        timeout = clamp(float(args.get("timeout", 30)), 1.0, 600.0)
    except (TypeError, ValueError):
        raise ApiError(HTTPStatus.BAD_REQUEST, "timeout must be a number") from None
    result = run_command(["/bin/sh", "-c", command], timeout, cwd)
    outcome = "timed out" if result["timed_out"] else f"exited with {result['code']}"
    log.info("%s ran %r in %s: %s after %.2f s", handler.client_address[0], command, cwd, outcome, result["elapsed"])
    return result


## @brief POST /api/sync {}.
#  @param handler The request being answered.
#  @param args Body arguments.
#  @return A line confirming the sync.
def route_sync(handler, args):
    os.sync()
    log.info("%s synced filesystems", handler.client_address[0])
    return {"output": "filesystems synced\n"}


## @brief POST /api/power {action}: signals init a second later, so the reply gets out first.
#  @param handler The request being answered.
#  @param args Body arguments.
#  @return A line confirming the request.
def route_power(handler, args):
    action = str(args.get("action", ""))
    if action not in POWER_SIGNALS:
        raise ApiError(HTTPStatus.BAD_REQUEST, f"unknown power action {action}")
    if os.geteuid() != 0:
        raise ApiError(HTTPStatus.FORBIDDEN, "the dashboard must run as root to shut down")
    threading.Timer(1.0, os.kill, (1, POWER_SIGNALS[action])).start()
    log.warning("%s requested %s", handler.client_address[0], action)
    return {"output": f"{action} requested\n"}


## @brief Routes answering GET requests.
GET_ROUTES = {
    "/api/overview": route_overview,
    "/api/processes": route_processes,
    "/api/process": route_process,
    "/api/services": route_services,
    "/api/mounts": route_mounts,
    "/api/devices": route_devices,
    "/api/logs": route_logs,
    "/api/log": route_log,
    "/api/files": route_files,
    "/api/file": route_file,
}

## @brief Routes answering POST requests.
POST_ROUTES = {
    "/api/signal": route_signal,
    "/api/service": route_service,
    "/api/exec": route_exec,
    "/api/sync": route_sync,
    "/api/power": route_power,
}


BASIC_TABS = (("overview", "Overview"), ("processes", "Processes"), ("services", "Services"), ("logs", "Logs"), ("mounts", "Mounts"), ("devices", "Devices"), ("files", "Files"), ("console", "Console"))

BASIC_REFRESH = {"overview": "1", "processes": "0", "logs": "0"}

BASIC_REFRESH_SECONDS = 5

BASIC_SIZES = {16384: "16 KB", 65536: "64 KB", 262144: "256 KB", 1048576: "1 MB"}

BASIC_STATES = {"R": ("running", "good"), "S": ("sleeping", "neutral"), "D": ("waiting", "serious"), "T": ("stopped", "warning"), "Z": ("zombie", "critical"), "X": ("dead", "critical")}

BASIC_PROC_COLUMNS = (("pid", "PID", True), ("user", "User", False), ("name", "Name", False), ("state", "State", False), ("cpu", "CPU %", True), ("rss", "RSS", True), ("threads", "Threads", True), ("fds", "FDs", True), ("cmdline", "Command", False))

POWER_WORDS = {"reboot": "Reboot", "poweroff": "Power off", "halt": "Halt"}

CONFIRM_TABS = {"power": "overview", "signal": "processes", "service": "services"}

MAX_NOTES = 32


def esc(value):
    return html.escape("" if value is None else str(value))


def basic_url(**query):
    return "/basic?" + urllib.parse.urlencode({k: v for k, v in query.items() if v is not None and v != ""})


def fmt_kb(value):
    if value is None:
        return "—"
    if value >= 1048576:
        return f"{value / 1048576:.1f} GiB"
    if value >= 1024:
        return f"{value / 1024:.{0 if value >= 102400 else 1}f} MiB"
    return f"{value} kB"


def fmt_bytes(value):
    return f"{value} B" if value < 1024 else fmt_kb(round(value / 1024))


def fmt_duration(seconds):
    s = int(seconds)
    d, h, m, sec = s // 86400, s % 86400 // 3600, s % 3600 // 60, s % 60
    if d:
        return f"{d}d {h}h"
    if h:
        return f"{h}h {m:02d}m"
    if m:
        return f"{m}m {sec:02d}s"
    return f"{sec}s"


def fmt_time(t):
    return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(t)) if t else "—"


def used_share(usage):
    return usage["used"] / (usage["used"] + usage["avail"]) if usage and usage["used"] + usage["avail"] > 0 else 0.0


def basic_size(args):
    try:
        size = int(args.get("bytes") or 65536)
    except ValueError:
        return 65536
    return size if size in BASIC_SIZES else 65536


def basic_refreshing(args, tab):
    return tab in BASIC_REFRESH and args.get("refresh", BASIC_REFRESH[tab]) == "1"


def basic_class(name):
    return f' class="{name}"' if name else ""


def basic_meter(ratio):
    ratio = clamp(ratio or 0.0, 0.0, 1.0)
    level = " critical" if ratio >= 0.9 else " warning" if ratio >= 0.75 else ""
    return f'<div class="meter{level}"><div style="width:{ratio * 100:.1f}%"></div></div>'


def basic_state(tone, text):
    return f'<span class="dot {tone}"></span>{esc(text)}'


def basic_card(title, sub, body, cls="card"):
    head = f"<h2>{esc(title)}</h2>" if title else ""
    note = f'<p class="sub">{esc(sub)}</p>' if sub else ""
    return f'<div class="{cls}">{head}{note}{body}</div>'


def basic_empty(text, colspan=None):
    if colspan:
        return f'<tr><td colspan="{colspan}" class="empty">{esc(text)}</td></tr>'
    return f'<div class="empty">{esc(text)}</div>'


def basic_facts(rows, mono=False):
    cells = []
    for key, value, *flag in rows:
        cls = ' class="mono"' if (flag[0] if flag else mono) else ""
        cells.append(f"<tr><th>{esc(key)}</th><td{cls}>{esc(value)}</td></tr>")
    return '<table class="facts">' + "".join(cells) + "</table>"


def basic_link(label, href, cls="btn"):
    return f'<a class="{cls}" href="{esc(href)}">{esc(label)}</a>'


def basic_disabled(label):
    return f'<span class="btn disabled">{esc(label)}</span>'


def basic_hidden(fields):
    return "".join(f'<input type="hidden" name="{esc(k)}" value="{esc(v)}">' for k, v in fields.items() if v is not None)


def basic_form(handler, label, fields, cls="btn"):
    hidden = basic_hidden(dict(fields, token=handler.server.token))
    return f'<form method="post" action="/basic" class="inline">{hidden}<input type="submit" class="{cls}" value="{esc(label)}"></form>'


def basic_size_form(fields, size, word):
    options = "".join(f'<option value="{value}"{" selected" if value == size else ""}>{word} {label}</option>' for value, label in BASIC_SIZES.items())
    return f'<form method="get" action="/basic" class="inline">{basic_hidden(fields)}<select name="bytes">{options}</select> <input type="submit" class="btn" value="Show"></form>'


def basic_refresh_link(args, tab):
    on = basic_refreshing(args, tab)
    query = {k: v for k, v in args.items() if k != "note"}
    query.update(tab=tab, refresh="0" if on else "1")
    return basic_link("Stop auto refresh" if on else "Auto refresh", basic_url(**query))


def basic_chart(points, top, span, label):
    width, height = 600, 150
    end = points[-1][0] if points else time.time()
    visible = [(t, v) for t, v in points if end - t <= span]
    x = lambda t: (1 - (end - t) / span) * width
    y = lambda v: (1 - min(v, top) / top) * height if top > 0 else height
    parts = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">', f'<rect x="0" y="0" width="{width}" height="{height}" fill="#fcfcfb"/>']
    for i in range(5):
        gy = height * i / 4
        parts.append(f'<line x1="0" y1="{gy:.1f}" x2="{width}" y2="{gy:.1f}" stroke="{"#c3c2b7" if i == 4 else "#e1e0d9"}" stroke-width="1"/>')
    if len(visible) > 1:
        line = " ".join(f"{x(t):.1f},{y(v):.1f}" for t, v in visible)
        parts.append(f'<polygon points="{x(visible[0][0]):.1f},{height} {line} {x(visible[-1][0]):.1f},{height}" fill="#dce9f9" stroke="none"/>')
        parts.append(f'<polyline points="{line}" fill="none" stroke="#2a78d6" stroke-width="2"/>')
    parts.append("</svg>")
    image = f'<img class="chart" src="data:image/svg+xml,{urllib.parse.quote("".join(parts))}" width="{width}" height="{height}" alt="{esc(label)}">'
    return f'{image}<div class="axis"><span class="right">now</span>{esc(fmt_duration(span))} ago</div>'


def basic_overview(handler, args):
    o = read_overview(handler.server.sampler, handler.connection.getsockname()[0])
    history, memory, disk = o["history"], o["memory"], o["disk"]
    last = history[-1] if history else None
    count = o["cpu_count"]
    tiles = [
        ("CPU", f"{round(last['cpu'])}%" if last else "—", f"{count} CPU{'' if count == 1 else 's'}", None),
        ("Memory in use", fmt_kb(memory["used"]), f"of {fmt_kb(memory['total'])}, {fmt_kb(memory['slab'])} kernel heap", memory["used"] / memory["total"] if memory["total"] else 0.0),
        ("Processes", o["processes"], f"last pid {o['last_pid']}" if o["last_pid"] is not None else "", None),
        ("Uptime", fmt_duration(o["uptime"]), f"since {fmt_time(o['boot_time'])}" if o["boot_time"] else "", None),
        ("Root disk in use", fmt_bytes(disk["used"]) if disk else "—", f"of {fmt_bytes(disk['size'])}, {fmt_bytes(disk['avail'])} available" if disk else "statfs failed on /", used_share(disk)),
    ]
    tile_html = "".join(f'<div class="card tile"><div class="label">{esc(label)}</div><div class="value">{esc(value)}</div><div class="note">{esc(note)}</div>{"" if ratio is None else basic_meter(ratio)}</div>' for label, value, note, ratio in tiles)
    span = o["interval"] * o["history_len"]
    cpu_chart = basic_chart([(p["t"], p["cpu"]) for p in history], 100.0, span, "CPU usage")
    mem_chart = basic_chart([(p["t"], p["mem"] / 1024) for p in history], memory["total"] / 1024, span, "Memory in use")
    cpus = o["cpus"] or [last["cpu"] if last else 0.0]
    cpu_rows = "".join(f'<tr><td>CPU {i}</td><td class="wide">{basic_meter(pct / 100)}</td><td class="num">{round(pct)}%</td></tr>' for i, pct in enumerate(cpus))
    facts = [
        ("Hostname", o["hostname"]),
        ("Kernel", o["version"] or f"{o['uname']['sysname']} {o['uname']['release']}"),
        ("Build", o["uname"]["version"]),
        ("Machine", o["uname"]["machine"]),
        ("CPU", f"{o['cpu_model'] or 'unknown'} × {count}"),
        ("Address", o["address"] or "no network"),
        ("Command line", o["cmdline"] or "—", True),
        ("Root filesystem", f"{disk['type']} on {disk['source']}" if disk and disk.get("type") else "unknown"),
        ("Filesystems", ", ".join(f["name"] for f in o["filesystems"])),
        ("Dashboard", f"pid {o['dashboard']['pid']} as {o['dashboard']['user']}, Python {o['dashboard']['python']}"),
    ]
    power = basic_form(handler, "Sync disks", {"do": "sync", "back": "overview"}) + "".join(basic_link(word, basic_url(tab="confirm", do="power", action=action), "btn danger") for action, word in POWER_WORDS.items())
    return "".join([
        f'<div class="toolbar">{basic_refresh_link(args, "overview")}</div>',
        f'<div class="tiles">{tile_html}</div>',
        basic_card("CPU usage", f"All CPUs, percent busy, from 0 to 100%, last {fmt_duration(span)}", cpu_chart),
        basic_card("Memory in use", f"Physical memory, from 0 to {fmt_kb(memory['total'])}, last {fmt_duration(span)}", mem_chart),
        basic_card("System", "From /proc and uname", basic_facts(facts)),
        basic_card("Per-CPU usage", "Last sample", f'<table class="cpus">{cpu_rows}</table>'),
        basic_card("Power", "Init sends SIGTERM to every process, waits up to 8 s, syncs, then reboots, powers off or halts.", f'<div class="buttons">{power}</div>'),
    ])


def basic_process_detail(handler, pid):
    try:
        d = read_process_detail(pid)
    except ApiError as e:
        return basic_card(f"PID {pid}", None, basic_empty(e.message), "card detail")
    status = dict(d["status"])
    name = status.get("Name") or str(pid)
    if pid <= 1:
        signals = '<p class="sub">Init takes signals only through the power controls on the overview.</p>'
    else:
        signals = '<div class="buttons">' + "".join(basic_link(f"SIG{sig}", basic_url(tab="confirm", do="signal", pid=pid, signal=sig, name=name), "btn small danger" if sig in ("KILL", "TERM") else "btn small") for sig in PROCESS_SIGNALS) + "</div>"
    files = "".join(f'<tr><td class="num">{esc(f["fd"])}</td><td class="mono">{esc(f["target"])}</td></tr>' for f in d["files"])
    return "".join([
        '<div class="card detail">',
        f"<h3>PID {pid} · {esc(name)}</h3>",
        f'<div class="argv mono">{esc(" ".join(d["argv"]) if d["argv"] else "(no arguments)")}</div>',
        signals,
        "<h4>Status</h4>",
        basic_facts(d["status"] + d["io"], mono=True),
        f'<h4>Open files ({len(d["files"])})</h4>',
        f'<div class="table-wrap"><table>{files}</table></div>' if files else basic_empty("No open files."),
        "</div>",
    ])


def basic_processes(handler, args):
    procs = handler.server.processes.scan()
    q = args.get("q", "").strip()
    needle = q.lower()
    columns = {key: numeric for key, _, numeric in BASIC_PROC_COLUMNS}
    key = args.get("sort") if args.get("sort") in columns else "cpu"
    numeric = columns[key]
    direction = args.get("dir") if args.get("dir") in ("asc", "desc") else ("desc" if numeric else "asc")
    selected = int(args["pid"]) if args.get("pid", "").isdigit() else None
    rows = sorted((p for p in procs if not needle or any(needle in str(p[k]).lower() for k in ("pid", "name", "cmdline", "user"))), key=lambda p: p["pid"])
    rows.sort(key=lambda p: (-1 if p[key] is None else p[key]) if numeric else str(p[key]).lower(), reverse=direction == "desc")
    keep = {"q": q or None, "refresh": args.get("refresh")}
    head = []
    for column, label, is_numeric in BASIC_PROC_COLUMNS:
        flip = ("asc" if direction == "desc" else "desc") if column == key else ("desc" if is_numeric else "asc")
        arrow = (" ▲" if direction == "asc" else " ▼") if column == key else ""
        head.append(f'<th{basic_class("num" if is_numeric else None)}><a href="{esc(basic_url(tab="processes", sort=column, dir=flip, pid=selected, **keep))}">{esc(label)}{arrow}</a></th>')
    body = []
    for p in rows:
        word, tone = BASIC_STATES.get(p["state"], ("unknown", "neutral"))
        href = esc(basic_url(tab="processes", sort=key, dir=direction, pid=p["pid"], **keep))
        body.append("".join([
            f'<tr{basic_class("selected" if p["pid"] == selected else None)}>',
            f'<td class="num"><a href="{href}">{p["pid"]}</a></td>',
            f"<td>{esc(p['user'])}</td>",
            f'<td><a href="{href}">{esc(p["name"])}</a></td>',
            f"<td>{basic_state(tone, p['state'] + ' ' + word)}</td>",
            f'<td class="num">{p["cpu"]:.1f}</td>',
            f'<td class="num">{esc(fmt_kb(p["rss"]))}</td>',
            f'<td class="num">{p["threads"]}</td>',
            f'<td class="num">{"—" if p["fds"] is None else p["fds"]}</td>',
            f'<td class="cmd" title="{esc(p["cmdline"])}">{esc(p["cmdline"])}</td>',
            "</tr>",
        ]))
    if not body:
        body.append(basic_empty("No process matches the filter." if procs else "No processes.", len(BASIC_PROC_COLUMNS)))
    count = f"{len(rows)} of {len(procs)} processes" if needle else f"{len(procs)} processes"
    search = f'<form method="get" action="/basic" class="inline">{basic_hidden({"tab": "processes", "sort": key, "dir": direction, "refresh": args.get("refresh")})}<input type="text" name="q" value="{esc(q)}" size="32" placeholder="Filter by name, command, user or pid"> <input type="submit" class="btn" value="Filter"></form>'
    refresh = basic_link("Refresh", basic_url(tab="processes", sort=key, dir=direction, pid=selected, **keep))
    table = f'<div class="card table-wrap"><table><thead><tr>{"".join(head)}</tr></thead><tbody>{"".join(body)}</tbody></table></div>'
    detail = basic_process_detail(handler, selected) if selected is not None else basic_card(None, None, basic_empty("Select a process to inspect it and send signals."))
    return f'<div class="toolbar">{search} {refresh} {basic_refresh_link(args, "processes")} <span class="sub">{esc(count)}</span></div>{detail}{table}'


def basic_services(handler, args):
    rows = []
    for s in list_services():
        if s["running"]:
            status = basic_state("good", f"running (pid {s['pid']})")
        else:
            status = basic_state("warning" if s["stale"] else "neutral", "stopped, stale pid file" if s["stale"] else "stopped")
        actions = [
            basic_disabled("Start") if s["running"] else basic_form(handler, "Start", {"do": "service", "name": s["name"], "action": "start", "back": "services"}, "btn small"),
            basic_link("Stop", basic_url(tab="confirm", do="service", name=s["name"], action="stop"), "btn small") if s["running"] or s["stale"] else basic_disabled("Stop"),
            basic_link("Restart", basic_url(tab="confirm", do="service", name=s["name"], action="restart"), "btn small"),
            basic_link("Log", basic_url(tab="logs", path=s["log"]), "btn small") if s["log"] else basic_disabled("Log"),
        ]
        tag = "" if s["enabled"] else '<span class="tag">not executable</span>'
        rows.append(f'<tr><td><span class="mono">{esc(s["name"])}</span>{tag}</td><td>{esc(s["description"])}</td><td>{status}</td><td>{"".join(actions)}</td></tr>')
    body = "".join(rows) or basic_empty("No scripts in /etc/init.d.", 4)
    table = f"<table><thead><tr><th>Service</th><th>Description</th><th>Status</th><th>Actions</th></tr></thead><tbody>{body}</tbody></table>"
    return basic_card("Services", "Scripts in /etc/init.d, tracked by their pid files in /var/run/init.d. Output goes to /var/log/init.d.", table, "card table-wrap")


def basic_log_meta(l):
    if not l.get("kernel"):
        return f"{fmt_bytes(l['size'])} · {fmt_time(l['mtime'])}"
    if l["error"] and not l["size"]:
        return f"{l['path']} · {l['error']}"
    return f"{l['path']} · {fmt_bytes(l['size'])} kept · {fmt_time(l['mtime'])}"


def basic_logs(handler, args):
    kernel = handler.server.kmsg.entry()
    logs = ([kernel] if kernel else []) + list_logs()
    path = args.get("path") or (logs[0]["path"] if logs else None)
    size = basic_size(args)
    links = "".join(f'<a class="logitem{" current" if l["path"] == path else ""}" href="{esc(basic_url(tab="logs", path=l["path"], bytes=size, refresh=args.get("refresh")))}"><span class="mono">{esc("Kernel messages" if l.get("kernel") else l["path"].removeprefix(LOG_DIR + "/"))}</span><span class="meta">{esc(basic_log_meta(l))}</span></a>' for l in logs)
    listing = basic_card("Log files", f"Kernel messages from {KMSG_PATH}, and the files under {LOG_DIR}", links or basic_empty(f"No files under {LOG_DIR}."))
    if path is None:
        return listing
    try:
        if path == KMSG_PATH:
            real = KMSG_PATH
            text, total = handler.server.kmsg.tail(size)
        else:
            real = resolve_log(path)
            text, total = tail_file(real, size)
        title = f"{real} · {fmt_bytes(total)}{' (showing the end)' if total > size else ''}"
        view = f'<pre class="out">{esc(text or "(empty)")}</pre>'
    except (ApiError, OSError) as e:
        title, view = path, basic_empty(e.message if isinstance(e, ApiError) else str(e))
    tools = f'<div class="toolbar">{basic_size_form({"tab": "logs", "path": path, "refresh": args.get("refresh")}, size, "Last")} {basic_refresh_link(args, "logs")}</div>'
    return listing + basic_card(None, None, f'<h3 class="mono">{esc(title)}</h3>{tools}{view}')


def basic_mounts(handler, args):
    mounts, filesystems = list_mounts(), read_filesystems()
    selected = int(args["mount"]) if args.get("mount", "").isdigit() and int(args["mount"]) < len(mounts) else None
    rows = []
    for i, m in enumerate(mounts):
        u = m["usage"] if m["usage"] and m["usage"]["blocks"] > 0 else None
        share = used_share(u)
        note = "covered" if m["covered"] else "statfs failed" if m["error"] else None
        inodes = f"{round(100 * (m['usage']['inodes'] - m['usage']['inodes_free']) / m['usage']['inodes'])}%" if m["usage"] and m["usage"]["inodes"] else "—"
        classes = " ".join(c for c in ("covered" if m["covered"] else "", "selected" if i == selected else "") if c)
        tag = f'<span class="tag">{esc(note)}</span>' if note else ""
        use = f'{basic_meter(share)}<span class="pct">{round(share * 100)}%</span>' if u else "—"
        rows.append("".join([
            f'<tr{basic_class(classes)}>',
            f'<td class="mono"><a href="{esc(basic_url(tab="mounts", mount=i))}">{esc(m["target"])}</a>{tag}</td>',
            f'<td class="mono">{esc(m["source"])}</td>',
            f"<td>{esc(m['type'])}</td>",
            f'<td class="num">{esc(fmt_bytes(u["size"])) if u else "—"}</td>',
            f'<td class="num">{esc(fmt_bytes(u["used"])) if u else "—"}</td>',
            f'<td class="num">{esc(fmt_bytes(u["avail"])) if u else "—"}</td>',
            f'<td>{use}</td>',
            f'<td class="num">{inodes}</td>',
            "</tr>",
        ]))
    body = "".join(rows) or basic_empty("No mounts in /proc/mounts.", 8)
    table = f'<table class="mounts"><thead><tr><th>Mountpoint</th><th>Source</th><th>Type</th><th class="num">Size</th><th class="num">Used</th><th class="num">Available</th><th>Use</th><th class="num">Inodes</th></tr></thead><tbody>{body}</tbody></table>'
    out = [basic_card("Mounts", f"{len(mounts)} mount{'' if len(mounts) == 1 else 's'} from /proc/mounts, in mount order, with usage from statfs", table, "card table-wrap")]
    if selected is None:
        out.append(basic_card(None, None, basic_empty("Select a mount to see its filesystem.")))
    else:
        m = mounts[selected]
        u = m["usage"]
        facts = [("Source", m["source"]), ("Mountpoint", m["target"]), ("Type", m["type"]), ("Options", ",".join(m["options"])), ("Access", "read only" if m["readonly"] else "read and write")]
        if u and u["blocks"]:
            facts += [
                ("Size", f"{fmt_bytes(u['size'])} in {u['blocks']} blocks"),
                ("Used", f"{fmt_bytes(u['used'])}, {round(used_share(u) * 100)}%"),
                ("Available", f"{fmt_bytes(u['avail'])}, plus {fmt_bytes(u['free'] - u['avail'])} reserved" if u["free"] > u["avail"] else fmt_bytes(u["avail"])),
            ]
        elif u:
            facts.append(("Size", "not counted"))
        if u:
            facts += [
                ("Block size", f"{fmt_bytes(u['block_size'])}, fragments of {fmt_bytes(u['fragment_size'])}" if u["fragment_size"] != u["block_size"] else fmt_bytes(u["block_size"])),
                ("Inodes", f"{u['inodes'] - u['inodes_free']} used of {u['inodes']}, {u['inodes_free']} free" if u["inodes"] else "not counted"),
                ("Longest name", f"{u['namemax']} bytes"),
            ]
        note = "A later mount on the same path hides this one, so statfs reaches that one instead." if m["covered"] else f"statfs failed: {m['error']}" if m["error"] else None
        browse = basic_disabled("Browse") if m["covered"] else basic_link("Browse", basic_url(tab="files", path=m["target"]), "btn small")
        out.append("".join([
            '<div class="card detail">',
            f'<h3 class="mono">{esc(m["target"])}</h3>',
            f'<p class="sub">{esc(m["type"])} from {esc(m["source"])}</p>',
            f'<div class="buttons">{browse}</div>',
            f'<p class="sub">{esc(note)}</p>' if note else "",
            "<h4>Filesystem</h4>",
            basic_facts(facts, mono=True),
            "</div>",
        ]))
    counts = collections.Counter(m["type"] for m in mounts)
    types = "".join(f'<tr><td class="mono">{esc(f["name"])}</td><td>{"none (nodev)" if f["nodev"] else "required"}</td><td class="num">{counts[f["name"]]}</td></tr>' for f in filesystems) or basic_empty("/proc/filesystems is empty.", 3)
    out.append(basic_card("Filesystem types", "From /proc/filesystems. A nodev type mounts without a source.", f'<table><thead><tr><th>Type</th><th>Source</th><th class="num">Mounts</th></tr></thead><tbody>{types}</tbody></table>', "card table-wrap"))
    return "".join(out)


def basic_plural(n, word):
    return f"{n} {word}{'' if n == 1 else 's'}"


def basic_named(name, code):
    return f"{name} ({code})" if name else code or "—"


def basic_device_row(key, selected, cells, cls=None):
    classes = " ".join(c for c in (cls, "selected" if key == selected else None) if c)
    link = f'<a href="{esc(basic_url(tab="devices", dev=key))}">{esc(cells[0])}</a>'
    rest = "".join(f'<td{basic_class(c)}>{esc(v)}</td>' for v, c in cells[1:])
    return f'<tr{basic_class(classes)}><td class="mono">{link}</td>{rest}</tr>'


def basic_device_detail(devices, key):
    for d in devices["pci"]:
        if f"pci:{d['slot']}" == key:
            title, sub, path = d["device_name"] or f"Device {d['device']}", f"{d['class_name']} · {d['slot']}", f"/sys/bus/pci/devices/{d['slot']}"
            facts = [("Slot", d["slot"]), ("Vendor", basic_named(d["vendor_name"], d["vendor"])), ("Device", basic_named(d["device_name"], d["device"])), ("Subsystem", d["subsystem"] or "—"), ("Class", f"{d['class_name']} ({d['class']})")]
            return title, sub, path, facts, d
    for d in devices["usb"]:
        if f"usb:{d['name']}" == key:
            title, sub, path = d["product_name"] or f"Product {d['product']}", f"{d['class_name']} · port {d['name']}", f"/sys/bus/usb/devices/{d['name']}"
            facts = [("Port", d["name"]), ("Bus", d["bus"]), ("Address", d["address"]), ("Vendor", basic_named(d["vendor_name"], d["vendor"])), ("Product", basic_named(d["product_name"], d["product"])), ("Release", d["release"] or "—"), ("Class", d["class_name"]), ("Interfaces", len(d["interfaces"]))]
            return title, sub, path, facts, d
        for i in d["interfaces"]:
            if f"usb:{i['name']}" == key:
                title, sub, path = f"Interface {i['number']}", f"{i['class_name']} · {d['product_name'] or d['name']}", f"/sys/bus/usb/devices/{i['name']}"
                facts = [("Interface", i["name"]), ("Device", basic_named(d["product_name"], d["name"])), ("Class", f"{i['class_name']} ({i['class']:02x}/{i['subclass']:02x}/{i['protocol']:02x})")]
                return title, sub, path, facts, i
    return None


def basic_devices(handler, args):
    devices = read_devices()
    selected = args.get("dev")
    out = [] if devices["sysfs"] else [basic_card(None, None, basic_empty("There is no /sys/bus here, so the kernel lists no PCI or USB devices."))]
    pci = "".join(basic_device_row(f"pci:{d['slot']}", selected, [d["slot"], (d["class_name"], None), (d["vendor_name"] or d["vendor"], None), (d["device_name"] or d["device"], None), (f"{d['vendor']}:{d['device']}" if d["vendor"] else "—", "mono")]) for d in devices["pci"]) or basic_empty("No PCI devices in /sys/bus/pci/devices.", 5)
    out.append(basic_card("PCI", f"{basic_plural(len(devices['pci']), 'function')} from /sys/bus/pci/devices", f'<table><thead><tr><th>Slot</th><th>Class</th><th>Vendor</th><th>Device</th><th>ID</th></tr></thead><tbody>{pci}</tbody></table>', "card table-wrap"))
    rows = []
    for d in devices["usb"]:
        rows.append(basic_device_row(f"usb:{d['name']}", selected, [d["name"], (d["bus"], "num"), (d["address"], "num"), (d["class_name"], None), (d["vendor_name"] or d["vendor"], None), (d["product_name"] or d["product"], None), (f"{d['vendor']}:{d['product']}" if d["vendor"] else "—", "mono")]))
        for i in d["interfaces"]:
            kind = f"{i['class']:02x}/{i['subclass']:02x}/{i['protocol']:02x}" + (f" · {i['driver']}" if i["driver"] else "")
            rows.append(basic_device_row(f"usb:{i['name']}", selected, [f"interface {i['number']}", ("", None), ("", None), (i["class_name"], None), (kind, "mono"), ("", None), ("", None)], "iface"))
    interfaces = sum(len(d["interfaces"]) for d in devices["usb"])
    usb = "".join(rows) or basic_empty("No USB devices in /sys/bus/usb/devices.", 7)
    out.append(basic_card("USB", f"{basic_plural(len(devices['usb']), 'device')} and {basic_plural(interfaces, 'interface')} from /sys/bus/usb/devices", f'<table class="usb"><thead><tr><th>Port</th><th class="num">Bus</th><th class="num">Address</th><th>Class</th><th>Vendor</th><th>Product</th><th>ID</th></tr></thead><tbody>{usb}</tbody></table>', "card table-wrap"))
    found = basic_device_detail(devices, selected) if selected else None
    if found is None:
        out.append(basic_card(None, None, basic_empty("Select a PCI or USB device to see its details.")))
    else:
        title, sub, path, facts, d = found
        if d["driver"]:
            facts.append(("Driver", d["driver"]))
        facts.append(("sysfs", path))
        uevent = "\n".join(f"{k}={v}" for k, v in d["uevent"].items()) or "(empty)"
        out.append(f'<div class="card detail"><h3>{esc(title)}</h3><p class="sub">{esc(sub)}</p>{basic_facts(facts, mono=True)}<h4>uevent</h4><pre class="out">{esc(uevent)}</pre></div>')
    nodes = "".join(f'<tr><td class="mono">{esc(n["path"])}</td><td>{esc(n["kind"])}</td><td class="num">{n["major"]}</td><td class="num">{n["minor"]}</td><td class="mono">{esc(n["mode"])}</td><td>{esc(n["owner"])}:{esc(n["group"])}</td></tr>' for n in devices["nodes"]) or basic_empty("No device nodes in /dev.", 6)
    out.append(basic_card("Device nodes", f"{basic_plural(len(devices['nodes']), 'character and block device')} in /dev", f'<table><thead><tr><th>Path</th><th>Type</th><th class="num">Major</th><th class="num">Minor</th><th>Mode</th><th>Owner</th></tr></thead><tbody>{nodes}</tbody></table>', "card table-wrap"))
    return "".join(out)


def basic_files(handler, args):
    size = basic_size(args)
    errors = []
    preview = args.get("file")
    try:
        path, st = resolve_path(args.get("path") or "/")
        if not stat.S_ISDIR(st.st_mode):
            if stat.S_ISREG(st.st_mode):
                preview = path
            else:
                errors.append(f"{path} is a {file_kind(st.st_mode)} file, and the explorer reads only regular files")
            path = os.path.dirname(path)
        listing = list_directory(path)
    except ApiError as e:
        errors.append(e.message)
        listing = list_directory("/")
    path = listing["path"]
    parts = [p for p in path.split("/") if p]
    crumbs = [basic_link("/", basic_url(tab="files", path="/"), "crumb")]
    for i, part in enumerate(parts):
        if i:
            crumbs.append('<span class="sep">/</span>')
        crumbs.append(basic_link(part, basic_url(tab="files", path="/" + "/".join(parts[:i + 1])), "crumb"))
    rows = []
    for entry in listing["entries"]:
        child = os.path.join(path, entry["name"])
        kind = entry["kind"]
        if kind in ("dir", "link"):
            href = basic_url(tab="files", path=child, bytes=size)
        elif kind == "file":
            href = basic_url(tab="files", path=path, file=child, bytes=size)
        else:
            href = None
        label = esc(entry["name"] + "/" if kind == "dir" else entry["name"])
        name = f'<a href="{esc(href)}"{basic_class("dir" if kind == "dir" else None)}>{label}</a>' if href else label
        if kind == "link":
            name += f'<span class="target"> → {esc(entry.get("target"))}</span>'
        elif href is None:
            name += f'<span class="tag">{esc(kind)}</span>'
        rows.append("".join([
            f'<tr{basic_class("selected" if child == preview else None)}>',
            f'<td class="name">{name}</td>',
            f'<td class="num">{esc(fmt_bytes(entry["size"])) if kind == "file" and "size" in entry else "—"}</td>',
            f'<td class="mono">{esc(entry.get("mode") or "—")}</td>',
            f'<td>{esc(entry["user"] + ":" + entry["group"]) if entry.get("user") else "—"}</td>',
            f'<td>{esc(fmt_time(entry.get("mtime")))}</td>',
            "</tr>",
        ]))
    body = "".join(rows) or basic_empty("Empty directory.", 5)
    up = basic_link("Up", basic_url(tab="files", path=listing["parent"])) if listing["parent"] else basic_disabled("Up")
    form = f'<form method="get" action="/basic" class="toolbar">{up} {basic_hidden({"tab": "files", "bytes": size})}<input type="text" name="path" value="{esc(path)}" size="48"> <input type="submit" class="btn primary" value="Open"> {basic_link("Refresh", basic_url(tab="files", path=path, file=preview, bytes=size))}</form>'
    notice = "".join(f'<div class="card notice error"><pre class="out">{esc(e)}</pre></div>' for e in errors)
    table = f'<div class="crumbs">{"".join(crumbs)}</div><table><thead><tr><th>Name</th><th class="num">Size</th><th>Mode</th><th>Owner</th><th>Modified</th></tr></thead><tbody>{body}</tbody></table><p class="sub">{len(listing["entries"])} entries{", listing cut short" if listing["truncated"] else ""}</p>'
    return form + notice + basic_file_preview(preview, path, size) + basic_card(None, None, table, "card table-wrap")


def basic_file_preview(preview, path, size):
    if not preview:
        return basic_card(None, None, '<p class="sub">Select a file to preview it. Read only. Only regular files are opened.</p>')
    try:
        r = preview_file(preview, size)
    except ApiError as e:
        return basic_card(None, None, f'<h3 class="mono">{esc(preview)}</h3><pre class="out">{esc(e.message)}</pre>')
    what = "binary, hex dump of the start" if r["binary"] else "showing the start" if r["truncated"] else "whole file"
    meta = f"{fmt_bytes(r['size'] or r['read'])} · {r['mode']} · {r['user']}:{r['group']} · {fmt_time(r['mtime'])} · {what}"
    download = basic_link("Download", "/api/download?" + urllib.parse.urlencode({"path": r["path"]}))
    tools = f'<div class="toolbar">{basic_size_form({"tab": "files", "path": path, "file": preview}, size, "First")} {download}</div>'
    return basic_card(None, None, f'<h3 class="mono">{esc(r["path"])}</h3><p class="sub">{esc(meta)}</p>{tools}<pre class="out">{esc(r["text"] or "(empty)")}</pre>')


def basic_console(handler, args):
    note = handler.server.notes.get(args.get("note")) or {}
    cwd = note.get("cwd") or "/"
    timeout = note.get("timeout") or 30
    fields = basic_hidden({"token": handler.server.token, "do": "exec", "back": "console"})
    form = "".join([
        f'<form method="post" action="/basic">{fields}',
        '<table class="form"><tr><th>Directory</th>',
        f'<td><input type="text" name="cwd" value="{esc(cwd)}" size="24"></td></tr>',
        '<tr><th>Command</th><td><input type="text" name="command" size="64"></td></tr>',
        f'<tr><th>Timeout</th><td><input type="text" name="timeout" value="{esc(timeout)}" size="4"> s</td></tr>',
        '<tr><th></th><td><input type="submit" class="btn primary" value="Run"></td></tr>',
        "</table></form>",
    ])
    return basic_card("Console", "Each command runs with /bin/sh -c, without a terminal; stdin is empty. The result shows above.", form)


def basic_confirm(handler, args):
    action = args.get("do")
    if action == "power":
        what = args.get("action")
        if what not in POWER_WORDS:
            raise ApiError(HTTPStatus.BAD_REQUEST, f"unknown power action {what}")
        question = f"{POWER_WORDS[what]} {socket.gethostname()} now? Every process gets SIGTERM."
        label, fields, back = POWER_WORDS[what], {"do": "power", "action": what, "back": "overview"}, basic_url(tab="overview")
    elif action == "signal":
        pid = int_arg(args, "pid")
        sig = str(args.get("signal", "TERM")).upper()
        question = f"Send SIG{sig} to {pid} ({args.get('name') or pid})?"
        label, fields, back = f"Send SIG{sig}", {"do": "signal", "pid": pid, "signal": sig, "back": "processes"}, basic_url(tab="processes", pid=pid)
    elif action == "service":
        name, what = str(args.get("name", "")), str(args.get("action", ""))
        question = f"{what.capitalize()} {name}?"
        label, fields, back = what.capitalize(), {"do": "service", "name": name, "action": what, "back": "services"}, basic_url(tab="services")
    else:
        raise ApiError(HTTPStatus.BAD_REQUEST, "nothing to confirm")
    return basic_card("Confirm", None, f'<p class="question">{esc(question)}</p><div class="buttons">{basic_form(handler, label, fields, "btn danger")}{basic_link("Cancel", back)}</div>')


BASIC_VIEWS = {
    "overview": basic_overview,
    "processes": basic_processes,
    "services": basic_services,
    "logs": basic_logs,
    "mounts": basic_mounts,
    "devices": basic_devices,
    "files": basic_files,
    "console": basic_console,
    "confirm": basic_confirm,
}

BASIC_ACTIONS = {
    "signal": route_signal,
    "service": route_service,
    "exec": route_exec,
    "sync": route_sync,
    "power": route_power,
}


def basic_action_title(args):
    action = args.get("do")
    if action == "exec":
        return f"{args.get('cwd') or '/'} $ {args.get('command', '')}"
    if action == "signal":
        return f"SIG{str(args.get('signal', 'TERM')).upper()} to {args.get('pid')}"
    if action == "service":
        return f"{args.get('name')}: {args.get('action')}"
    if action == "power":
        return POWER_WORDS.get(args.get("action"), "Power")
    return "Sync disks"


def basic_action(handler, args):
    note = {"title": basic_action_title(args), "ok": False, "cwd": args.get("cwd"), "timeout": args.get("timeout")}
    if not hmac.compare_digest(str(args.get("token", "")).encode(), handler.server.token.encode()):
        return dict(note, text="This form came from an older dashboard. Reload the page and try again.\n")
    route = BASIC_ACTIONS.get(args.get("do"))
    if route is None:
        return dict(note, text=f"no action {args.get('do')}\n")
    try:
        r = route(handler, args)
    except ApiError as e:
        return dict(note, text=e.message + "\n")
    if args.get("do") != "exec":
        text = r["output"] + ("the dashboard will disconnect.\n" if args.get("do") == "power" else "")
        return dict(note, text=text or "(no output)", ok=True)
    what = "killed after timeout" if r["timed_out"] else "failed to start" if r["code"] is None else f"killed by signal {-r['code']}" if r["code"] < 0 else f"exit {r['code']}"
    return dict(note, text=r["output"] + ("\n[output truncated]" if r["truncated"] else ""), ok=r["code"] == 0, meta=f"{what} · {r['elapsed']:.2f} s")


def basic_post(handler, args):
    note = handler.server.add_note(basic_action(handler, args))
    back = args.get("back") if args.get("back") in dict(BASIC_TABS) else "overview"
    pid = args.get("pid") if back == "processes" else None
    return basic_url(tab=back, pid=pid, note=note)


def basic_note(note):
    if not note:
        return ""
    meta = f'<div class="meta">{basic_state("good" if note["ok"] else "critical", note["meta"])}</div>' if note.get("meta") else ""
    text = f'<pre class="out">{esc(note["text"])}</pre>' if note.get("text") else ""
    return f'<div class="card notice{"" if note["ok"] else " error"}"><div class="line">{esc(note["title"])}</div>{text}{meta}</div>'


def render_basic(handler, args):
    tab = args.get("tab") if args.get("tab") in BASIC_VIEWS else "overview"
    current = CONFIRM_TABS.get(args.get("do"), "overview") if tab == "confirm" else tab
    try:
        body = BASIC_VIEWS[tab](handler, args)
    except ApiError as e:
        body = f'<div class="card notice error"><pre class="out">{esc(e.message)}</pre></div>'
    uname = os.uname()
    host = socket.gethostname()
    nav = "".join(f'<a href="{esc(basic_url(tab=name))}"{basic_class("current" if name == current else None)}>{label}</a>' for name, label in BASIC_TABS)
    refresh = ""
    if basic_refreshing(args, tab):
        again = {k: v for k, v in args.items() if k != "note"}
        refresh = f'<meta http-equiv="refresh" content="{BASIC_REFRESH_SECONDS}; url={esc(basic_url(**again))}">'
    note = basic_note(handler.server.notes.get(args.get("note")))
    page = f"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
{refresh}<title>{esc(host)} · aplus dashboard</title>
<style>{BASIC_CSS}</style>
</head>
<body>
<div class="top"><a class="full" href="/">Full view</a><span class="brand">aplus</span><span class="host">{esc(host)}</span><span class="release">{esc(uname.sysname)} {esc(uname.release)} · {esc(uname.machine)}</span></div>
<div class="nav">{nav}</div>
<div class="main">{note}{body}</div>
</body>
</html>
"""
    return page.encode("utf-8")


BASIC_CSS = """
body { margin: 0; background: #f9f9f7; color: #0b0b0b; font-family: sans-serif; font-size: 14px; line-height: 1.45; }
a { color: #2a78d6; }
h3 { font-size: 15px; margin: 0 0 4px; }
h4 { font-size: 12px; color: #52514e; margin: 16px 0 6px; }
.top { background: #fcfcfb; border-bottom: 1px solid #e1e0d9; padding: 12px 16px; }
.top .brand, .top .host { font-weight: bold; margin-right: 12px; }
.top .release { color: #52514e; font-size: 13px; }
.top .full { float: right; font-size: 13px; }
.nav { background: #fcfcfb; border-bottom: 1px solid #e1e0d9; padding: 0 8px; }
.nav a { display: inline-block; padding: 9px 10px; color: #52514e; text-decoration: none; border-bottom: 2px solid #fcfcfb; }
.nav a.current { color: #0b0b0b; font-weight: bold; border-bottom: 2px solid #2a78d6; }
.main { padding: 16px; max-width: 1400px; margin: 0 auto; }
.card { background: #fcfcfb; border: 1px solid #e1e0d9; border-radius: 10px; padding: 14px 16px; margin: 0 0 16px 0; }
.card h2 { font-size: 13px; margin: 0 0 2px; }
.sub { color: #52514e; font-size: 12px; margin: 0 0 10px; }
.tiles { margin: 0 0 4px; }
.tile { display: inline-block; vertical-align: top; width: 200px; margin: 0 12px 12px 0; }
.tile .label { color: #52514e; font-size: 13px; }
.tile .value { font-size: 26px; font-weight: bold; }
.tile .note { color: #52514e; font-size: 12px; }
.meter { height: 6px; background: #cde2fb; margin-top: 8px; overflow: hidden; }
.meter div { height: 6px; background: #2a78d6; }
.meter.warning div { background: #fab219; }
.meter.critical div { background: #d03b3b; }
img.chart { display: block; max-width: 100%; height: auto; border: 0; }
.axis { color: #898781; font-size: 11px; max-width: 600px; }
.axis .right { float: right; }
.table-wrap { overflow: auto; }
table { border-collapse: collapse; width: 100%; font-size: 13px; }
th, td { text-align: left; padding: 5px 8px; border-bottom: 1px solid #e1e0d9; white-space: nowrap; vertical-align: top; }
th { color: #52514e; font-size: 12px; }
th a { color: #52514e; text-decoration: none; }
td.num, th.num { text-align: right; }
td.cmd { max-width: 420px; overflow: hidden; text-overflow: ellipsis; font-family: monospace; font-size: 12px; color: #52514e; }
td.wide { width: 100%; }
td.wide .meter { margin: 6px 0 0; }
tr.selected td { background: #f0efec; }
tr.covered td { color: #898781; }
table.facts th { font-weight: normal; width: 1%; }
table.facts td { white-space: normal; word-wrap: break-word; }
table.form th { font-weight: normal; width: 1%; border: 0; }
table.form td { border: 0; }
table.usb tr.iface td { color: #52514e; font-size: 12px; }
table.usb tr.iface td:first-child { padding-left: 26px; }
table.mounts .meter { display: inline-block; width: 64px; margin: 0 8px 0 0; vertical-align: middle; }
.mono, pre, code, td.name { font-family: monospace; font-size: 12.5px; }
td.name a.dir { font-weight: bold; }
td.name .target { color: #52514e; }
pre.out { background: #f0efec; padding: 10px; margin: 0; overflow: auto; white-space: pre-wrap; word-wrap: break-word; max-height: 600px; }
.argv { background: #f0efec; padding: 8px 10px; margin: 8px 0 12px; word-wrap: break-word; }
.toolbar { margin: 0 0 12px; }
.buttons { margin: 4px 0; }
.btn { display: inline-block; background: #fcfcfb; border: 1px solid #c3c2b7; border-radius: 6px; padding: 3px 10px; margin: 0 4px 4px 0; color: #0b0b0b; text-decoration: none; font-size: 13px; cursor: pointer; }
.btn.small { padding: 1px 8px; font-size: 12px; }
.btn.primary { background: #2a78d6; border-color: #2a78d6; color: #ffffff; }
.btn.danger { background: #d03b3b; border-color: #d03b3b; color: #ffffff; }
.btn.disabled { color: #898781; border-color: #e1e0d9; cursor: default; }
form.inline { display: inline; margin: 0; }
input[type=text], select { background: #fcfcfb; border: 1px solid #c3c2b7; padding: 3px 6px; font-size: 13px; }
.dot { display: inline-block; width: 8px; height: 8px; border-radius: 4px; background: #898781; margin-right: 6px; }
.dot.good { background: #0ca30c; }
.dot.warning { background: #fab219; }
.dot.serious { background: #ec835a; }
.dot.critical { background: #d03b3b; }
.dot.neutral { width: 6px; height: 6px; background: #fcfcfb; border: 1px solid #898781; }
.tag { display: inline-block; font-size: 11px; padding: 0 6px; margin-left: 8px; background: #f0efec; color: #52514e; }
.empty { color: #52514e; padding: 20px 0; text-align: center; }
.notice { border-left: 3px solid #2a78d6; }
.notice.error { border-left: 3px solid #d03b3b; }
.notice .line { font-family: monospace; font-weight: bold; margin: 0 0 6px; }
.notice .meta { color: #52514e; font-size: 11px; margin-top: 4px; }
.question { margin: 0 0 12px; }
.crumbs { font-family: monospace; margin: 0 0 10px; }
.crumbs a { text-decoration: none; padding: 2px 3px; }
.crumbs .sep { color: #898781; }
.logitem { display: block; padding: 5px 8px; text-decoration: none; color: #0b0b0b; }
.logitem.current { background: #f0efec; border-left: 3px solid #2a78d6; }
.logitem .mono { display: block; font-size: 12px; }
.logitem .meta { display: block; color: #52514e; font-size: 11px; }
"""


## @brief HTTP server holding the dashboard's shared state.
class DashboardServer(ThreadingHTTPServer):
    daemon_threads = True

    ## @brief Creates the server and binds it.
    #  @param address (host, port) to listen on.
    #  @param sampler The running sampler.
    def __init__(self, address, sampler, kmsg):
        super().__init__(address, DashboardHandler)
        self.sampler = sampler
        self.kmsg = kmsg
        self.processes = ProcessTable()
        self.token = secrets.token_urlsafe(24)
        self.notes = collections.OrderedDict()
        self.notes_lock = threading.Lock()

    def add_note(self, note):
        key = secrets.token_hex(8)
        with self.notes_lock:
            self.notes[key] = note
            while len(self.notes) > MAX_NOTES:
                self.notes.popitem(last=False)
        return key

    ## @brief Ignores clients that went away and logs every other request failure.
    #  @param request The client socket.
    #  @param client_address The client's address.
    def handle_error(self, request, client_address):
        if not isinstance(sys.exc_info()[1], ConnectionError):
            log.exception("request from %s failed", client_address[0])


## @brief Request handler serving the page and the JSON API.
class DashboardHandler(BaseHTTPRequestHandler):
    server_version = "pymonitor/1.0"
    protocol_version = "HTTP/1.1"
    timeout = 60

    ## @brief Logs a request line at debug level.
    #  @param fmt Format string.
    #  @param args Format arguments.
    def log_message(self, fmt, *args):
        log.debug("%s %s", self.address_string(), fmt % args)

    ## @brief Logs a protocol error, such as an idle keep-alive timing out, at debug level.
    #  @param fmt Format string.
    #  @param args Format arguments.
    def log_error(self, fmt, *args):
        log.debug("%s %s", self.address_string(), fmt % args)

    ## @brief Answers a GET request.
    def do_GET(self):
        self.dispatch(GET_ROUTES, post=False)

    ## @brief Answers a POST request.
    def do_POST(self):
        self.dispatch(POST_ROUTES, post=True)

    ## @brief Serves the page or runs an API route, turning failures into JSON errors.
    #  @param routes Route table for the request's method.
    #  @param post Whether the request carries a JSON body.
    def dispatch(self, routes, post):
        url = urllib.parse.urlsplit(self.path)
        try:
            if not post and url.path in ("/", "/index.html"):
                self.send_body(HTTPStatus.OK, PAGE, "text/html; charset=utf-8")
                return
            if url.path == "/basic":
                if post:
                    self.send_redirect(basic_post(self, self.read_form()))
                else:
                    self.send_body(HTTPStatus.OK, render_basic(self, dict(urllib.parse.parse_qsl(url.query))), "text/html; charset=utf-8")
                return
            args = self.read_json() if post else dict(urllib.parse.parse_qsl(url.query))
            if not post and url.path == "/api/download":
                self.send_file(args.get("path"))
                return
            route = routes.get(url.path)
            if route is None:
                raise ApiError(HTTPStatus.NOT_FOUND, f"no route {url.path}")
            self.authorize(post)
            self.send_json(HTTPStatus.OK, route(self, args))
        except ApiError as e:
            self.send_json(e.status, {"error": e.message})
        except ConnectionError:
            self.close_connection = True
        except Exception as e:
            log.exception("%s %s failed", self.command, url.path)
            self.send_json(HTTPStatus.INTERNAL_SERVER_ERROR, {"error": f"{type(e).__name__}: {e}"})

    ## @brief Rejects requests POSTs a cross-site form could send.
    #  @param post Whether the request is a POST.
    def authorize(self, post):
        if post and self.headers.get("X-Dashboard") != "1":
            raise ApiError(HTTPStatus.FORBIDDEN, "missing X-Dashboard header")

    ## @brief Reads the request body as a JSON object.
    #  @return The decoded object.
    def read_json(self):
        try:
            data = json.loads(self.read_body() or b"{}")
        except ValueError:
            raise ApiError(HTTPStatus.BAD_REQUEST, "invalid JSON") from None
        if not isinstance(data, dict):
            raise ApiError(HTTPStatus.BAD_REQUEST, "expected a JSON object")
        return data

    def read_body(self):
        try:
            length = int(self.headers.get("Content-Length") or 0)
        except ValueError:
            raise ApiError(HTTPStatus.BAD_REQUEST, "invalid Content-Length") from None
        if length < 0 or length > MAX_BODY:
            self.close_connection = True
            raise ApiError(HTTPStatus.REQUEST_ENTITY_TOO_LARGE, "request body too large")
        return self.rfile.read(length)

    def read_form(self):
        return dict(urllib.parse.parse_qsl(self.read_body().decode("utf-8", "replace"), keep_blank_values=True))

    def send_redirect(self, location):
        self.send_response(HTTPStatus.SEE_OTHER)
        self.send_header("Location", location)
        self.send_header("Content-Length", "0")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()

    ## @brief Sends a JSON response.
    #  @param status HTTP status code.
    #  @param payload Object to encode.
    def send_json(self, status, payload):
        self.send_body(status, json.dumps(payload).encode(), "application/json")

    ## @brief Sends a complete response.
    #  @param status HTTP status code.
    #  @param body Response bytes.
    #  @param content_type Content-Type header value.
    def send_body(self, status, body, content_type):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Content-Security-Policy", CSP)
        self.end_headers()
        self.wfile.write(body)

    ## @brief Streams a regular file as an attachment, reading procfs files whole since they report no size.
    #  @param path Requested path.
    def send_file(self, path):
        path, _ = resolve_file(path)
        try:
            f = open(path, "rb")
        except OSError as e:
            raise os_error(e, path) from None
        with f:
            size = os.fstat(f.fileno()).st_size
            body = f.read() if size == 0 else None
            log.info("%s downloaded %s", self.client_address[0], path)
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", "application/octet-stream")
            self.send_header("Content-Length", str(size if body is None else len(body)))
            self.send_header("Content-Disposition", "attachment; filename*=UTF-8''" + urllib.parse.quote(os.path.basename(path)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.end_headers()
            self.close_connection = True
            if body is not None:
                self.wfile.write(body)
                return
            while size > 0:
                chunk = f.read(min(64 * 1024, size))
                if not chunk:
                    break
                self.wfile.write(chunk)
                size -= len(chunk)


PAGE = r"""<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>aplus dashboard</title>
<noscript><meta http-equiv="refresh" content="0; url=/basic"></noscript>
<script>if (!window.fetch || !window.Promise || !document.documentElement.replaceChildren) location.replace("/basic");</script>
<style>
:root {
  color-scheme: light;
  --page: #f9f9f7;
  --surface: #fcfcfb;
  --surface-2: #f0efec;
  --ink: #0b0b0b;
  --ink-2: #52514e;
  --muted: #898781;
  --grid: #e1e0d9;
  --axis: #c3c2b7;
  --border: rgba(11, 11, 11, 0.10);
  --accent: #2a78d6;
  --accent-ink: #ffffff;
  --track: #cde2fb;
  --good: #0ca30c;
  --warning: #fab219;
  --serious: #ec835a;
  --critical: #d03b3b;
  --shadow: 0 1px 2px rgba(11, 11, 11, 0.06);
  --mono: ui-monospace, "SF Mono", "Cascadia Mono", Menlo, Consolas, monospace;
  --sans: system-ui, -apple-system, "Segoe UI", sans-serif;
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    color-scheme: dark;
    --page: #0d0d0d;
    --surface: #1a1a19;
    --surface-2: #242422;
    --ink: #ffffff;
    --ink-2: #c3c2b7;
    --muted: #898781;
    --grid: #2c2c2a;
    --axis: #383835;
    --border: rgba(255, 255, 255, 0.10);
    --accent: #3987e5;
    --track: #0d366b;
    --shadow: none;
  }
}
:root[data-theme="dark"] {
  color-scheme: dark;
  --page: #0d0d0d;
  --surface: #1a1a19;
  --surface-2: #242422;
  --ink: #ffffff;
  --ink-2: #c3c2b7;
  --muted: #898781;
  --grid: #2c2c2a;
  --axis: #383835;
  --border: rgba(255, 255, 255, 0.10);
  --accent: #3987e5;
  --track: #0d366b;
  --shadow: none;
}
* { box-sizing: border-box; }
[hidden] { display: none !important; }
html, body { margin: 0; }
body { background: var(--page); color: var(--ink); font: 14px/1.45 var(--sans); }
button, input, select { font: inherit; color: inherit; }
header { display: flex; flex-wrap: wrap; align-items: center; gap: 8px 16px; padding: 14px 24px; background: var(--surface); border-bottom: 1px solid var(--border); }
header .brand { font-weight: 700; letter-spacing: 0.02em; }
header .host { font-weight: 600; }
header .release { color: var(--ink-2); font-size: 13px; }
header .spacer { flex: 1; }
header a.basic { color: var(--ink-2); font-size: 13px; }
.conn { display: inline-flex; align-items: center; gap: 6px; font-size: 13px; color: var(--ink-2); }
.dot { width: 8px; height: 8px; border-radius: 50%; background: var(--muted); flex: none; }
.dot.good { background: var(--good); }
.dot.warning { background: var(--warning); }
.dot.serious { background: var(--serious); }
.dot.critical { background: var(--critical); }
.dot.neutral { background: transparent; box-shadow: inset 0 0 0 1.5px var(--muted); }
nav { display: flex; gap: 4px; padding: 0 16px; background: var(--surface); border-bottom: 1px solid var(--border); overflow-x: auto; }
nav button { background: none; border: 0; padding: 10px 12px; color: var(--ink-2); cursor: pointer; border-bottom: 2px solid transparent; white-space: nowrap; }
nav button[aria-selected="true"] { color: var(--ink); border-bottom-color: var(--accent); font-weight: 600; }
nav button:hover { color: var(--ink); }
main { padding: 20px 24px 48px; max-width: 1400px; margin: 0 auto; }
section[hidden] { display: none; }
.grid { display: grid; gap: 16px; }
.tiles { grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); }
.charts { grid-template-columns: repeat(auto-fit, minmax(340px, 1fr)); margin-top: 16px; }
.split { grid-template-columns: minmax(0, 2fr) minmax(280px, 1fr); margin-top: 16px; align-items: start; }
.card { background: var(--surface); border: 1px solid var(--border); border-radius: 10px; padding: 16px; box-shadow: var(--shadow); min-width: 0; }
.card h2 { font-size: 13px; font-weight: 600; margin: 0 0 2px; }
.card .sub { color: var(--ink-2); font-size: 12px; margin: 0 0 12px; }
.tile .label { color: var(--ink-2); font-size: 13px; }
.tile .value { font-size: 28px; font-weight: 600; margin-top: 4px; }
.tile .note { color: var(--ink-2); font-size: 12px; margin-top: 2px; }
.meter { height: 6px; border-radius: 3px; background: var(--track); overflow: hidden; margin-top: 10px; }
.meter > span { display: block; height: 100%; background: var(--accent); border-radius: 3px; transition: width 0.3s ease; }
.meter.warning > span { background: var(--warning); }
.meter.critical > span { background: var(--critical); }
.chart { position: relative; }
.chart svg { display: block; width: 100%; overflow: visible; }
.chart svg:focus { outline: 2px solid var(--accent); outline-offset: 4px; border-radius: 4px; }
.chart .axis text { fill: var(--muted); font-size: 11px; font-variant-numeric: tabular-nums; }
.chart .endlabel { fill: var(--ink); font-size: 12px; font-weight: 600; }
.tip { position: absolute; top: 0; pointer-events: none; background: var(--surface); border: 1px solid var(--border); border-radius: 6px; padding: 6px 8px; font-size: 12px; box-shadow: 0 4px 12px rgba(0, 0, 0, 0.12); white-space: nowrap; display: none; }
.tip strong { display: block; font-size: 13px; }
.tip .key { display: inline-block; width: 12px; height: 2px; background: var(--accent); vertical-align: middle; margin-right: 6px; border-radius: 1px; }
.tip span { color: var(--ink-2); }
.cpus { display: grid; gap: 10px; }
.cpu-row { display: grid; grid-template-columns: 56px 1fr 48px; align-items: center; gap: 10px; font-size: 13px; }
.cpu-row .meter { margin: 0; }
.cpu-row .pct { text-align: right; font-variant-numeric: tabular-nums; }
dl.facts { display: grid; grid-template-columns: max-content minmax(0, 1fr); gap: 6px 16px; margin: 0; font-size: 13px; }
dl.facts dt { color: var(--ink-2); }
dl.facts dd { margin: 0; overflow-wrap: anywhere; }
.mono, code, pre { font-family: var(--mono); font-size: 12.5px; }
.toolbar { display: flex; flex-wrap: wrap; align-items: center; gap: 8px 12px; margin-bottom: 12px; }
.toolbar .grow { flex: 1; }
input[type="text"], input[type="search"], input[type="number"], select { background: var(--surface); border: 1px solid var(--axis); border-radius: 6px; padding: 6px 10px; min-width: 0; }
input:focus, select:focus, button:focus-visible { outline: 2px solid var(--accent); outline-offset: 1px; }
label.check { display: inline-flex; align-items: center; gap: 6px; color: var(--ink-2); font-size: 13px; }
.btn { background: var(--surface); border: 1px solid var(--axis); border-radius: 6px; padding: 5px 12px; cursor: pointer; white-space: nowrap; }
.btn:hover:not(:disabled) { background: var(--surface-2); }
.btn:disabled { opacity: 0.45; cursor: default; }
.btn.primary { background: var(--accent); border-color: var(--accent); color: var(--accent-ink); }
.btn.primary:hover:not(:disabled) { filter: brightness(1.08); background: var(--accent); }
.btn.danger { background: var(--critical); border-color: var(--critical); color: #ffffff; }
.btn.danger:hover:not(:disabled) { filter: brightness(1.08); background: var(--critical); }
.btn.small { padding: 2px 8px; font-size: 12px; }
.buttons { display: flex; flex-wrap: wrap; gap: 8px; }
.table-wrap { overflow-x: auto; }
table { border-collapse: collapse; width: 100%; font-size: 13px; }
th, td { text-align: left; padding: 6px 10px; border-bottom: 1px solid var(--grid); white-space: nowrap; }
th { color: var(--ink-2); font-weight: 600; font-size: 12px; position: sticky; top: 0; background: var(--surface); }
th.sortable { cursor: pointer; user-select: none; }
th.sortable:hover { color: var(--ink); }
th[aria-sort="ascending"]::after { content: " \25B2"; font-size: 9px; }
th[aria-sort="descending"]::after { content: " \25BC"; font-size: 9px; }
td.num, th.num { text-align: right; font-variant-numeric: tabular-nums; }
td.cmd { max-width: 420px; overflow: hidden; text-overflow: ellipsis; font-family: var(--mono); font-size: 12px; color: var(--ink-2); }
tbody tr { cursor: pointer; }
tbody tr:hover { background: var(--surface-2); }
tbody tr[aria-selected="true"] { background: var(--surface-2); box-shadow: inset 3px 0 0 var(--accent); }
.state { display: inline-flex; align-items: center; gap: 6px; }
.tag { display: inline-block; font-size: 11px; padding: 1px 6px; border-radius: 4px; background: var(--surface-2); color: var(--ink-2); }
.detail h3 { margin: 0 0 4px; font-size: 15px; }
.detail .argv { background: var(--surface-2); padding: 8px 10px; border-radius: 6px; overflow-wrap: anywhere; margin: 8px 0 12px; }
.detail h4 { margin: 16px 0 6px; font-size: 12px; color: var(--ink-2); font-weight: 600; }
.detail .files { max-height: 240px; overflow: auto; }
.empty { color: var(--ink-2); padding: 24px 0; text-align: center; }
.usage { display: grid; grid-template-columns: 72px 40px; align-items: center; gap: 8px; }
.usage .meter { margin: 0; }
.usage .pct { text-align: right; font-variant-numeric: tabular-nums; }
#mounts-table tr.covered td { color: var(--muted); }
#mounts-table td.mono { max-width: 280px; overflow: hidden; text-overflow: ellipsis; }
#usb-table tr.iface td { color: var(--ink-2); font-size: 12px; }
#usb-table tr.iface td:first-child { padding-left: 26px; }
#nodes-table tbody tr { cursor: default; }
.device-detail { position: sticky; top: 12px; }
pre.out { background: var(--surface-2); border-radius: 8px; padding: 12px; margin: 0; overflow: auto; white-space: pre-wrap; overflow-wrap: anywhere; }
.logs { grid-template-columns: minmax(220px, 300px) minmax(0, 1fr); align-items: start; }
.loglist { display: flex; flex-direction: column; gap: 2px; max-height: 70vh; overflow: auto; }
.loglist button { text-align: left; background: none; border: 0; border-radius: 6px; padding: 6px 8px; cursor: pointer; }
.loglist button:hover { background: var(--surface-2); }
.loglist button[aria-current="true"] { background: var(--surface-2); box-shadow: inset 3px 0 0 var(--accent); }
.loglist .path { display: block; font-family: var(--mono); font-size: 12px; overflow-wrap: anywhere; }
.loglist .meta { display: block; color: var(--ink-2); font-size: 11px; }
#log-view { height: 66vh; }
#console-out { height: 60vh; display: flex; flex-direction: column; gap: 12px; }
.files { grid-template-columns: minmax(0, 3fr) minmax(320px, 2fr); align-items: start; }
.files .preview { position: sticky; top: 12px; }
.crumbs { display: flex; flex-wrap: wrap; align-items: center; gap: 2px; font-family: var(--mono); font-size: 12.5px; margin-bottom: 10px; }
.crumbs button { background: none; border: 0; border-radius: 4px; padding: 2px 4px; cursor: pointer; color: var(--ink-2); }
.crumbs button:hover { background: var(--surface-2); color: var(--ink); }
.crumbs button:last-child { color: var(--ink); font-weight: 600; }
.crumbs span { color: var(--muted); }
#files-table td.name { font-family: var(--mono); font-size: 12.5px; max-width: 360px; overflow: hidden; text-overflow: ellipsis; }
#files-table td.name .dir { font-weight: 600; }
#files-table td.name .target { color: var(--ink-2); }
#files-table td.mono { font-family: var(--mono); font-size: 12px; color: var(--ink-2); }
#file-view { height: 62vh; }
.preview-head { display: flex; flex-wrap: wrap; align-items: center; gap: 8px 12px; margin-bottom: 4px; }
.preview-head .mono { overflow-wrap: anywhere; }
a.btn { text-decoration: none; display: inline-block; color: inherit; }
.cmd-block .line { font-family: var(--mono); font-size: 12.5px; font-weight: 600; }
.cmd-block .line span { color: var(--ink-2); font-weight: 400; }
.cmd-block pre { margin: 4px 0 2px; white-space: pre-wrap; overflow-wrap: anywhere; font-size: 12.5px; }
.cmd-block .meta { font-size: 11px; color: var(--ink-2); display: inline-flex; align-items: center; gap: 6px; }
.console-form { display: grid; grid-template-columns: minmax(120px, 200px) minmax(0, 1fr) 80px auto; gap: 8px; margin-top: 12px; }
.power { display: grid; gap: 12px; }
.power p { margin: 0; color: var(--ink-2); font-size: 12px; }
#toast { position: fixed; left: 50%; bottom: 24px; transform: translateX(-50%); background: var(--ink); color: var(--page); padding: 8px 14px; border-radius: 8px; font-size: 13px; max-width: calc(100vw - 32px); display: none; z-index: 10; white-space: pre-wrap; }
#toast.error { background: var(--critical); color: #ffffff; }
@media (max-width: 860px) {
  header, main { padding-left: 16px; padding-right: 16px; }
  nav { padding: 0 8px; }
  .split, .logs, .files { grid-template-columns: minmax(0, 1fr); }
  .files .preview, .device-detail { position: static; }
  .charts { grid-template-columns: minmax(0, 1fr); }
  .console-form { grid-template-columns: minmax(0, 1fr) 80px; }
  .console-form #cwd { grid-column: 1 / -1; }
}
</style>
</head>
<body>
<noscript><p style="margin:0;padding:10px 24px">JavaScript is off. The <a href="/basic">basic view</a> works without it.</p></noscript>
<header>
  <span class="brand">aplus</span>
  <span class="host" id="host">&mdash;</span>
  <span class="release" id="release"></span>
  <span class="spacer"></span>
  <a class="basic" href="/basic">Basic view</a>
  <span class="conn"><span class="dot" id="conn-dot"></span><span id="conn-text">connecting</span></span>
</header>
<nav role="tablist">
  <button role="tab" data-tab="overview" aria-selected="true">Overview</button>
  <button role="tab" data-tab="processes" aria-selected="false">Processes</button>
  <button role="tab" data-tab="services" aria-selected="false">Services</button>
  <button role="tab" data-tab="logs" aria-selected="false">Logs</button>
  <button role="tab" data-tab="mounts" aria-selected="false">Mounts</button>
  <button role="tab" data-tab="devices" aria-selected="false">Devices</button>
  <button role="tab" data-tab="files" aria-selected="false">Files</button>
  <button role="tab" data-tab="console" aria-selected="false">Console</button>
</nav>
<main>
  <section id="tab-overview">
    <div class="grid tiles">
      <div class="card tile"><div class="label">CPU</div><div class="value" id="t-cpu">&mdash;</div><div class="note" id="t-cpu-note"></div></div>
      <div class="card tile"><div class="label">Memory in use</div><div class="value" id="t-mem">&mdash;</div><div class="note" id="t-mem-note"></div><div class="meter" id="t-mem-meter"><span style="width:0"></span></div></div>
      <div class="card tile"><div class="label">Processes</div><div class="value" id="t-procs">&mdash;</div><div class="note" id="t-procs-note"></div></div>
      <div class="card tile"><div class="label">Uptime</div><div class="value" id="t-uptime">&mdash;</div><div class="note" id="t-uptime-note"></div></div>
      <div class="card tile"><div class="label">Root disk in use</div><div class="value" id="t-disk">&mdash;</div><div class="note" id="t-disk-note"></div><div class="meter" id="t-disk-meter"><span style="width:0"></span></div></div>
    </div>
    <div class="grid charts">
      <div class="card"><h2>CPU usage</h2><p class="sub" id="cpu-sub">All CPUs, percent busy</p><div class="chart" id="cpu-chart"></div></div>
      <div class="card"><h2>Memory in use</h2><p class="sub" id="mem-sub">Physical memory</p><div class="chart" id="mem-chart"></div></div>
    </div>
    <div class="grid split">
      <div class="card"><h2>System</h2><p class="sub">From /proc and uname</p><dl class="facts" id="facts"></dl></div>
      <div class="grid" style="gap:16px">
        <div class="card"><h2>Per-CPU usage</h2><p class="sub">Last sample</p><div class="cpus" id="cpus"></div></div>
        <div class="card power">
          <div><h2>Power</h2><p>Init sends SIGTERM to every process, waits up to 8 s, syncs, then reboots, powers off or halts.</p></div>
          <div class="buttons">
            <button class="btn" id="btn-sync">Sync disks</button>
            <button class="btn danger" data-power="reboot">Reboot</button>
            <button class="btn danger" data-power="poweroff">Power off</button>
            <button class="btn danger" data-power="halt">Halt</button>
          </div>
        </div>
      </div>
    </div>
  </section>

  <section id="tab-processes" hidden>
    <div class="toolbar">
      <input type="search" id="proc-filter" placeholder="Filter by name, command, user or pid" class="grow" aria-label="Filter processes">
      <label class="check"><input type="checkbox" id="proc-auto" checked> Auto refresh</label>
      <span class="sub" id="proc-count" style="color:var(--ink-2);font-size:12px"></span>
    </div>
    <div class="grid split" style="margin-top:0">
      <div class="card table-wrap"><table id="proc-table"><thead><tr></tr></thead><tbody></tbody></table></div>
      <div class="card detail" id="proc-detail"><div class="empty">Select a process to inspect it and send signals.</div></div>
    </div>
  </section>

  <section id="tab-services" hidden>
    <div class="card table-wrap">
      <h2>Services</h2><p class="sub">Scripts in /etc/init.d, tracked by their pid files in /var/run/init.d. Output goes to /var/log/init.d.</p>
      <table id="svc-table"><thead><tr><th>Service</th><th>Description</th><th>Status</th><th>Actions</th></tr></thead><tbody></tbody></table>
    </div>
    <div class="card" style="margin-top:16px"><h2>Output</h2><p class="sub">From the last service action</p><pre class="out" id="svc-output">No action yet.</pre></div>
  </section>

  <section id="tab-logs" hidden>
    <div class="grid logs">
      <div class="card"><h2>Log files</h2><p class="sub">Kernel messages from /dev/kmsg, and the files under /var/log</p><div class="loglist" id="loglist"></div></div>
      <div class="card">
        <div class="toolbar">
          <span class="mono grow" id="log-title">No log selected</span>
          <select id="log-bytes" aria-label="Amount of log to show">
            <option value="16384">Last 16 KB</option>
            <option value="65536" selected>Last 64 KB</option>
            <option value="262144">Last 256 KB</option>
            <option value="1048576">Last 1 MB</option>
          </select>
          <label class="check"><input type="checkbox" id="log-follow" checked> Follow</label>
        </div>
        <pre class="out" id="log-view"></pre>
      </div>
    </div>
  </section>

  <section id="tab-mounts" hidden>
    <div class="card table-wrap">
      <h2>Mounts</h2><p class="sub" id="mounts-sub">From /proc/mounts, in mount order, with usage from statfs</p>
      <table id="mounts-table"><thead><tr><th>Mountpoint</th><th>Source</th><th>Type</th><th class="num">Size</th><th class="num">Used</th><th class="num">Available</th><th>Use</th><th class="num">Inodes</th></tr></thead><tbody></tbody></table>
    </div>
    <div class="grid split">
      <div class="card detail" id="mount-detail"><div class="empty">Select a mount to see its filesystem.</div></div>
      <div class="card table-wrap">
        <h2>Filesystem types</h2><p class="sub">From /proc/filesystems. A nodev type mounts without a source.</p>
        <table id="fstypes-table"><thead><tr><th>Type</th><th>Source</th><th class="num">Mounts</th></tr></thead><tbody></tbody></table>
      </div>
    </div>
  </section>

  <section id="tab-devices" hidden>
    <div class="card" id="devices-note" hidden><div class="empty">There is no /sys/bus here, so the kernel lists no PCI or USB devices.</div></div>
    <div class="grid split" style="margin-top:0">
      <div class="grid" style="gap:16px">
        <div class="card table-wrap">
          <h2>PCI</h2><p class="sub" id="pci-sub">From /sys/bus/pci/devices</p>
          <table id="pci-table"><thead><tr><th>Slot</th><th>Class</th><th>Vendor</th><th>Device</th><th>ID</th></tr></thead><tbody></tbody></table>
        </div>
        <div class="card table-wrap">
          <h2>USB</h2><p class="sub" id="usb-sub">From /sys/bus/usb/devices</p>
          <table id="usb-table"><thead><tr><th>Port</th><th class="num">Bus</th><th class="num">Address</th><th>Class</th><th>Vendor</th><th>Product</th><th>ID</th></tr></thead><tbody></tbody></table>
        </div>
      </div>
      <div class="card detail device-detail" id="device-detail"><div class="empty">Select a PCI or USB device to see its details.</div></div>
    </div>
    <div class="card table-wrap" style="margin-top:16px">
      <h2>Device nodes</h2><p class="sub" id="nodes-sub">Character and block devices in /dev</p>
      <table id="nodes-table"><thead><tr><th>Path</th><th>Type</th><th class="num">Major</th><th class="num">Minor</th><th>Mode</th><th>Owner</th></tr></thead><tbody></tbody></table>
    </div>
  </section>

  <section id="tab-files" hidden>
    <form class="toolbar" id="files-form" autocomplete="off">
      <button class="btn" type="button" id="files-up" title="Parent directory">Up</button>
      <input type="text" id="files-path" class="grow" value="/" aria-label="Path to open">
      <button class="btn primary" type="submit">Open</button>
      <button class="btn" type="button" id="files-refresh">Refresh</button>
    </form>
    <div class="grid files">
      <div class="card table-wrap">
        <div class="crumbs" id="files-crumbs"></div>
        <table id="files-table"><thead><tr><th>Name</th><th class="num">Size</th><th>Mode</th><th>Owner</th><th>Modified</th></tr></thead><tbody></tbody></table>
        <p class="sub" id="files-note" style="margin:10px 0 0"></p>
      </div>
      <div class="card preview">
        <div class="preview-head">
          <span class="mono grow" id="file-title">No file selected</span>
          <select id="file-bytes" aria-label="Amount of the file to show">
            <option value="16384">First 16 KB</option>
            <option value="65536" selected>First 64 KB</option>
            <option value="262144">First 256 KB</option>
            <option value="1048576">First 1 MB</option>
          </select>
          <a class="btn" id="file-download" download hidden>Download</a>
        </div>
        <p class="sub" id="file-meta">Read only. Only regular files are opened.</p>
        <pre class="out" id="file-view"></pre>
      </div>
    </div>
  </section>

  <section id="tab-console" hidden>
    <div class="card">
      <h2>Console</h2><p class="sub">Each command runs with /bin/sh -c, without a terminal; stdin is empty.</p>
      <div class="out" id="console-out" style="background:var(--surface-2);border-radius:8px;padding:12px;overflow:auto"></div>
      <form class="console-form" id="console-form" autocomplete="off">
        <input type="text" id="cwd" value="/" aria-label="Working directory" title="Working directory">
        <input type="text" id="command" placeholder="Command, then Enter. Up and Down walk the history." aria-label="Command">
        <input type="number" id="timeout" value="30" min="1" max="600" aria-label="Timeout in seconds" title="Timeout in seconds">
        <div class="buttons"><button class="btn primary" type="submit" id="run">Run</button><button class="btn" type="button" id="clear">Clear</button></div>
      </form>
    </div>
  </section>
</main>
<div id="toast" role="status" aria-live="polite"></div>
<script>
"use strict";

const SVG_NS = "http://www.w3.org/2000/svg";
const SIGNALS = ["TERM", "INT", "HUP", "KILL", "STOP", "CONT", "USR1", "USR2"];
const STATES = {R: ["running", "good"], S: ["sleeping", "neutral"], D: ["waiting", "serious"], T: ["stopped", "warning"], Z: ["zombie", "critical"], X: ["dead", "critical"]};
const PROC_COLUMNS = [
  {key: "pid", label: "PID", num: true},
  {key: "user", label: "User"},
  {key: "name", label: "Name"},
  {key: "state", label: "State"},
  {key: "cpu", label: "CPU %", num: true},
  {key: "rss", label: "RSS", num: true},
  {key: "threads", label: "Threads", num: true},
  {key: "fds", label: "FDs", num: true},
  {key: "cmdline", label: "Command"},
];

const state = {
  tab: "overview",
  overview: null,
  procs: [],
  sort: {key: "cpu", dir: -1},
  selected: null,
  mounts: null,
  mountKey: null,
  devices: null,
  deviceKey: null,
  logs: [],
  logPath: null,
  filesPath: storageGet("pymonitor-files-path") || "/",
  filesLoaded: false,
  preview: null,
  history: [],
  historyPos: 0,
  busy: {},
  ticks: 0,
};

/**
 * @brief Finds one element.
 * @param {string} sel CSS selector.
 * @returns {Element} The first match.
 */
function $(sel) { return document.querySelector(sel); }

/**
 * @brief Reads a value from localStorage.
 * @param {string} key Storage key.
 * @returns {string|null} The value, or null when storage is unavailable.
 */
function storageGet(key) {
  try { return localStorage.getItem(key); } catch (e) { return null; }
}

/**
 * @brief Writes a value to localStorage, ignoring unavailable storage.
 * @param {string} key Storage key.
 * @param {string} value Value to store.
 */
function storageSet(key, value) {
  try { localStorage.setItem(key, value); } catch (e) { }
}

/**
 * @brief Creates an HTML element.
 * @param {string} tag Tag name.
 * @param {Object} props Attributes; "class", "text" and on* handlers are special.
 * @param {...(Node|string)} children Child nodes or text.
 * @returns {HTMLElement} The element.
 */
function el(tag, props, ...children) {
  const node = document.createElement(tag);
  for (const [k, v] of Object.entries(props || {})) {
    if (v === undefined || v === null || v === false) continue;
    if (k === "class") node.className = v;
    else if (k === "text") node.textContent = v;
    else if (k.startsWith("on")) node.addEventListener(k.slice(2), v);
    else node.setAttribute(k, v === true ? "" : v);
  }
  for (const c of children) if (c !== null && c !== undefined) node.append(c instanceof Node ? c : String(c));
  return node;
}

/**
 * @brief Creates an SVG element.
 * @param {string} tag Tag name.
 * @param {Object} attrs Attributes.
 * @returns {SVGElement} The element.
 */
function svg(tag, attrs) {
  const node = document.createElementNS(SVG_NS, tag);
  for (const [k, v] of Object.entries(attrs || {})) node.setAttribute(k, v);
  return node;
}

/**
 * @brief Calls the dashboard API.
 * @param {string} path API path with query.
 * @param {Object} [body] JSON body; its presence makes the request a POST.
 * @returns {Promise<Object>} The decoded response.
 */
async function api(path, body) {
  const opts = {headers: {"X-Dashboard": "1"}};
  if (body !== undefined) {
    opts.method = "POST";
    opts.headers["Content-Type"] = "application/json";
    opts.body = JSON.stringify(body);
  }
  const res = await fetch(path, opts);
  const data = await res.json().catch(() => ({}));
  if (!res.ok) throw new Error(data.error || `${res.status} ${res.statusText}`);
  return data;
}

/**
 * @brief Shows a short message at the bottom of the page.
 * @param {string} text Message.
 * @param {boolean} [error] Whether it reports a failure.
 */
function toast(text, error) {
  const t = $("#toast");
  t.textContent = text;
  t.className = error ? "error" : "";
  t.style.display = "block";
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => { t.style.display = "none"; }, error ? 6000 : 3500);
}

/**
 * @brief Formats a size given in kB.
 * @param {number} kb Size in kB.
 * @returns {string} The size with a binary unit.
 */
function fmtKB(kb) {
  if (kb === null || kb === undefined) return "—";
  if (kb >= 1048576) return `${(kb / 1048576).toFixed(1)} GiB`;
  if (kb >= 1024) return `${(kb / 1024).toFixed(kb >= 102400 ? 0 : 1)} MiB`;
  return `${kb} kB`;
}

/**
 * @brief Formats a size given in bytes.
 * @param {number} bytes Size in bytes.
 * @returns {string} The size with a binary unit.
 */
function fmtBytes(bytes) {
  if (bytes < 1024) return `${bytes} B`;
  return fmtKB(Math.round(bytes / 1024));
}

/**
 * @brief Formats a duration.
 * @param {number} s Seconds.
 * @returns {string} The two largest units, such as "3h 05m".
 */
function fmtDuration(s) {
  s = Math.floor(s);
  const d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60), sec = s % 60;
  if (d) return `${d}d ${h}h`;
  if (h) return `${h}h ${String(m).padStart(2, "0")}m`;
  if (m) return `${m}m ${String(sec).padStart(2, "0")}s`;
  return `${sec}s`;
}

/**
 * @brief Formats a memory amount given in MiB for chart axes.
 * @param {number} mib Amount in MiB.
 * @returns {string} The amount in MiB or GiB.
 */
function fmtMiB(mib) {
  if (!mib) return "0";
  return mib >= 1024 ? `${(mib / 1024).toFixed(mib % 1024 ? 1 : 0)} GiB` : `${Math.round(mib)} MiB`;
}

/**
 * @brief Picks round tick values from zero up to a maximum.
 * @param {number} max Largest value on the axis.
 * @param {number} count Rough number of intervals wanted.
 * @returns {number[]} The ticks.
 */
function niceTicks(max, count) {
  if (!(max > 0)) return [0];
  const raw = max / count, mag = Math.pow(10, Math.floor(Math.log10(raw)));
  const step = [1, 2, 2.5, 5, 10].map(f => f * mag).find(s => s >= raw);
  const ticks = [];
  for (let v = 0; v <= max + 1e-9; v += step) ticks.push(v);
  return ticks;
}

/**
 * @brief Picks memory axis ticks on whole GiB once the total reaches one.
 * @param {number} totalMiB Physical memory in MiB.
 * @returns {number[]} The ticks, in MiB.
 */
function memoryTicks(totalMiB) {
  return totalMiB >= 1024 ? niceTicks(totalMiB / 1024, 4).map(v => v * 1024) : niceTicks(totalMiB, 4);
}

/**
 * @brief Draws a single-series line chart with a crosshair tooltip.
 * @param {HTMLElement} host Chart container; it keeps the hover position between redraws.
 * @param {Array<{t:number,v:number}>} points Samples, oldest first.
 * @param {{max:number, ticks:number[], format:function(number):string, span:number, label:string}} opts Axis and formatting options.
 */
function lineChart(host, points, opts) {
  const W = Math.max(host.clientWidth, 280), H = 180, L = 56, R = 60, T = 10, B = 22;
  const iw = W - L - R, ih = H - T - B;
  const end = points.length ? points[points.length - 1].t : Date.now() / 1000;
  const x = t => L + (1 - (end - t) / opts.span) * iw;
  const y = v => T + (1 - Math.min(v, opts.max) / opts.max) * ih;
  const root = svg("svg", {viewBox: `0 0 ${W} ${H}`, height: H, role: "img", tabindex: "0", "aria-label": opts.label});
  const axis = svg("g", {class: "axis"});
  for (const v of opts.ticks) {
    axis.append(svg("line", {x1: L, x2: W - R, y1: y(v), y2: y(v), stroke: v === 0 ? "var(--axis)" : "var(--grid)", "stroke-width": 1, "shape-rendering": "crispEdges"}));
    const label = svg("text", {x: L - 8, y: y(v) + 4, "text-anchor": "end"});
    label.textContent = opts.format(v);
    axis.append(label);
  }
  for (const ago of [opts.span, opts.span / 2, 0]) {
    const label = svg("text", {x: x(end - ago), y: H - 4, "text-anchor": ago === opts.span ? "start" : ago === 0 ? "end" : "middle"});
    label.textContent = ago === 0 ? "now" : ago >= 60 ? `${Math.round(ago / 60)} min ago` : `${Math.round(ago)} s ago`;
    axis.append(label);
  }
  root.append(axis);
  const visible = points.filter(p => end - p.t <= opts.span);
  if (visible.length > 1) {
    const line = visible.map((p, i) => `${i ? "L" : "M"}${x(p.t).toFixed(1)},${y(p.v).toFixed(1)}`).join("");
    root.append(svg("path", {d: `${line}L${x(end).toFixed(1)},${y(0)}L${x(visible[0].t).toFixed(1)},${y(0)}Z`, fill: "var(--accent)", "fill-opacity": 0.1, stroke: "none"}));
    root.append(svg("path", {d: line, fill: "none", stroke: "var(--accent)", "stroke-width": 2, "stroke-linejoin": "round", "stroke-linecap": "round"}));
  }
  if (visible.length) {
    const last = visible[visible.length - 1];
    root.append(svg("circle", {cx: x(last.t), cy: y(last.v), r: 4, fill: "var(--accent)", stroke: "var(--surface)", "stroke-width": 2}));
    const label = svg("text", {x: x(last.t) + 10, y: y(last.v) + 4, class: "endlabel"});
    label.textContent = opts.format(last.v);
    root.append(label);
  }
  const cross = svg("g", {style: "display:none"});
  const crossLine = svg("line", {y1: T, y2: T + ih, stroke: "var(--muted)", "stroke-width": 1, "shape-rendering": "crispEdges"});
  const crossDot = svg("circle", {r: 4, fill: "var(--accent)", stroke: "var(--surface)", "stroke-width": 2});
  cross.append(crossLine, crossDot);
  root.append(cross);
  const tip = el("div", {class: "tip"});
  host.replaceChildren(root, tip);

  const show = fromEnd => {
    if (!visible.length || fromEnd === null || fromEnd === undefined) {
      cross.style.display = "none";
      tip.style.display = "none";
      return;
    }
    const i = Math.max(0, visible.length - 1 - Math.min(fromEnd, visible.length - 1));
    const p = visible[i], px = x(p.t), py = y(p.v);
    host.hover = visible.length - 1 - i;
    crossLine.setAttribute("x1", px);
    crossLine.setAttribute("x2", px);
    crossDot.setAttribute("cx", px);
    crossDot.setAttribute("cy", py);
    cross.style.display = "";
    const ago = Math.round(end - p.t);
    tip.replaceChildren(el("strong", {text: opts.format(p.v)}), el("i", {class: "key"}), el("span", {text: ago ? `${ago} s ago` : "now"}));
    tip.style.display = "block";
    const scale = root.getBoundingClientRect().width / W || 1;
    const left = px * scale;
    tip.style.left = `${left + tip.offsetWidth + 12 > host.clientWidth ? left - tip.offsetWidth - 12 : left + 12}px`;
  };
  root.addEventListener("pointermove", ev => {
    const rect = root.getBoundingClientRect();
    const px = (ev.clientX - rect.left) * W / rect.width;
    let best = 0;
    for (let i = 1; i < visible.length; i++) if (Math.abs(x(visible[i].t) - px) < Math.abs(x(visible[best].t) - px)) best = i;
    show(visible.length - 1 - best);
  });
  root.addEventListener("pointerleave", () => { host.hover = null; show(null); });
  root.addEventListener("focus", () => show(host.hover ?? 0));
  root.addEventListener("blur", () => { host.hover = null; show(null); });
  root.addEventListener("keydown", ev => {
    if (ev.key !== "ArrowLeft" && ev.key !== "ArrowRight") return;
    ev.preventDefault();
    show(Math.max(0, (host.hover ?? 0) + (ev.key === "ArrowLeft" ? 1 : -1)));
  });
  show(host.hover);
}

/**
 * @brief Fills a meter and picks its severity.
 * @param {HTMLElement} meter The .meter element.
 * @param {number} ratio Filled share, from 0 to 1.
 */
function setMeter(meter, ratio) {
  ratio = Math.max(0, Math.min(1, ratio || 0));
  meter.className = "meter" + (ratio >= 0.9 ? " critical" : ratio >= 0.75 ? " warning" : "");
  meter.firstElementChild.style.width = `${(ratio * 100).toFixed(1)}%`;
}

/**
 * @brief Shows whether the dashboard is reaching the guest.
 * @param {boolean} ok Whether the last overview request succeeded.
 * @param {string} [why] Error text for the tooltip.
 */
function setOnline(ok, why) {
  $("#conn-dot").className = "dot " + (ok ? "good" : "critical");
  $("#conn-text").textContent = ok ? "live" : "offline";
  $("#conn-text").parentElement.title = ok ? "" : (why || "");
}

/**
 * @brief Fetches the overview, which also drives the header and connection state.
 */
async function loadOverview() {
  try {
    const o = await api("/api/overview");
    state.overview = o;
    setOnline(true);
    $("#host").textContent = o.hostname;
    $("#release").textContent = `${o.uname.sysname} ${o.uname.release} · ${o.uname.machine}`;
    document.title = `${o.hostname} · aplus dashboard`;
    if (state.tab === "overview") renderOverview();
  } catch (e) {
    setOnline(false, e.message);
  }
}

/**
 * @brief Renders the overview tab from the last overview payload.
 */
function renderOverview() {
  const o = state.overview;
  if (!o) return;
  const last = o.history.length ? o.history[o.history.length - 1] : null;
  $("#t-cpu").textContent = last ? `${Math.round(last.cpu)}%` : "—";
  $("#t-cpu-note").textContent = `${o.cpu_count} CPU${o.cpu_count === 1 ? "" : "s"}`;
  $("#t-mem").textContent = fmtKB(o.memory.used);
  $("#t-mem-note").textContent = `of ${fmtKB(o.memory.total)}, ${fmtKB(o.memory.slab)} kernel heap`;
  setMeter($("#t-mem-meter"), o.memory.total ? o.memory.used / o.memory.total : 0);
  $("#t-procs").textContent = String(o.processes);
  $("#t-procs-note").textContent = o.last_pid !== null ? `last pid ${o.last_pid}` : "";
  $("#t-uptime").textContent = fmtDuration(o.uptime);
  $("#t-uptime-note").textContent = o.boot_time ? `since ${new Date(o.boot_time * 1000).toLocaleString()}` : "";
  const disk = o.disk;
  $("#t-disk").textContent = disk ? fmtBytes(disk.used) : "—";
  $("#t-disk-note").textContent = disk ? `of ${fmtBytes(disk.size)}, ${fmtBytes(disk.avail)} available` : "statfs failed on /";
  setMeter($("#t-disk-meter"), usedShare(disk));

  const span = o.interval * o.history_len;
  $("#cpu-sub").textContent = `All CPUs, percent busy, last ${fmtDuration(span)}`;
  $("#mem-sub").textContent = `Physical memory, last ${fmtDuration(span)}`;
  lineChart($("#cpu-chart"), o.history.map(p => ({t: p.t, v: p.cpu})), {max: 100, ticks: [0, 25, 50, 75, 100], format: v => `${Math.round(v)}%`, span, label: "CPU usage"});
  const totalMiB = o.memory.total / 1024;
  lineChart($("#mem-chart"), o.history.map(p => ({t: p.t, v: p.mem / 1024})), {max: totalMiB, ticks: memoryTicks(totalMiB), format: fmtMiB, span, label: "Memory in use"});

  $("#cpus").replaceChildren(...(o.cpus.length ? o.cpus : [last ? last.cpu : 0]).map((pct, i) => {
    const meter = el("div", {class: "meter"}, el("span"));
    setMeter(meter, pct / 100);
    return el("div", {class: "cpu-row"}, el("span", {text: `CPU ${i}`}), meter, el("span", {class: "pct", text: `${Math.round(pct)}%`}));
  }));

  const facts = [
    ["Hostname", o.hostname],
    ["Kernel", o.version || `${o.uname.sysname} ${o.uname.release}`],
    ["Build", o.uname.version],
    ["Machine", o.uname.machine],
    ["CPU", `${o.cpu_model || "unknown"} × ${o.cpu_count}`],
    ["Address", o.address || "no network"],
    ["Command line", o.cmdline || "—", true],
    ["Root filesystem", disk && disk.type ? `${disk.type} on ${disk.source}` : "unknown"],
    ["Filesystems", o.filesystems.map(f => f.name).join(", ")],
    ["Dashboard", `pid ${o.dashboard.pid} as ${o.dashboard.user}, Python ${o.dashboard.python}`],
  ];
  $("#facts").replaceChildren(...facts.flatMap(([k, v, mono]) => [el("dt", {text: k}), el("dd", {class: mono ? "mono" : null, text: v})]));
}

/**
 * @brief Fetches the process table and, if one is selected, its details.
 */
async function loadProcesses() {
  const data = await api("/api/processes");
  state.procs = data.processes;
  renderProcesses();
  if (state.selected !== null) await loadDetail();
}

/**
 * @brief Builds the process table header with its sort controls.
 */
function renderProcessHeader() {
  const row = $("#proc-table thead tr");
  row.replaceChildren(...PROC_COLUMNS.map(c => el("th", {
    class: "sortable" + (c.num ? " num" : ""),
    "data-key": c.key,
    tabindex: "0",
    text: c.label,
    onclick: () => sortBy(c.key),
    onkeydown: ev => { if (ev.key === "Enter" || ev.key === " ") { ev.preventDefault(); sortBy(c.key); } },
  })));
}

/**
 * @brief Sorts the process table by a column, flipping the direction on a second click.
 * @param {string} key Column key.
 */
function sortBy(key) {
  const numeric = PROC_COLUMNS.find(c => c.key === key).num;
  state.sort = state.sort.key === key ? {key, dir: -state.sort.dir} : {key, dir: numeric ? -1 : 1};
  renderProcesses();
}

/**
 * @brief Renders the filtered, sorted process table.
 */
function renderProcesses() {
  for (const th of document.querySelectorAll("#proc-table th")) {
    if (th.dataset.key === state.sort.key) th.setAttribute("aria-sort", state.sort.dir > 0 ? "ascending" : "descending");
    else th.removeAttribute("aria-sort");
  }
  const q = $("#proc-filter").value.trim().toLowerCase();
  const {key, dir} = state.sort;
  const rows = state.procs
    .filter(p => !q || [p.pid, p.name, p.cmdline, p.user].some(v => String(v).toLowerCase().includes(q)))
    .sort((a, b) => {
      const va = a[key] ?? -1, vb = b[key] ?? -1;
      return (typeof va === "number" && typeof vb === "number" ? va - vb : String(va).localeCompare(String(vb))) * dir || a.pid - b.pid;
    });
  $("#proc-count").textContent = q ? `${rows.length} of ${state.procs.length} processes` : `${state.procs.length} processes`;
  const body = $("#proc-table tbody");
  if (!rows.length) {
    body.replaceChildren(el("tr", {}, el("td", {colspan: PROC_COLUMNS.length, class: "empty", text: state.procs.length ? "No process matches the filter." : "Loading…"})));
    return;
  }
  body.replaceChildren(...rows.map(p => {
    const [word, tone] = STATES[p.state] || ["unknown", "neutral"];
    return el("tr", {"aria-selected": String(p.pid === state.selected), onclick: () => selectProcess(p.pid)},
      el("td", {class: "num", text: p.pid}),
      el("td", {text: p.user}),
      el("td", {text: p.name}),
      el("td", {title: word}, el("span", {class: "state"}, el("span", {class: `dot ${tone}`}), `${p.state} ${word}`)),
      el("td", {class: "num", text: p.cpu.toFixed(1)}),
      el("td", {class: "num", text: fmtKB(p.rss)}),
      el("td", {class: "num", text: p.threads}),
      el("td", {class: "num", text: p.fds ?? "—"}),
      el("td", {class: "cmd", title: p.cmdline, text: p.cmdline}));
  }));
}

/**
 * @brief Selects a process and loads its details.
 * @param {number} pid Process id.
 */
function selectProcess(pid) {
  state.selected = pid;
  renderProcesses();
  loadDetail();
}

/**
 * @brief Fetches and renders the selected process's details.
 */
async function loadDetail() {
  const pid = state.selected;
  try {
    const d = await api(`/api/process?pid=${pid}`);
    if (state.selected === pid) renderDetail(d);
  } catch (e) {
    if (state.selected === pid) $("#proc-detail").replaceChildren(el("h3", {text: `PID ${pid}`}), el("div", {class: "empty", text: e.message}));
  }
}

/**
 * @brief Renders the detail panel for one process.
 * @param {Object} d Payload from /api/process.
 */
function renderDetail(d) {
  const status = Object.fromEntries(d.status);
  const name = status.Name || String(d.pid);
  const signals = el("div", {class: "buttons"}, ...SIGNALS.map(sig => el("button", {
    class: "btn small" + (sig === "KILL" || sig === "TERM" ? " danger" : ""),
    text: `SIG${sig}`,
    disabled: d.pid <= 1,
    onclick: () => sendSignal(d.pid, sig, name),
  })));
  const facts = el("dl", {class: "facts"}, ...d.status.concat(d.io).flatMap(([k, v]) => [el("dt", {text: k}), el("dd", {class: "mono", text: v})]));
  const files = d.files.length
    ? el("table", {}, el("tbody", {}, ...d.files.map(f => el("tr", {style: "cursor:default"}, el("td", {class: "num", text: f.fd}), el("td", {class: "mono", text: f.target})))))
    : el("div", {class: "empty", text: "No open files."});
  $("#proc-detail").replaceChildren(
    el("h3", {text: `PID ${d.pid} · ${name}`}),
    el("div", {class: "argv mono", text: d.argv.length ? d.argv.join(" ") : "(no arguments)"}),
    d.pid <= 1 ? el("p", {class: "sub", text: "Init takes signals only through the power controls on the overview."}) : signals,
    el("h4", {text: "Status"}), facts,
    el("h4", {text: `Open files (${d.files.length})`}), el("div", {class: "files"}, files));
}

/**
 * @brief Sends a signal to a process after confirming.
 * @param {number} pid Process id.
 * @param {string} sig Signal name without the SIG prefix.
 * @param {string} name Process name, for the confirmation.
 */
async function sendSignal(pid, sig, name) {
  if (!window.confirm(`Send SIG${sig} to ${pid} (${name})?`)) return;
  try {
    const r = await api("/api/signal", {pid, signal: sig});
    toast(r.output.trim());
    setTimeout(() => runTask("processes", loadProcesses), 400);
  } catch (e) {
    toast(e.message, true);
  }
}

/**
 * @brief Fetches and renders the services table.
 */
async function loadServices() {
  const data = await api("/api/services");
  const body = $("#svc-table tbody");
  if (!data.services.length) {
    body.replaceChildren(el("tr", {}, el("td", {colspan: 4, class: "empty", text: "No scripts in /etc/init.d."})));
    return;
  }
  body.replaceChildren(...data.services.map(s => {
    const status = s.running
      ? el("span", {class: "state"}, el("span", {class: "dot good"}), `running (pid ${s.pid})`)
      : el("span", {class: "state"}, el("span", {class: `dot ${s.stale ? "warning" : "neutral"}`}), s.stale ? "stopped, stale pid file" : "stopped");
    return el("tr", {style: "cursor:default"},
      el("td", {}, el("span", {class: "mono", text: s.name}), s.enabled ? null : el("span", {class: "tag", style: "margin-left:8px", text: "not executable"})),
      el("td", {text: s.description}),
      el("td", {}, status),
      el("td", {}, el("div", {class: "buttons"},
        el("button", {class: "btn small", text: "Start", disabled: s.running, onclick: () => serviceAction(s.name, "start")}),
        el("button", {class: "btn small", text: "Stop", disabled: !s.running && !s.stale, onclick: () => serviceAction(s.name, "stop")}),
        el("button", {class: "btn small", text: "Restart", onclick: () => serviceAction(s.name, "restart")}),
        el("button", {class: "btn small", text: "Log", disabled: !s.log, onclick: () => openLog(s.log)}))));
  }));
}

/**
 * @brief Runs a service action after confirming stops and restarts.
 * @param {string} name Script name.
 * @param {string} action "start", "stop" or "restart".
 */
async function serviceAction(name, action) {
  if (action !== "start" && !window.confirm(`${action[0].toUpperCase() + action.slice(1)} ${name}?`)) return;
  $("#svc-output").textContent = `${action} ${name}…`;
  try {
    const r = await api("/api/service", {name, action});
    $("#svc-output").textContent = r.output || "(no output)";
    toast(`${name}: ${action} done`);
  } catch (e) {
    $("#svc-output").textContent = e.message;
    toast(e.message, true);
  }
  setTimeout(() => runTask("services", loadServices), 800);
}

/**
 * @brief Computes the share of a filesystem in use as df does, against the space users can still take.
 * @param {Object} usage Usage from the API, or null.
 * @returns {number} The share from 0 to 1, or 0 when the filesystem counts no space.
 */
function usedShare(usage) {
  return usage && usage.used + usage.avail > 0 ? usage.used / (usage.used + usage.avail) : 0;
}

/**
 * @brief Names a mount uniquely, since one path can be mounted on more than once.
 * @param {Object} m Mount from /api/mounts.
 * @param {number} i Its position in the mount table.
 * @returns {string} The key.
 */
function mountKey(m, i) { return `${i}:${m.target}`; }

/**
 * @brief Fetches the mount table and renders the mounts tab.
 */
async function loadMounts() {
  state.mounts = await api("/api/mounts");
  renderMounts();
}

/**
 * @brief Renders the mount table, the selected mount and the filesystem types.
 */
function renderMounts() {
  const {mounts, filesystems} = state.mounts;
  $("#mounts-sub").textContent = `${mounts.length} mount${mounts.length === 1 ? "" : "s"} from /proc/mounts, in mount order, with usage from statfs`;
  $("#mounts-table tbody").replaceChildren(...(mounts.length
    ? mounts.map((m, i) => mountRow(m, mountKey(m, i)))
    : [el("tr", {}, el("td", {colspan: 8, class: "empty", text: "No mounts in /proc/mounts."}))]));
  const selected = mounts.find((m, i) => mountKey(m, i) === state.mountKey);
  if (selected) renderMountDetail(selected);
  else {
    state.mountKey = null;
    $("#mount-detail").replaceChildren(el("div", {class: "empty", text: "Select a mount to see its filesystem."}));
  }
  const counts = {};
  for (const m of mounts) counts[m.type] = (counts[m.type] || 0) + 1;
  $("#fstypes-table tbody").replaceChildren(...(filesystems.length
    ? filesystems.map(f => el("tr", {style: "cursor:default"},
        el("td", {class: "mono", text: f.name}),
        el("td", {text: f.nodev ? "none (nodev)" : "required"}),
        el("td", {class: "num", text: counts[f.name] || 0})))
    : [el("tr", {}, el("td", {colspan: 3, class: "empty", text: "/proc/filesystems is empty."}))]));
}

/**
 * @brief Builds one row of the mount table.
 * @param {Object} m Mount from /api/mounts.
 * @param {string} key The mount's key.
 * @returns {HTMLElement} The row.
 */
function mountRow(m, key) {
  const u = m.usage && m.usage.blocks > 0 ? m.usage : null;
  const share = usedShare(u);
  const meter = el("div", {class: "meter"}, el("span"));
  setMeter(meter, share);
  const note = m.covered ? "covered" : m.error ? "statfs failed" : null;
  const select = () => selectMount(key);
  return el("tr", {
    class: m.covered ? "covered" : null,
    tabindex: "0",
    "aria-selected": String(key === state.mountKey),
    onclick: select,
    onkeydown: ev => { if (ev.key === "Enter") select(); },
  },
    el("td", {class: "mono", title: m.target}, m.target, note ? el("span", {class: "tag", style: "margin-left:8px", text: note}) : null),
    el("td", {class: "mono", title: m.source, text: m.source}),
    el("td", {text: m.type}),
    el("td", {class: "num", text: u ? fmtBytes(u.size) : "—"}),
    el("td", {class: "num", text: u ? fmtBytes(u.used) : "—"}),
    el("td", {class: "num", text: u ? fmtBytes(u.avail) : "—"}),
    el("td", {}, u ? el("div", {class: "usage"}, meter, el("span", {class: "pct", text: `${Math.round(share * 100)}%`})) : "—"),
    el("td", {class: "num", text: m.usage && m.usage.inodes ? `${Math.round(100 * (m.usage.inodes - m.usage.inodes_free) / m.usage.inodes)}%` : "—"}));
}

/**
 * @brief Selects a mount and shows its filesystem.
 * @param {string} key The mount's key.
 */
function selectMount(key) {
  state.mountKey = key;
  renderMounts();
}

/**
 * @brief Renders the detail panel for one mount.
 * @param {Object} m Mount from /api/mounts.
 */
function renderMountDetail(m) {
  const u = m.usage;
  const facts = [
    ["Source", m.source],
    ["Mountpoint", m.target],
    ["Type", m.type],
    ["Options", m.options.join(",")],
    ["Access", m.readonly ? "read only" : "read and write"],
  ];
  if (u && u.blocks) facts.push(
    ["Size", `${fmtBytes(u.size)} in ${u.blocks} blocks`],
    ["Used", `${fmtBytes(u.used)}, ${Math.round(usedShare(u) * 100)}%`],
    ["Available", u.free > u.avail ? `${fmtBytes(u.avail)}, plus ${fmtBytes(u.free - u.avail)} reserved` : fmtBytes(u.avail)]);
  else if (u) facts.push(["Size", "not counted"]);
  if (u) facts.push(
    ["Block size", u.fragment_size !== u.block_size ? `${fmtBytes(u.block_size)}, fragments of ${fmtBytes(u.fragment_size)}` : fmtBytes(u.block_size)],
    ["Inodes", u.inodes ? `${u.inodes - u.inodes_free} used of ${u.inodes}, ${u.inodes_free} free` : "not counted"],
    ["Longest name", `${u.namemax} bytes`]);
  const note = m.covered ? "A later mount on the same path hides this one, so statfs reaches that one instead." : m.error ? `statfs failed: ${m.error}` : null;
  $("#mount-detail").replaceChildren(...[
    el("h3", {text: m.target}),
    el("p", {class: "sub", text: `${m.type} from ${m.source}`}),
    el("div", {class: "buttons"}, el("button", {class: "btn small", text: "Browse", disabled: m.covered, onclick: () => browse(m.target)})),
    note ? el("p", {class: "sub", style: "margin:12px 0 0", text: note}) : null,
    el("h4", {text: "Filesystem"}),
    el("dl", {class: "facts"}, ...facts.flatMap(([k, v]) => [el("dt", {text: k}), el("dd", {class: "mono", text: v})])),
  ].filter(Boolean));
}

async function loadDevices() {
  state.devices = await api("/api/devices");
  renderDevices();
}

function plural(n, word) { return `${n} ${word}${n === 1 ? "" : "s"}`; }

function hex2(n) { return n.toString(16).padStart(2, "0"); }

function named(name, id) { return name ? `${name} (${id})` : id || "—"; }

function emptyRow(colspan, text) {
  return el("tr", {style: "cursor:default"}, el("td", {colspan, class: "empty", text}));
}

function deviceRow(key, cells, cls) {
  const select = () => { state.deviceKey = key; renderDevices(); };
  return el("tr", {
    class: cls,
    tabindex: "0",
    "aria-selected": String(key === state.deviceKey),
    onclick: select,
    onkeydown: ev => { if (ev.key === "Enter") select(); },
  }, ...cells);
}

function renderDevices() {
  const {sysfs, pci, usb, nodes} = state.devices;
  $("#devices-note").hidden = sysfs;
  $("#pci-sub").textContent = `${plural(pci.length, "function")} from /sys/bus/pci/devices`;
  $("#pci-table tbody").replaceChildren(...(pci.length
    ? pci.map(d => deviceRow(`pci:${d.slot}`, [
        el("td", {class: "mono", text: d.slot}),
        el("td", {text: d.class_name}),
        el("td", {text: d.vendor_name || d.vendor}),
        el("td", {text: d.device_name || d.device}),
        el("td", {class: "mono", text: d.vendor ? `${d.vendor}:${d.device}` : "—"})]))
    : [emptyRow(5, "No PCI devices in /sys/bus/pci/devices.")]));
  const interfaces = usb.reduce((n, d) => n + d.interfaces.length, 0);
  $("#usb-sub").textContent = `${plural(usb.length, "device")} and ${plural(interfaces, "interface")} from /sys/bus/usb/devices`;
  $("#usb-table tbody").replaceChildren(...(usb.length
    ? usb.flatMap(d => [
        deviceRow(`usb:${d.name}`, [
          el("td", {class: "mono", text: d.name}),
          el("td", {class: "num", text: d.bus}),
          el("td", {class: "num", text: d.address}),
          el("td", {text: d.class_name}),
          el("td", {text: d.vendor_name || d.vendor}),
          el("td", {text: d.product_name || d.product}),
          el("td", {class: "mono", text: d.vendor ? `${d.vendor}:${d.product}` : "—"})]),
        ...d.interfaces.map(i => deviceRow(`usb:${i.name}`, [
          el("td", {class: "mono", text: `interface ${i.number}`}),
          el("td"),
          el("td"),
          el("td", {text: i.class_name}),
          el("td", {colspan: 3, class: "mono", text: `${hex2(i.class)}/${hex2(i.subclass)}/${hex2(i.protocol)}${i.driver ? ` · ${i.driver}` : ""}`})], "iface")),
      ])
    : [emptyRow(7, "No USB devices in /sys/bus/usb/devices.")]));
  $("#nodes-sub").textContent = `${plural(nodes.length, "character and block device")} in /dev`;
  $("#nodes-table tbody").replaceChildren(...(nodes.length
    ? nodes.map(n => el("tr", {},
        el("td", {class: "mono", text: n.path}),
        el("td", {text: n.kind}),
        el("td", {class: "num", text: n.major}),
        el("td", {class: "num", text: n.minor}),
        el("td", {class: "mono", text: n.mode}),
        el("td", {text: `${n.owner}:${n.group}`})))
    : [emptyRow(6, "No device nodes in /dev.")]));
  renderDeviceDetail();
}

function findDevice(key) {
  const {pci, usb} = state.devices;
  for (const d of pci) if (`pci:${d.slot}` === key) return {kind: "pci", d};
  for (const d of usb) {
    if (`usb:${d.name}` === key) return {kind: "usb", d};
    for (const i of d.interfaces) if (`usb:${i.name}` === key) return {kind: "interface", d: i, parent: d};
  }
  return null;
}

function renderDeviceDetail() {
  const found = state.deviceKey ? findDevice(state.deviceKey) : null;
  if (!found) {
    state.deviceKey = null;
    $("#device-detail").replaceChildren(el("div", {class: "empty", text: "Select a PCI or USB device to see its details."}));
    return;
  }
  const {kind, d, parent} = found;
  let title, sub, facts, path;
  if (kind === "pci") {
    title = d.device_name || `Device ${d.device}`;
    sub = `${d.class_name} · ${d.slot}`;
    path = `/sys/bus/pci/devices/${d.slot}`;
    facts = [["Slot", d.slot], ["Vendor", named(d.vendor_name, d.vendor)], ["Device", named(d.device_name, d.device)], ["Subsystem", d.subsystem || "—"], ["Class", `${d.class_name} (${d.class})`]];
  } else if (kind === "usb") {
    title = d.product_name || `Product ${d.product}`;
    sub = `${d.class_name} · port ${d.name}`;
    path = `/sys/bus/usb/devices/${d.name}`;
    facts = [["Port", d.name], ["Bus", d.bus], ["Address", d.address], ["Vendor", named(d.vendor_name, d.vendor)], ["Product", named(d.product_name, d.product)], ["Release", d.release || "—"], ["Class", d.class_name], ["Interfaces", d.interfaces.length]];
  } else {
    title = `Interface ${d.number}`;
    sub = `${d.class_name} · ${parent.product_name || parent.name}`;
    path = `/sys/bus/usb/devices/${d.name}`;
    facts = [["Interface", d.name], ["Device", named(parent.product_name, parent.name)], ["Class", `${d.class_name} (${hex2(d.class)}/${hex2(d.subclass)}/${hex2(d.protocol)})`]];
  }
  if (d.driver) facts.push(["Driver", d.driver]);
  facts.push(["sysfs", path]);
  const uevent = Object.entries(d.uevent).map(([k, v]) => `${k}=${v}`).join("\n");
  $("#device-detail").replaceChildren(
    el("h3", {text: title}),
    el("p", {class: "sub", text: sub}),
    el("dl", {class: "facts"}, ...facts.flatMap(([k, v]) => [el("dt", {text: k}), el("dd", {class: "mono", text: v})])),
    el("h4", {text: "uevent"}),
    el("pre", {class: "out", text: uevent || "(empty)"}));
}

/**
 * @brief Opens a directory in the files tab.
 * @param {string} path Absolute directory path.
 */
function browse(path) {
  state.filesPath = path;
  state.filesLoaded = false;
  showTab("files");
}

/**
 * @brief Fetches and renders the list of log files.
 */
async function loadLogs() {
  const data = await api("/api/logs");
  state.logs = data.logs;
  if (!state.logPath && state.logs.length) state.logPath = state.logs[0].path;
  renderLogList();
}

/**
 * @brief Renders the log file list.
 */
function renderLogList() {
  const list = $("#loglist");
  if (!state.logs.length) {
    list.replaceChildren(el("div", {class: "empty", text: "No files under /var/log."}));
    return;
  }
  list.replaceChildren(...state.logs.map(l => el("button", {
    "aria-current": String(l.path === state.logPath),
    onclick: () => openLog(l.path),
  }, el("span", {class: "path", text: l.kernel ? "Kernel messages" : l.path.replace(/^\/var\/log\//, "")}), el("span", {class: "meta", text: logMeta(l)}))));
}

function logMeta(l) {
  if (!l.kernel) return `${fmtBytes(l.size)} · ${new Date(l.mtime * 1000).toLocaleString()}`;
  if (l.error && !l.size) return `${l.path} · ${l.error}`;
  return `${l.path} · ${fmtBytes(l.size)} kept · ${new Date(l.mtime * 1000).toLocaleString()}`;
}

/**
 * @brief Switches to the logs tab and shows one file.
 * @param {string} path Log path.
 */
function openLog(path) {
  state.logPath = path;
  $("#log-view").textContent = "";
  if (state.tab !== "logs") showTab("logs");
  else { renderLogList(); runTask("log", loadLog); }
}

/**
 * @brief Fetches the end of the selected log, keeping the view pinned to the bottom if it was.
 */
async function loadLog() {
  if (!state.logPath) return;
  const view = $("#log-view");
  const pinned = view.scrollHeight - view.scrollTop - view.clientHeight < 24;
  const r = await api(`/api/log?path=${encodeURIComponent(state.logPath)}&bytes=${$("#log-bytes").value}`);
  if (r.path !== state.logPath) return;
  $("#log-title").textContent = `${r.path} · ${fmtBytes(r.size)}${r.truncated ? " (showing the end)" : ""}`;
  if (view.textContent !== r.text) {
    view.textContent = r.text || "(empty)";
    if (pinned) view.scrollTop = view.scrollHeight;
  }
}

/**
 * @brief Joins a directory and an entry name.
 * @param {string} dir Absolute directory path.
 * @param {string} name Entry name.
 * @returns {string} The entry's path.
 */
function joinPath(dir, name) {
  return dir === "/" ? `/${name}` : `${dir}/${name}`;
}

/**
 * @brief Opens a path in the explorer: lists a directory, or previews the file it resolves to.
 * @param {string} path Absolute path; symlinks are followed by the server.
 */
async function openPath(path) {
  try {
    const r = await api(`/api/files?path=${encodeURIComponent(path)}`);
    if (r.kind === "dir") renderFiles(r);
    else if (r.kind === "file") previewFile(r.path);
    else toast(`${r.path} is a ${r.kind} file, and the explorer reads only regular files`, true);
  } catch (e) {
    toast(e.message, true);
    if (!state.filesLoaded && path !== "/") openPath("/");
  }
}

/**
 * @brief Renders a directory listing, its breadcrumbs and the path box.
 * @param {Object} r Listing from /api/files.
 */
function renderFiles(r) {
  state.filesPath = r.path;
  state.filesLoaded = true;
  storageSet("pymonitor-files-path", r.path);
  $("#files-path").value = r.path;
  $("#files-up").disabled = r.parent === null;
  const parts = r.path.split("/").filter(Boolean);
  const crumbs = [el("button", {type: "button", text: "/", onclick: () => openPath("/")})];
  parts.forEach((part, i) => {
    const target = "/" + parts.slice(0, i + 1).join("/");
    if (i) crumbs.push(el("span", {text: "/"}));
    crumbs.push(el("button", {type: "button", text: part, onclick: () => openPath(target)}));
  });
  $("#files-crumbs").replaceChildren(...crumbs);
  $("#files-note").textContent = `${r.entries.length} entries${r.truncated ? ", listing cut short" : ""}`;
  const body = $("#files-table tbody");
  if (!r.entries.length) {
    body.replaceChildren(el("tr", {}, el("td", {colspan: 5, class: "empty", text: "Empty directory."})));
    return;
  }
  body.replaceChildren(...r.entries.map(entry => {
    const path = joinPath(r.path, entry.name);
    const open = () => entry.kind === "dir" || entry.kind === "link" ? openPath(path) : entry.kind === "file" ? previewFile(path) : toast(`${path} is a ${entry.kind} file, and the explorer reads only regular files`, true);
    const name = entry.kind === "dir" ? el("span", {class: "dir", text: `${entry.name}/`})
      : entry.kind === "link" ? el("span", {}, entry.name, el("span", {class: "target", text: ` → ${entry.target}`}))
      : entry.kind === "file" ? el("span", {text: entry.name})
      : el("span", {}, entry.name, el("span", {class: "tag", style: "margin-left:8px", text: entry.kind}));
    return el("tr", {
      tabindex: "0",
      "aria-selected": String(path === state.preview),
      onclick: open,
      onkeydown: ev => { if (ev.key === "Enter") open(); },
    },
      el("td", {class: "name", title: path}, name),
      el("td", {class: "num", text: entry.kind === "file" && entry.size !== undefined ? fmtBytes(entry.size) : "—"}),
      el("td", {class: "mono", text: entry.mode || "—"}),
      el("td", {text: entry.user ? `${entry.user}:${entry.group}` : "—"}),
      el("td", {text: entry.mtime ? new Date(entry.mtime * 1000).toLocaleString() : "—"}));
  }));
}

/**
 * @brief Shows the start of a file in the preview pane.
 * @param {string} path Absolute file path.
 */
async function previewFile(path) {
  state.preview = path;
  for (const row of document.querySelectorAll("#files-table tbody tr")) row.setAttribute("aria-selected", String(row.firstChild && row.firstChild.title === path));
  $("#file-title").textContent = path;
  $("#file-view").textContent = "Loading…";
  try {
    const r = await api(`/api/file?path=${encodeURIComponent(path)}&bytes=${$("#file-bytes").value}`);
    if (state.preview !== path) return;
    const what = r.binary ? "binary, hex dump of the start" : r.truncated ? "showing the start" : "whole file";
    $("#file-title").textContent = r.path;
    $("#file-meta").textContent = `${fmtBytes(r.size || r.read)} · ${r.mode} · ${r.user}:${r.group} · ${new Date(r.mtime * 1000).toLocaleString()} · ${what}`;
    $("#file-view").textContent = r.text || "(empty)";
    $("#file-view").scrollTop = 0;
    const link = $("#file-download");
    link.href = `/api/download?path=${encodeURIComponent(r.path)}`;
    link.hidden = false;
  } catch (e) {
    if (state.preview !== path) return;
    $("#file-view").textContent = e.message;
    $("#file-download").hidden = true;
  }
}

/**
 * @brief Runs the command in the console form.
 */
async function runCommand() {
  const input = $("#command"), command = input.value.trim();
  if (!command) return;
  if (state.history[state.history.length - 1] !== command) state.history.push(command);
  state.historyPos = state.history.length;
  input.value = "";
  const cwd = $("#cwd").value.trim() || "/";
  const meta = el("div", {class: "meta"}, el("span", {class: "dot warning"}), "running…");
  const output = el("pre");
  const block = el("div", {class: "cmd-block"}, el("div", {class: "line"}, el("span", {text: `${cwd} $ `}), command), output, meta);
  const out = $("#console-out");
  out.append(block);
  out.scrollTop = out.scrollHeight;
  $("#run").disabled = true;
  try {
    const r = await api("/api/exec", {command, cwd, timeout: Number($("#timeout").value) || 30});
    output.textContent = r.output + (r.truncated ? "\n[output truncated]" : "");
    const ok = r.code === 0;
    const what = r.timed_out ? "killed after timeout" : r.code === null ? "failed to start" : r.code < 0 ? `killed by signal ${-r.code}` : `exit ${r.code}`;
    meta.replaceChildren(el("span", {class: `dot ${ok ? "good" : "critical"}`}), `${what} · ${r.elapsed.toFixed(2)} s`);
  } catch (e) {
    output.textContent = e.message;
    meta.replaceChildren(el("span", {class: "dot critical"}), "request failed");
  } finally {
    $("#run").disabled = false;
    out.scrollTop = out.scrollHeight;
    input.focus();
  }
}

/**
 * @brief Walks the console history with the arrow keys.
 * @param {KeyboardEvent} ev Key event from the command input.
 */
function walkHistory(ev) {
  if (ev.key !== "ArrowUp" && ev.key !== "ArrowDown") return;
  ev.preventDefault();
  state.historyPos = Math.max(0, Math.min(state.history.length, state.historyPos + (ev.key === "ArrowUp" ? -1 : 1)));
  ev.target.value = state.history[state.historyPos] || "";
}

/**
 * @brief Asks init to reboot, power off or halt after confirming.
 * @param {string} action "reboot", "poweroff" or "halt".
 */
async function power(action) {
  const words = {reboot: "Reboot", poweroff: "Power off", halt: "Halt"};
  if (!window.confirm(`${words[action]} ${state.overview ? state.overview.hostname : "the machine"} now? Every process gets SIGTERM.`)) return;
  try {
    const r = await api("/api/power", {action});
    toast(`${r.output.trim()}; the dashboard will disconnect.`);
  } catch (e) {
    toast(e.message, true);
  }
}

/**
 * @brief Runs a refresh unless the same one is still in flight, reporting failures in the header.
 * @param {string} name Task name.
 * @param {function(): Promise} fn Task body.
 */
async function runTask(name, fn) {
  if (state.busy[name]) return;
  state.busy[name] = true;
  try { await fn(); } catch (e) { setOnline(false, e.message); } finally { state.busy[name] = false; }
}

/**
 * @brief Switches tabs and refreshes the new one at once.
 * @param {string} name Tab name.
 */
function showTab(name) {
  state.tab = name;
  for (const b of document.querySelectorAll("nav button")) b.setAttribute("aria-selected", String(b.dataset.tab === name));
  for (const s of document.querySelectorAll("main > section")) s.hidden = s.id !== `tab-${name}`;
  storageSet("pymonitor-tab", name);
  if (name === "overview") renderOverview();
  if (name === "processes") runTask("processes", loadProcesses);
  if (name === "services") runTask("services", loadServices);
  if (name === "mounts") runTask("mounts", loadMounts);
  if (name === "devices") runTask("devices", loadDevices);
  if (name === "logs") runTask("logs", async () => { await loadLogs(); await loadLog(); });
  if (name === "files" && !state.filesLoaded) openPath(state.filesPath);
  if (name === "console") $("#command").focus();
}

/**
 * @brief Drives the periodic refreshes, once a second, while the page is visible.
 */
function tick() {
  const n = ++state.ticks;
  if (document.hidden) return;
  if (n % 2 === 0) runTask("overview", loadOverview);
  if (state.tab === "processes" && $("#proc-auto").checked && n % 3 === 0) runTask("processes", loadProcesses);
  if (state.tab === "services" && n % 5 === 0) runTask("services", loadServices);
  if (state.tab === "mounts" && n % 5 === 0) runTask("mounts", loadMounts);
  if (state.tab === "devices" && n % 5 === 0) runTask("devices", loadDevices);
  if (state.tab === "logs" && n % 10 === 0) runTask("logs", loadLogs);
  if (state.tab === "logs" && $("#log-follow").checked && n % 2 === 0) runTask("log", loadLog);
}

for (const b of document.querySelectorAll("nav button")) b.addEventListener("click", () => showTab(b.dataset.tab));
for (const b of document.querySelectorAll("[data-power]")) b.addEventListener("click", () => power(b.dataset.power));
$("#btn-sync").addEventListener("click", async () => {
  try { toast((await api("/api/sync", {})).output.trim()); } catch (e) { toast(e.message, true); }
});
$("#proc-filter").addEventListener("input", renderProcesses);
$("#log-bytes").addEventListener("change", () => runTask("log", loadLog));
$("#files-form").addEventListener("submit", ev => { ev.preventDefault(); openPath($("#files-path").value.trim() || "/"); });
$("#files-up").addEventListener("click", () => openPath(state.filesPath.replace(/\/[^/]*$/, "") || "/"));
$("#files-refresh").addEventListener("click", () => openPath(state.filesPath));
$("#file-bytes").addEventListener("change", () => { if (state.preview) previewFile(state.preview); });
$("#console-form").addEventListener("submit", ev => { ev.preventDefault(); runCommand(); });
$("#command").addEventListener("keydown", walkHistory);
$("#clear").addEventListener("click", () => $("#console-out").replaceChildren());
window.addEventListener("resize", () => { if (state.tab === "overview") renderOverview(); });
renderProcessHeader();
showTab(["overview", "processes", "services", "logs", "mounts", "devices", "files", "console"].includes(storageGet("pymonitor-tab")) ? storageGet("pymonitor-tab") : "overview");
loadOverview();
setInterval(tick, 1000);
</script>
</body>
</html>
""".encode("utf-8")


## @brief Turns SIGTERM into a clean exit from serve_forever.
#  @param signum Signal number.
#  @param frame Interrupted frame.
def handle_sigterm(signum, frame):
    raise KeyboardInterrupt


## @brief Parses the command line, starts the sampler and serves until interrupted.
#  @return The process exit status.
def main():
    parser = argparse.ArgumentParser(description="Web dashboard to monitor and control aplus.")
    parser.add_argument("--bind", default="0.0.0.0", help="address to listen on (default: 0.0.0.0)")
    parser.add_argument("--port", type=int, default=8080, help="port to listen on (default: 8080)")
    parser.add_argument("--interval", type=float, default=1.0, help="seconds between CPU and memory samples (default: 1)")
    parser.add_argument("--verbose", action="store_true", help="log every request")
    args = parser.parse_args()

    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO, format="%(asctime)s %(name)s[%(process)d]: %(levelname)s: %(message)s", datefmt="%Y-%m-%dT%H:%M:%S%z")

    sampler = Sampler(clamp(args.interval, 0.2, 60.0))
    sampler.start()
    kmsg = KernelLog()
    kmsg.start()
    try:
        server = DashboardServer((args.bind, args.port), sampler, kmsg)
    except OSError as e:
        log.error("cannot listen on %s:%d: %s", args.bind, args.port, e)
        return 1
    signal.signal(signal.SIGTERM, handle_sigterm)

    log.info("serving on http://%s:%d/", args.bind, args.port)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        log.info("shutting down")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
