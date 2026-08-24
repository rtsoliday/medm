#!/usr/bin/env python3
"""Run IOC-backed QtEDM integration checks with structured state assertions."""

import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path
from typing import Callable, List, Match, Optional, Set
from urllib.parse import parse_qs, urlparse


CHANNEL_PATTERN = re.compile(
    r'((?:chan(?:[ABCD])?|channel(?:[ABCD])?|variable|setpoint|readback|'
    r'readbackPv|readbackChannel|dataPv|countPvName|xdata|ydata|trigger|'
    r'erase)=")([^"]*)(")',
    re.IGNORECASE)
CHILD_DISPLAY_PATTERN = re.compile(
    r'((?:^|\n)\s*display=")([^"]+\.adl)(")', re.IGNORECASE)


class CaseFailure(RuntimeError):
  """Raised when an IOC-backed integration case fails."""


def native_child_path(path: Path) -> str:
  """Return a path suitable for a native child launched from Cygwin."""
  if sys.platform.startswith("cygwin"):
    result = subprocess.run(
        ["cygpath", "-w", str(path)],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
        check=True,
    )
    return result.stdout.strip()
  return str(path)


def prefix_channel_name(channel: str, prefix: str) -> str:
  stripped = channel.strip()
  if not stripped or "://" in stripped or stripped.startswith(prefix):
    return channel
  return f"{prefix}{stripped}"


def display_channel_name(channel: str, prefix: str, provider: str) -> str:
  stripped = channel.strip()
  if provider != "pva":
    return prefix_channel_name(channel, prefix)
  if not stripped:
    return channel
  if stripped.lower().startswith("pva://"):
    pv = stripped[6:]
    if pv.startswith(prefix):
      return channel
    return f"pva://{prefix}{pv}"
  if "://" in stripped:
    return channel
  if stripped.startswith(prefix):
    return f"pva://{stripped}"
  return f"pva://{prefix}{stripped}"


def rewrite_display_with_prefix(display_path: Path, prefix: str,
    output_dir: Path, provider: str = "ca",
    visited: Optional[Set[Path]] = None) -> Path:
  if visited is None:
    visited = set()
  resolved_display = display_path.resolve()
  output_path = output_dir / display_path.name
  if resolved_display in visited:
    return output_path
  visited.add(resolved_display)

  text = display_path.read_text(encoding="utf-8")

  def replace(match: Match[str]) -> str:
    channel = match.group(2)
    if (re.search(r'variable="$', match.group(1), re.IGNORECASE)
        and re.fullmatch(r'[A-L]', channel.strip(), re.IGNORECASE)):
      return match.group(0)
    return (
        f'{match.group(1)}'
        f'{display_channel_name(channel, prefix, provider)}'
        f'{match.group(3)}')

  rewritten = CHANNEL_PATTERN.sub(replace, text)
  output_path.write_text(rewritten, encoding="utf-8")

  for match in CHILD_DISPLAY_PATTERN.finditer(text):
    child_name = match.group(2).strip()
    child_path = Path(child_name)
    if child_path.is_absolute():
      continue
    source_child = (display_path.parent / child_path).resolve()
    if source_child.is_file() and source_child != display_path.resolve():
      rewrite_display_with_prefix(
          source_child, prefix, output_dir, provider, visited)
  return output_path


def run_cavput(cavput_bin: Path, pv: str, value: str) -> subprocess.CompletedProcess:
  return subprocess.run(
      [str(cavput_bin), f"-list={pv}={value}"],
      check=False,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      universal_newlines=True,
  )


def run_caget(caget_bin: Path, pv: str) -> subprocess.CompletedProcess:
  return subprocess.run(
      [str(caget_bin), "-t", pv],
      check=False,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      universal_newlines=True,
  )


def run_pvput(pvput_bin: Path, pv: str, value: str) -> subprocess.CompletedProcess:
  return subprocess.run(
      [str(pvput_bin), "-w", "5", pv, value],
      check=False,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      universal_newlines=True,
  )


