# <license>
#
#     This file is part of the Sapphire Operating System.
#
#     Copyright (C) 2013-2022  Jeremy Billheimer
#
#
#     This program is free software: you can redistribute it and/or modify
#     it under the terms of the GNU General Public License as published by
#     the Free Software Foundation, either version 3 of the License, or
#     (at your option) any later version.
#
#     This program is distributed in the hope that it will be useful,
#     but WITHOUT ANY WARRANTY; without even the implied warranty of
#     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#     GNU General Public License for more details.
#
#     You should have received a copy of the GNU General Public License
#     along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# </license>

# Multi-simulator host tests: several chromatron sim processes running on
# one host must each get a distinct loopback IP (127.0.0.N), keep it across
# a simulated reboot, and stay reachable through the normal catbus client.
#
# Run:  pytest src/tests/sim -v --durations=0
# (the sim binary is built with build_sim.sh; SIM_ELF overrides its path)

import logging
import os
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path

import pytest

# repo root is three levels up from this file (src/tests/sim); make the catbus
# python package importable no matter where pytest is invoked from
REPO_ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO_ROOT / 'python' / 'chromatron'))

from catbus import Client
from catbus import CATBUS_MAIN_PORT
from catbus.messages import DiscoverMsg, AnnounceMsg, deserialize

pytestmark = pytest.mark.sim

SIM_ELF = os.environ.get('SIM_ELF') or str(REPO_ROOT / 'src' / 'chromatron' / 'main.elf')

# each test uses its own IP so tests stay independent of other sims the
# user may have running (auto-assign uses 127.0.0.2-32)
_IP = {'n': 20}

def next_ip():
    _IP['n'] += 1
    return '127.0.0.%d' % _IP['n']

# how long to wait for a sim to come up (boot is ~1s; 10s is generous)
SIM_READY_TIMEOUT = 10.0


def _discover_once(timeout):
    # one round of discovery on the loopback broadcast; returns a dict of
    # ip -> (origin_id, data_port) for local (127.x) responders only
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
    s.settimeout(timeout)

    msg = DiscoverMsg(flags=1)
    s.sendto(msg.pack(), ('127.255.255.255', CATBUS_MAIN_PORT))

    nodes = {}

    end = time.time() + timeout
    while time.time() < end:
        try:
            data, addr = s.recvfrom(1024)

        except socket.timeout:
            break

        try:
            m = deserialize(data)

        except Exception:
            continue

        if not isinstance(m, AnnounceMsg):
            continue

        if m.header.transaction_id != msg.header.transaction_id:
            continue

        if addr[0].startswith('127.'):
            nodes[addr[0]] = (m.header.origin_id, m.data_port)

    s.close()

    return nodes


def _wait_for_ip(ip, timeout=SIM_READY_TIMEOUT):
    # poll discovery until the given IP answers; returns its (origin_id, port)
    deadline = time.time() + timeout

    while time.time() < deadline:
        nodes = _discover_once(0.4)

        if ip in nodes:
            return nodes[ip]

        time.sleep(0.1)

    raise TimeoutError(f'sim at {ip} not discoverable after {timeout}s')


def _wait_for_exit(proc, timeout=5.0):
    try:
        proc.wait(timeout=timeout)

    except subprocess.TimeoutExpired:
        _kill_sim(proc)
        raise


def _kill_sim(proc):
    # sims are started in their own session; kill the group so a re-execed
    # (simulated reboot) child dies with it
    if proc.poll() is None:
        try:
            os.killpg(proc.pid, signal.SIGKILL)

        except (ProcessLookupError, PermissionError):
            proc.kill()

    proc.wait()


class Sim:
    def __init__(self, state_dir, ip=None, run_time=30):
        self.state_dir = state_dir
        self.ip = ip
        args = [SIM_ELF, str(run_time)]

        if ip:
            args += ['--ip', ip]

        self.log_file = open(state_dir / 'sim.log', 'wb')
        self.proc = subprocess.Popen(
            args,
            cwd=str(state_dir),
            stdout=self.log_file,
            stderr=subprocess.STDOUT,
            stdin=subprocess.DEVNULL,
            start_new_session=True,
        )

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()
        self.log_file.close()
        return False

    def wait_ready(self):
        assert self.ip, 'Sim.wait_ready needs an explicit --ip'
        self.origin_id, self.data_port = _wait_for_ip(self.ip)
        return self

    def close(self):
        _kill_sim(self.proc)


