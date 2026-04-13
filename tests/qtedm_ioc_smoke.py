#!/usr/bin/env python3
"""Run IOC-backed QtEDM integration checks with structured state assertions."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path


CHANNEL_PATTERN = re.compile(r'(chan=")([^"]*)(")')
class CaseFailure(RuntimeError):
  """Raised when an IOC-backed integration case fails."""


def prefix_channel_name(channel: str, prefix: str) -> str:
  stripped = channel.strip()
  if not stripped or "://" in stripped or stripped.startswith(prefix):
    return channel
  return f"{prefix}{stripped}"


def rewrite_display_with_prefix(display_path: Path, prefix: str,
    output_dir: Path) -> Path:
  text = display_path.read_text(encoding="utf-8")

  def replace(match: re.Match[str]) -> str:
    channel = match.group(2)
    return f'{match.group(1)}{prefix_channel_name(channel, prefix)}{match.group(3)}'

  rewritten = CHANNEL_PATTERN.sub(replace, text)
  output_path = output_dir / display_path.name
  output_path.write_text(rewritten, encoding="utf-8")
  return output_path


def run_cavput(cavput_bin: Path, pv: str, value: str) -> subprocess.CompletedProcess[str]:
  return subprocess.run(
      [str(cavput_bin), f"-list={pv}={value}"],
      check=False,
      capture_output=True,
      text=True,
  )


def wait_for_ioc_ready(process: subprocess.Popen[str], ready_file: Path,
    timeout_seconds: float) -> None:
  deadline = time.monotonic() + timeout_seconds

  while time.monotonic() < deadline:
    if ready_file.exists():
      return
    time.sleep(0.1)

    if process.poll() is not None:
      raise CaseFailure(
          "run_local_ioc.sh exited before finishing PV initialization")

  raise CaseFailure(
      f"Timed out waiting for run_local_ioc.sh readiness file {ready_file}")


def wait_for_file(path: Path, process: subprocess.Popen[str],
    timeout_seconds: float) -> None:
  deadline = time.monotonic() + timeout_seconds
  while time.monotonic() < deadline:
    if path.exists():
      return
    if process.poll() is not None:
      break
    time.sleep(0.1)
  raise CaseFailure(f"Timed out waiting for {path.name}")


def terminate_process(process: subprocess.Popen[str] | None) -> None:
  if process is None or process.poll() is not None:
    return
  process.terminate()
  try:
    process.wait(timeout=5)
  except subprocess.TimeoutExpired:
    process.kill()
    process.wait(timeout=5)


def load_cases(cases_path: Path, selected_names: set[str]) -> list[dict]:
  cases = json.loads(cases_path.read_text(encoding="utf-8"))
  if not selected_names:
    return cases
  selected = [case for case in cases if case.get("name") in selected_names]
  missing = sorted(selected_names - {case.get("name") for case in selected})
  if missing:
    raise CaseFailure(f"Unknown IOC test case(s): {', '.join(missing)}")
  return selected


def flatten_widgets(state_data: dict) -> list[dict]:
  widgets: list[dict] = []
  for display in state_data.get("displays", []):
    widgets.extend(display.get("widgets", []))
  return widgets


def normalize_selector(value, prefix: str):
  if isinstance(value, dict):
    normalized = {}
    for key, item in value.items():
      if key == "channel" and isinstance(item, str):
        normalized[key] = prefix_channel_name(item, prefix)
      else:
        normalized[key] = normalize_selector(item, prefix)
    return normalized
  if isinstance(value, list):
    return [normalize_selector(item, prefix) for item in value]
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


def matching_widgets(state_data: dict, selector: dict, prefix: str) -> list[dict]:
  normalized_selector = normalize_selector(selector, prefix)
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


def compare_expected(actual, expected, path: str, failures: list[str]) -> None:
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


def assert_expectations(case: dict, widgets: list[dict]) -> None:
  expect = dict(case.get("expect", {}))
  numeric_expected = expect.pop("numeric_value", None)
  numeric_tolerance = float(expect.pop("numeric_tolerance", 0.0))

  failures: list[str] = []
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
    raise CaseFailure(f"Case {case['name']} failed:\n{joined}")


def run_case(case: dict, repo_root: Path, qtedm_bin: Path, cavput_bin: Path,
    prefix: str, temp_dir: Path) -> None:
  display_path = (repo_root / case["display"]).resolve()
  rewritten_display = rewrite_display_with_prefix(display_path, prefix, temp_dir)
  ready_path = temp_dir / f"{case['name']}.ready"
  state_path = temp_dir / f"{case['name']}.json"

  command = [
      str(qtedm_bin),
      "-x",
      "-testReadyFile",
      str(ready_path),
      "-testDumpState",
      str(state_path),
      "-testExitAfterMs",
      str(case.get("exit_after_ms", 4500)),
      str(rewritten_display),
  ]
  process = subprocess.Popen(
      command,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      text=True,
  )
  try:
    wait_for_file(ready_path, process, timeout_seconds=15)
    time.sleep(float(case.get("post_ready_delay_ms", 1000)) / 1000.0)

    for write in case.get("writes", []):
      delay_ms = float(write.get("delay_ms", 0))
      if delay_ms > 0:
        time.sleep(delay_ms / 1000.0)
      pv = prefix_channel_name(write["pv"], prefix)
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
    if process.poll() is None:
      terminate_process(process)

  if process.returncode != 0:
    raise CaseFailure(
        f"qtedm exited with status {process.returncode} for case {case['name']}\n"
        f"STDOUT:\n{stdout}\nSTDERR:\n{stderr}")

  if not state_path.is_file():
    raise CaseFailure(f"Missing state dump for case {case['name']}: {state_path}")

  state_data = json.loads(state_path.read_text(encoding="utf-8"))
  widgets = matching_widgets(state_data, case["selector"], prefix)
  assert_expectations(case, widgets)


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Run IOC-backed QtEDM integration checks.")
  parser.add_argument("--qtedm", required=True, help="Path to qtedm binary")
  parser.add_argument(
      "--run-local-ioc", required=True, help="Path to tests/run_local_ioc.sh")
  parser.add_argument("--cavput", required=True, help="Path to tests/cavput")
  parser.add_argument("--cases", required=True, help="JSON case manifest")
  parser.add_argument(
      "--case", action="append", default=[],
      help="Specific case name to run (may be repeated)")
  args = parser.parse_args()

  qtedm_bin = Path(args.qtedm).expanduser().resolve()
  run_local_ioc = Path(args.run_local_ioc).expanduser().resolve()
  cavput_bin = Path(args.cavput).expanduser().resolve()
  cases_path = Path(args.cases).expanduser().resolve()
  repo_root = Path(__file__).resolve().parents[1]

  for path in (qtedm_bin, run_local_ioc, cavput_bin, cases_path):
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

  prefix = f"qtedm_ioc_{int(time.time())}_{os.getpid()}:"
  ioc_process: subprocess.Popen[str] | None = None

  temp_dir = Path(tempfile.mkdtemp(prefix="qtedm-ioc."))
  keep_temp_dir = False
  ioc_log = temp_dir / "local_ioc.log"
  ioc_runner_log = temp_dir / "run_local_ioc.out"
  ioc_ready_file = temp_dir / "ioc.ready"
  ioc_command = [
      str(run_local_ioc),
      "--execution-time",
      "240",
      "--pv-prefix",
      prefix,
      "--log-file",
      str(ioc_log),
      "--ready-file",
      str(ioc_ready_file),
  ]
  runner_log_handle = ioc_runner_log.open("w", encoding="utf-8")
  ioc_process = subprocess.Popen(
      ioc_command,
      stdout=runner_log_handle,
      stderr=subprocess.STDOUT,
      text=True,
  )

  try:
    wait_for_ioc_ready(ioc_process, ioc_ready_file, timeout_seconds=120)
    for case in cases:
      run_case(case, repo_root, qtedm_bin, cavput_bin, prefix, temp_dir)
      print(f"PASS {case['name']}")
  except Exception as exc:
    keep_temp_dir = True
    terminate_process(ioc_process)
    runner_log_handle.close()
    if isinstance(exc, CaseFailure):
      message = str(exc)
    else:
      message = f"Unexpected IOC test failure: {exc}"
    log_hint = [
        f"Preserved IOC temp dir: {temp_dir}",
        f"IOC log: {ioc_log}" if ioc_log.exists() else "IOC log unavailable",
        f"IOC runner log: {ioc_runner_log}"
        if ioc_runner_log.exists() else "IOC runner log unavailable",
    ]
    sys.stderr.write(f"{message}\n" + "\n".join(log_hint) + "\n")
    return 1
  finally:
    terminate_process(ioc_process)
    runner_log_handle.close()
    if not keep_temp_dir:
      shutil.rmtree(temp_dir, ignore_errors=True)

  return 0


if __name__ == "__main__":
  try:
    sys.exit(main())
  except CaseFailure as exc:
    sys.stderr.write(f"{exc}\n")
    sys.exit(1)