def wait_for_pva_ready(process: subprocess.Popen, pvget_bin: Path, pv: str,
    timeout_seconds: float) -> None:
  deadline = time.monotonic() + timeout_seconds
  last_output = ""
  while time.monotonic() < deadline:
    if process.poll() is not None:
      raise CaseFailure("softIocPVA exited before its test PV became available")
    result = subprocess.run(
        [str(pvget_bin), "-q", "-w", "1", pv],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        universal_newlines=True,
    )
    if result.returncode == 0:
      return
    last_output = (result.stderr or result.stdout).strip()
    time.sleep(0.2)
  raise CaseFailure(
      f"Timed out waiting for PVA test PV {pv}: {last_output}")


class ArchiveFixtureHandler(BaseHTTPRequestHandler):
  """Serve a bounded Archiver Appliance-compatible JSON response."""

  def do_GET(self) -> None:
    parsed = urlparse(self.path)
    if not parsed.path.endswith("/data/getData.json"):
      self.send_error(404)
      return
    query = parse_qs(parsed.query)
    pv = query.get("pv", [""])[0]
    sample_count = int(getattr(self.server, "sample_count", 5))
    now = int(time.time())
    data = [
        {
            "secs": now - sample_count + index,
            "nanos": index * 1000,
            "val": 10.0 + index,
            "status": 0,
            "severity": 0,
        }
        for index in range(sample_count)
    ]
    payload = json.dumps([
        {"meta": {"name": pv}, "data": data}
    ], separators=(",", ":")).encode("utf-8")
    self.send_response(200)
    self.send_header("Content-Type", "application/json")
    self.send_header("Content-Length", str(len(payload)))
    self.end_headers()
    self.wfile.write(payload)

  def log_message(self, format_string: str, *args) -> None:
    del format_string, args


class ArchiveFixtureServer:
  def __init__(self, sample_count: int):
    self.server = ThreadingHTTPServer(
        ("127.0.0.1", 0), ArchiveFixtureHandler)
    self.server.sample_count = sample_count
    self.thread = threading.Thread(
        target=self.server.serve_forever,
        name="qtedm-archive-fixture",
        daemon=True)

  def start(self) -> str:
    self.thread.start()
    port = self.server.server_address[1]
    return f"http://127.0.0.1:{port}/retrieval"

  def close(self) -> None:
    self.server.shutdown()
    self.server.server_close()
    self.thread.join(timeout=5)


def wait_for_ioc_ready(process: subprocess.Popen, ready_file: Path,
    timeout_seconds: float) -> None:
  deadline = time.monotonic() + timeout_seconds

  while time.monotonic() < deadline:
    if ready_file.exists():
      return
    time.sleep(0.1)
    if ready_file.exists():
      return

    if process.poll() is not None:
      raise CaseFailure(
          "run_local_ioc.sh exited before finishing PV initialization")

  raise CaseFailure(
      f"Timed out waiting for run_local_ioc.sh readiness file {ready_file}")


def wait_for_file(path: Path, process: subprocess.Popen,
    timeout_seconds: float) -> None:
  deadline = time.monotonic() + timeout_seconds
  while time.monotonic() < deadline:
    if path.exists():
      return
    if process.poll() is not None:
      break
    time.sleep(0.1)
  raise CaseFailure(f"Timed out waiting for {path.name}")


def terminate_process(process: Optional[subprocess.Popen]) -> None:
  if process is None or process.poll() is not None:
    return
  process.terminate()
  try:
    process.wait(timeout=5)
  except subprocess.TimeoutExpired:
    process.kill()
    process.wait(timeout=5)


def load_cases(cases_path: Path, selected_names: Set[str]) -> List[dict]:
  cases = json.loads(cases_path.read_text(encoding="utf-8"))
  if not selected_names:
    return cases
  selected = [case for case in cases if case.get("name") in selected_names]
  missing = sorted(selected_names - {case.get("name") for case in selected})
  if missing:
    raise CaseFailure(f"Unknown IOC test case(s): {', '.join(missing)}")
  return selected