@pytest.fixture
def sim_factory(tmp_path_factory):
    sims = []

    def factory(ip=None, run_time=30):
        state_dir = tmp_path_factory.mktemp('sim_state')
        sim = Sim(state_dir, ip=ip or next_ip(), run_time=run_time)
        sims.append(sim)
        return sim

    yield factory

    for sim in sims:
        sim.close()


@pytest.fixture
def ss_table():
    # callable: the socket table must be read after the sim is up
    def get():
        try:
            return subprocess.check_output(
                ['ss', '-ulnp'], text=True, timeout=5).splitlines()

        except (OSError, subprocess.CalledProcessError, subprocess.TimeoutExpired):
            pytest.skip('ss not available')

    return get


if not os.path.exists(SIM_ELF):
    pytest.skip(f'sim binary not found: {SIM_ELF} (build with build_sim.sh)',
                allow_module_level=True)


def test_two_sims_get_distinct_ips(sim_factory):
    a = sim_factory().wait_ready()
    b = sim_factory().wait_ready()

    assert a.ip != b.ip
    assert a.origin_id != b.origin_id
    assert a.data_port == CATBUS_MAIN_PORT
    assert b.data_port == CATBUS_MAIN_PORT


def test_socket_table(sim_factory, ss_table):
    sim = sim_factory().wait_ready()
    pid = sim.proc.pid

    bound = [l for l in ss_table() if f'pid={pid}' in l]
    taps = [l for l in bound if '127.255.255.255' in l]

    # the per-sim data socket, bound to its own IP
    assert any(f'{sim.ip}:{CATBUS_MAIN_PORT}' in l for l in bound), \
        f'no socket bound to {sim.ip}:{CATBUS_MAIN_PORT} in:\n' + '\n'.join(bound)

    # the shared broadcast taps (how the sim hears discovery)
    assert any(f'127.255.255.255:{CATBUS_MAIN_PORT}' in l for l in taps), \
        f'no broadcast tap on port {CATBUS_MAIN_PORT} in:\n' + '\n'.join(taps)


def test_kv_roundtrip(sim_factory):
    sim = sim_factory().wait_ready()

    c = Client((sim.ip, sim.data_port))

    try:
        version = c.get_keys('firmware_version')['firmware_version']
        assert isinstance(version, str) and len(version) > 0

        cfg_version = c.get_keys('cfg_version')['cfg_version']
        assert isinstance(cfg_version, int)
    finally:
        c.close()


def test_three_sims_all_distinct(sim_factory):
    sims = [sim_factory().wait_ready() for _ in range(3)]

    ips = [s.ip for s in sims]
    origins = [s.origin_id for s in sims]

    assert len(set(ips)) == 3
    assert len(set(origins)) == 3


def test_explicit_ip(sim_factory):
    ip = '127.0.0.25'
    sim = sim_factory(ip=ip).wait_ready()

    assert sim.ip == ip


def test_ip_conflict_fails(sim_factory):
    ip = '127.0.0.26'
    sim_factory(ip=ip).wait_ready()

    # a second sim claiming the same IP must refuse to start
    clash = sim_factory(ip=ip)

    _wait_for_exit(clash.proc)
    assert clash.proc.returncode != 0

    # the first sim is still up and on its IP
    _wait_for_ip(ip, timeout=2.0)


def test_reboot_keeps_ip(sim_factory):
    sim = sim_factory().wait_ready()
    ip, origin = sim.ip, sim.origin_id

    # trigger a simulated reboot (re-exec) through the normal KV path
    c = Client((ip, sim.data_port))

    try:
        c.set_keys(reboot=1)
    finally:
        c.close()

    # wait for the sim to drop off the network, then come back
    deadline = time.time() + 10.0

    while time.time() < deadline:
        if ip not in _discover_once(0.4):
            break

        time.sleep(0.1)
    else:
        raise TimeoutError(f'sim at {ip} did not go down after reboot')

    new_origin, new_port = _wait_for_ip(ip, timeout=10.0)

    # the re-execed process must keep its IP and identity
    assert new_origin == origin
    assert new_port == CATBUS_MAIN_PORT