def flatten_widgets(state_data: dict) -> List[dict]:
  widgets: List[dict] = []
  for display in state_data.get("displays", []):
    widgets.extend(display.get("widgets", []))
  return widgets


def normalize_selector(value, prefix: str, provider: str):
  if isinstance(value, dict):
    normalized = {}
    for key, item in value.items():
      if key == "channel" and isinstance(item, str):
        normalized[key] = display_channel_name(item, prefix, provider)
      else:
        normalized[key] = normalize_selector(item, prefix, provider)
    return normalized
  if isinstance(value, list):
    return [normalize_selector(item, prefix, provider) for item in value]
  return value


def object_contains(actual, expected) -> bool:
  if isinstance(expected, dict):
    if not isinstance(actual, dict):
      return False
    for key, value in expected.items():
      if key not in actual or not object_contains(actual[key], value):
        return False
    return True
  if isinstance(expected, list):
    return actual == expected
  return actual == expected


def matching_widgets(state_data: dict, selector: dict, prefix: str,
    provider: str) -> List[dict]:
  normalized_selector = normalize_selector(selector, prefix, provider)
  expected_type = normalized_selector.get("type", "<unknown>")
  matches = [
      widget for widget in flatten_widgets(state_data)
      if object_contains(widget, normalized_selector)
  ]
  if matches:
    return matches

  available = sorted({
      f"{widget.get('type')}:{widget.get('channel')}:{widget.get('geometry')}"
      for widget in flatten_widgets(state_data)
      if widget.get("type") == expected_type
  })
  raise CaseFailure(
      f"No widgets matched selector {normalized_selector!r}. "
      f"Available {expected_type} widgets: "
      f"{', '.join(available[:10])}")


def compare_expected(actual, expected, path: str, failures: List[str]) -> None:
  if isinstance(expected, dict):
    if not isinstance(actual, dict):
      failures.append(f"{path} expected object {expected!r}, got {actual!r}")
      return
    for key, value in expected.items():
      if key not in actual:
        failures.append(f"{path}.{key} missing, expected {value!r}")
        continue
      compare_expected(actual[key], value, f"{path}.{key}", failures)
    return

  if isinstance(expected, list):
    if actual != expected:
      failures.append(f"{path} expected {expected!r}, got {actual!r}")
    return

  if actual != expected:
    failures.append(f"{path} expected {expected!r}, got {actual!r}")


def assert_expectations(case_name: str, expect_source: dict,
    widgets: List[dict]) -> None:
  expect = dict(expect_source)
  numeric_expected = expect.pop("numeric_value", None)
  numeric_tolerance = float(expect.pop("numeric_tolerance", 0.0))

  failures: List[str] = []
  for widget in widgets:
    for key, expected in expect.items():
      actual = widget.get(key)
      compare_expected(
          actual, expected,
          f"{widget.get('type')}:{widget.get('channel')}:{key}", failures)
    if numeric_expected is not None:
      actual_value = widget.get("numeric_value")
      if actual_value is None:
        failures.append(
            f"{widget.get('type')}:{widget.get('channel')} missing numeric_value")
      elif abs(float(actual_value) - float(numeric_expected)) > numeric_tolerance:
        failures.append(
            f"{widget.get('type')}:{widget.get('channel')} expected "
            f"numeric_value={numeric_expected}, got {actual_value}")

  if failures:
    joined = "\n".join(failures)
    raise CaseFailure(f"Case {case_name} failed:\n{joined}")


def assert_case_state(case: dict, state_data: dict, prefix: str,
    provider: str) -> None:
  assertions = case.get("assertions")
  if assertions is None:
    widgets = matching_widgets(
        state_data, case["selector"], prefix, provider)
    assert_expectations(case["name"], case.get("expect", {}), widgets)
    return
  if not isinstance(assertions, list) or not assertions:
    raise CaseFailure(
        f"Case {case['name']} assertions must be a non-empty list")
  for index, assertion in enumerate(assertions):
    if not isinstance(assertion, dict) or "selector" not in assertion:
      raise CaseFailure(
          f"Case {case['name']} assertion {index} requires a selector")
    widgets = matching_widgets(
        state_data, assertion["selector"], prefix, provider)
    if not assertion.get("allow_multiple", False) and len(widgets) != 1:
      raise CaseFailure(
          f"Case {case['name']} assertion {index} matched {len(widgets)} "
          "widgets; exactly one is required")
    assert_expectations(
        f"{case['name']} assertion {index}",
        assertion.get("expect", {}), widgets)


def verify_pv_values(case: dict, provider: str, prefix: str,
    caget_bin: Path, pvget_bin: Optional[Path]) -> None:
  number_pattern = re.compile(
      r"(?<![A-Za-z_])[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?")
  for verification in case.get("verify_pvs", []):
    pv = prefix_channel_name(verification["pv"], prefix)
    if provider == "pva":
      if pvget_bin is None:
        raise CaseFailure("PVA verification requested without pvget")
      result = subprocess.run(
          [str(pvget_bin), "-q", "-w", "5", pv],
          check=False,
          stdout=subprocess.PIPE,
          stderr=subprocess.PIPE,
          universal_newlines=True,
      )
    else:
      result = run_caget(caget_bin, pv)
    output = (result.stdout or result.stderr).strip()
    if result.returncode != 0:
      raise CaseFailure(f"Failed to read back {pv}: {output}")
    if "contains" in verification:
      expected_text = str(verification["contains"])
      if expected_text not in output:
        raise CaseFailure(
            f"PV {pv} expected output containing {expected_text!r}, "
            f"got {output!r}")
    if "numeric_value" in verification:
      expected_number = float(verification["numeric_value"])
      tolerance = float(verification.get("numeric_tolerance", 0.0))
      numbers = [float(item) for item in number_pattern.findall(output)]
      if not any(abs(item - expected_number) <= tolerance for item in numbers):
        raise CaseFailure(
            f"PV {pv} expected numeric value {expected_number} +/- "
            f"{tolerance}, got {output!r}")


def run_case(case: dict, repo_root: Path, qtedm_bin: Path, cavput_bin: Path,
    caget_bin: Path, pvget_bin: Optional[Path], pvput_bin: Optional[Path],
    prefix: str, temp_dir: Path,
    restart_provider: Optional[Callable[[], None]] = None) -> None:
  provider = str(case.get("provider", "ca")).lower()
  display_path = (repo_root / case["display"]).resolve()
  rewritten_display = rewrite_display_with_prefix(
      display_path, prefix, temp_dir, provider)
  ready_path = temp_dir / f"{case['name']}.ready"
  state_path = temp_dir / f"{case['name']}.json"
  actions_path = temp_dir / f"{case['name']}.actions.json"
  archive_server: Optional[ArchiveFixtureServer] = None
  process: Optional[subprocess.Popen] = None

  process_env = os.environ.copy()
  archive_fixture = case.get("archive_fixture")
  if archive_fixture:
    sample_count = int(archive_fixture.get("sample_count", 5))
    archive_server = ArchiveFixtureServer(sample_count)
    process_env["QTEDM_ARCHIVER_URL"] = archive_server.start()
  else:
    process_env.pop("QTEDM_ARCHIVER_URL", None)

  qtedm_args = [str(value) for value in case.get("qtedm_args", [])]
  command = [
      str(qtedm_bin),
      *qtedm_args,
      "-x",
      "-testReadyFile",
      native_child_path(ready_path),
      "-testDumpState",
      native_child_path(state_path),
      "-testExitAfterMs",
      str(case.get("exit_after_ms", 4500)),
      native_child_path(rewritten_display),
  ]
  actions = case.get("actions", [])
  if actions:
    normalized_actions = []
    for action in actions:
      normalized_action = dict(action)
      normalized_action["selector"] = normalize_selector(
          action.get("selector", {}), prefix, provider)
      normalized_actions.append(normalized_action)
    actions_path.write_text(
        json.dumps(normalized_actions, indent=2), encoding="utf-8")
    command[1:1] = ["-testActions", native_child_path(actions_path)]

  for write in case.get("pre_writes", []):
    pv = prefix_channel_name(write["pv"], prefix)
    if provider == "pva":
      if pvput_bin is None:
        raise CaseFailure("PVA case requested without a pvput executable")
      result = run_pvput(pvput_bin, pv, str(write["value"]))
    else:
      result = run_cavput(cavput_bin, pv, str(write["value"]))
    if result.returncode != 0:
      raise CaseFailure(
          f"Failed pre-launch write {pv}={write['value']}: "
          f"{(result.stderr or result.stdout).strip()}")
  process = subprocess.Popen(
      command,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      universal_newlines=True,
      env=process_env,
  )
  try:
    wait_for_file(ready_path, process, timeout_seconds=15)
    post_ready_delay_ms = float(case.get("post_ready_delay_ms", 0))
    if post_ready_delay_ms > 0:
      time.sleep(post_ready_delay_ms / 1000.0)

    if case.get("restart_provider", False):
      if restart_provider is None:
        raise CaseFailure(
            f"Case {case['name']} requested an unavailable provider restart")
      restart_provider()

    for write in case.get("writes", []):
      delay_ms = float(write.get("delay_ms", 0))
      if delay_ms > 0:
        time.sleep(delay_ms / 1000.0)
      pv = prefix_channel_name(write["pv"], prefix)
      if provider == "pva":
        if pvput_bin is None:
          raise CaseFailure("PVA case requested without a pvput executable")
        result = run_pvput(pvput_bin, pv, str(write["value"]))
      else:
        result = run_cavput(cavput_bin, pv, str(write["value"]))
      if result.returncode != 0:
        raise CaseFailure(
            f"Failed to write {pv}={write['value']}: "
            f"{(result.stderr or result.stdout).strip()}")

    stdout, stderr = process.communicate(
        timeout=float(case.get("exit_after_ms", 4500)) / 1000.0 + 10.0)
  except subprocess.TimeoutExpired as exc:
    terminate_process(process)
    raise CaseFailure(f"Timed out waiting for qtedm in case {case['name']}") from exc
  finally:
    if process is not None and process.poll() is None:
      terminate_process(process)
    if archive_server is not None:
      archive_server.close()

  if process.returncode != 0:
    raise CaseFailure(
        f"qtedm exited with status {process.returncode} for case {case['name']}\n"
        f"STDOUT:\n{stdout}\nSTDERR:\n{stderr}")

  if not state_path.is_file():
    raise CaseFailure(f"Missing state dump for case {case['name']}: {state_path}")

  state_data = json.loads(state_path.read_text(encoding="utf-8"))
  try:
    assert_case_state(case, state_data, prefix, provider)
    verify_pv_values(case, provider, prefix, caget_bin, pvget_bin)
  except CaseFailure as exc:
    raise CaseFailure(
        f"{exc}\nQTEDM STDOUT:\n{stdout}\nQTEDM STDERR:\n{stderr}") from exc


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Run IOC-backed QtEDM integration checks.")
  parser.add_argument("--qtedm", required=True, help="Path to qtedm binary")
  parser.add_argument(
      "--run-local-ioc", required=True, help="Path to tests/run_local_ioc.sh")
  parser.add_argument("--cavput", required=True, help="Path to tests/cavput")
  parser.add_argument("--caget", required=True, help="Path to EPICS caget")
  parser.add_argument(
      "--soft-ioc-pva", help="Path to the EPICS softIocPVA executable")
  parser.add_argument("--pvget", help="Path to the EPICS pvget executable")
  parser.add_argument("--pvput", help="Path to the EPICS pvput executable")
  parser.add_argument(
      "--pva-database", help="Database loaded by softIocPVA")
  parser.add_argument("--cases", required=True, help="JSON case manifest")
  parser.add_argument(
      "--case", action="append", default=[],
      help="Specific case name to run (may be repeated)")
  args = parser.parse_args()

  qtedm_bin = Path(args.qtedm).expanduser().resolve()
  run_local_ioc = Path(args.run_local_ioc).expanduser().resolve()
  cavput_bin = Path(args.cavput).expanduser().resolve()
  caget_bin = Path(args.caget).expanduser().resolve()
  soft_ioc_pva = (
      Path(args.soft_ioc_pva).expanduser().resolve()
      if args.soft_ioc_pva else None)
  pvget_bin = (
      Path(args.pvget).expanduser().resolve() if args.pvget else None)
  pvput_bin = (
      Path(args.pvput).expanduser().resolve() if args.pvput else None)
  pva_database = (
      Path(args.pva_database).expanduser().resolve()
      if args.pva_database else None)
  cases_path = Path(args.cases).expanduser().resolve()
  repo_root = Path(__file__).resolve().parents[1]

  for path in (qtedm_bin, run_local_ioc, cavput_bin, caget_bin, cases_path):
    if not path.exists():
      raise CaseFailure(f"Required path not found: {path}")

  if not qtedm_bin.is_file():
    raise CaseFailure(f"QtEDM binary not found: {qtedm_bin}")
  if not os.access(run_local_ioc, os.X_OK):
    raise CaseFailure(f"run_local_ioc helper is not executable: {run_local_ioc}")
  if not os.access(cavput_bin, os.X_OK):
    raise CaseFailure(f"cavput helper is not executable: {cavput_bin}")

  selected_names = set(args.case)
  cases = load_cases(cases_path, selected_names)
  ca_cases = [
      case for case in cases
      if str(case.get("provider", "ca")).lower() != "pva"
  ]
  pva_cases = [
      case for case in cases
      if str(case.get("provider", "ca")).lower() == "pva"
  ]
  # softIocPVA also starts a CA server. On Linux, running it beside the CA IOC
  # on the same UDP port can divert unicast CA searches, so finish CA cases
  # before starting the PVA IOC.
  cases = ca_cases + pva_cases
  has_pva_cases = bool(pva_cases)
  if has_pva_cases:
    pva_paths = (soft_ioc_pva, pvget_bin, pvput_bin, pva_database)
    if any(path is None or not path.is_file() for path in pva_paths):
      raise CaseFailure(
          "PVA cases require --soft-ioc-pva, --pvget, --pvput, and "
          "--pva-database")
  case_runtime_ms = sum(
      int(case.get("exit_after_ms", 4500)) for case in cases)
  ioc_execution_seconds = max(
      240, (case_runtime_ms + 999) // 1000 + 180)

  prefix = f"qtedm_ioc_{int(time.time())}_{os.getpid()}:"
  pva_prefix = f"{prefix}pva:"
  ioc_process: Optional[subprocess.Popen] = None
  pva_process: Optional[subprocess.Popen] = None

  temp_dir = Path(tempfile.mkdtemp(prefix="qtedm-ioc."))
  keep_temp_dir = False
  ioc_log = temp_dir / "local_ioc.log"
  ioc_runner_log = temp_dir / "run_local_ioc.out"
  ioc_ready_file = temp_dir / "ioc.ready"
  pva_log = temp_dir / "soft_ioc_pva.log"
  ioc_command = [
      str(run_local_ioc),
      "--execution-time",
      str(ioc_execution_seconds),
      "--pv-prefix",
      prefix,
      "--log-file",
      str(ioc_log),
      "--ready-file",
      str(ioc_ready_file),
  ]
  runner_log_handle = None
  pva_log_handle = None

  def start_ca_fixture(initialization_profile: str = "full") -> None:
    nonlocal ioc_process, runner_log_handle
    if ioc_process is not None and ioc_process.poll() is None:
      return
    ioc_ready_file.unlink(missing_ok=True)
    runner_log_handle = ioc_runner_log.open("a", encoding="utf-8")
    command = list(ioc_command)
    if initialization_profile != "full":
      command.extend(["--init-profile", initialization_profile])
    ioc_process = subprocess.Popen(
        command,
        stdout=runner_log_handle,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    )
    wait_for_ioc_ready(ioc_process, ioc_ready_file, timeout_seconds=120)

  def restart_ca_fixture() -> None:
    nonlocal ioc_process, runner_log_handle
    terminate_process(ioc_process)
    ioc_process = None
    if runner_log_handle is not None:
      runner_log_handle.close()
      runner_log_handle = None
    start_ca_fixture("slider")

  def start_pva_fixture() -> None:
    nonlocal pva_process, pva_log_handle
    if pva_process is not None and pva_process.poll() is None:
      return
    pva_log_handle = pva_log.open("a", encoding="utf-8")
    pva_process = subprocess.Popen(
        [
            str(soft_ioc_pva),
            "-S",
            "-m",
            f"P={pva_prefix}",
            "-d",
            native_child_path(pva_database),
        ],
        # softIocPVA treats stdin EOF as a request to exit. Keep a private
        # pipe open for the complete PVA lifetime; terminate_process() still
        # provides bounded shutdown without an executable wrapper.
        stdin=subprocess.PIPE,
        stdout=pva_log_handle,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    )
    wait_for_pva_ready(
        pva_process, pvget_bin,
        f"{pva_prefix}sp:test:compact:setpoint",
        timeout_seconds=30)

  def restart_pva_fixture() -> None:
    nonlocal pva_process, pva_log_handle
    terminate_process(pva_process)
    pva_process = None
    if pva_log_handle is not None:
      pva_log_handle.close()
      pva_log_handle = None
    start_pva_fixture()

  try:
    start_ca_fixture()
    for case in cases:
      provider = str(case.get("provider", "ca")).lower()
      if provider == "pva" and pva_process is None:
        start_pva_fixture()
      if provider == "pva" and pva_process.poll() is not None:
        raise CaseFailure(
            "softIocPVA exited between PVA integration cases")
      case_prefix = pva_prefix if provider == "pva" else prefix
      run_case(
          case, repo_root, qtedm_bin, cavput_bin, caget_bin, pvget_bin,
          pvput_bin, case_prefix, temp_dir,
          restart_pva_fixture if provider == "pva" else restart_ca_fixture)
      print(f"PASS {case['name']}")
  except Exception as exc:
    keep_temp_dir = True
    terminate_process(ioc_process)
    terminate_process(pva_process)
    if runner_log_handle is not None:
      runner_log_handle.close()
      runner_log_handle = None
    if pva_log_handle is not None:
      pva_log_handle.close()
    if isinstance(exc, CaseFailure):
      message = str(exc)
    else:
      message = f"Unexpected IOC test failure: {exc}"
    log_hint = [
        f"Preserved IOC temp dir: {temp_dir}",
        f"IOC log: {ioc_log}" if ioc_log.exists() else "IOC log unavailable",
        f"IOC runner log: {ioc_runner_log}"
        if ioc_runner_log.exists() else "IOC runner log unavailable",
        f"PVA IOC log: {pva_log}"
        if pva_log.exists() else "PVA IOC log unavailable",
    ]
    sys.stderr.write(f"{message}\n" + "\n".join(log_hint) + "\n")
    return 1
  finally:
    terminate_process(ioc_process)
    terminate_process(pva_process)
    if runner_log_handle is not None:
      runner_log_handle.close()
    if pva_log_handle is not None:
      pva_log_handle.close()
    if not keep_temp_dir:
      shutil.rmtree(temp_dir, ignore_errors=True)

  return 0


if __name__ == "__main__":
  try:
    sys.exit(main())
  except CaseFailure as exc:
    sys.stderr.write(f"{exc}\n")
    sys.exit(1)
