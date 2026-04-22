#!/usr/bin/env python3
"""Run focused QtEDM visual regression tests against golden screenshots."""

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
from typing import List, Match, Optional, Set


CHANNEL_PATTERN = re.compile(
    r'((?:chan|channelA|channelB|channelC|channelD|variable)=")([^"]*)(")',
    re.IGNORECASE)


class CaseFailure(RuntimeError):
  """Raised when a visual regression case fails."""


def prefix_channel_name(channel: str, prefix: str) -> str:
  stripped = channel.strip()
  if not stripped or "://" in stripped or stripped.startswith(prefix):
    return channel
  return f"{prefix}{stripped}"


def rewrite_display_with_prefix(display_path: Path, prefix: str,
    output_dir: Path) -> Path:
  text = display_path.read_text(encoding="utf-8")

  def replace(match: Match[str]) -> str:
    channel = match.group(2)
    return f'{match.group(1)}{prefix_channel_name(channel, prefix)}{match.group(3)}'

  rewritten = CHANNEL_PATTERN.sub(replace, text)
  output_path = output_dir / display_path.name
  output_path.write_text(rewritten, encoding="utf-8")
  return output_path


def terminate_process(process: Optional[subprocess.Popen]) -> None:
  if process is None or process.poll() is not None:
    return
  process.terminate()
  try:
    process.wait(timeout=5)
  except subprocess.TimeoutExpired:
    process.kill()
    process.wait(timeout=5)


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


def load_cases(cases_path: Path, selected_names: Set[str]) -> List[dict]:
  cases = json.loads(cases_path.read_text(encoding="utf-8"))
  if not selected_names:
    return cases
  selected = [case for case in cases if case.get("name") in selected_names]
  missing = sorted(selected_names - {case.get("name") for case in selected})
  if missing:
    raise CaseFailure(f"Unknown visual test case(s): {', '.join(missing)}")
  return selected


def compare_images(compare_tool: Path, expected: Path, actual: Path, diff: Path,
    tolerance: dict, case_name: str) -> None:
  command = [
      str(compare_tool),
      "--expected",
      str(expected),
      "--actual",
      str(actual),
      "--diff",
      str(diff),
      "--max-different-pixels",
      str(int(tolerance.get("max_different_pixels", 0))),
      "--max-mean-absolute-delta",
      str(float(tolerance.get("max_mean_absolute_delta", 0.0))),
      "--max-channel-delta",
      str(int(tolerance.get("max_channel_delta", 0))),
  ]
  result = subprocess.run(
      command,
      check=False,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      universal_newlines=True,
  )
  if result.returncode == 0:
    return

  raise CaseFailure(
      f"Visual diff failed for {case_name}\n"
      f"Expected: {expected}\n"
      f"Actual: {actual}\n"
      f"Diff: {diff}\n"
      f"STDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}")


def run_case(case: dict, repo_root: Path, qtedm_bin: Path, compare_tool: Path,
    prefix: str, temp_dir: Path, update_goldens: bool) -> None:
  display_path = (repo_root / case["display"]).resolve()
  if case.get("use_ioc"):
    display_to_run = rewrite_display_with_prefix(display_path, prefix, temp_dir)
  else:
    display_to_run = display_path

  ready_path = temp_dir / f"{case['name']}.ready"
  actual_path = temp_dir / f"{case['name']}.png"
  diff_path = temp_dir / f"{case['name']}.diff.png"
  golden_path = (repo_root / case["golden"]).resolve()

  command = [
      str(qtedm_bin),
      "-x",
      "-testReadyFile",
      str(ready_path),
      "-testCaptureScreenshot",
      str(actual_path),
      "-testExitAfterMs",
      str(case.get("exit_after_ms", 1500)),
      str(display_to_run),
  ]
  process = subprocess.Popen(
      command,
      stdout=subprocess.PIPE,
      stderr=subprocess.PIPE,
      universal_newlines=True,
  )
  try:
    wait_for_file(ready_path, process, timeout_seconds=20)
    stdout, stderr = process.communicate(
        timeout=float(case.get("exit_after_ms", 1500)) / 1000.0 + 10.0)
  except subprocess.TimeoutExpired as exc:
    terminate_process(process)
    raise CaseFailure(
        f"Timed out waiting for qtedm in visual case {case['name']}") from exc
  finally:
    if process.poll() is None:
      terminate_process(process)

  if process.returncode != 0:
    raise CaseFailure(
        f"qtedm exited with status {process.returncode} for visual case "
        f"{case['name']}\nSTDOUT:\n{stdout}\nSTDERR:\n{stderr}")

  if not actual_path.is_file():
    raise CaseFailure(
        f"Missing screenshot for visual case {case['name']}: {actual_path}")

  golden_path.parent.mkdir(parents=True, exist_ok=True)
  if update_goldens:
    shutil.copyfile(actual_path, golden_path)
    print(f"UPDATED {case['name']}")
    return

  if not golden_path.is_file():
    raise CaseFailure(
        f"Missing golden image for visual case {case['name']}: {golden_path}")

  compare_images(compare_tool, golden_path, actual_path, diff_path,
      case.get("tolerance", {}), case["name"])
  print(f"PASS {case['name']}")


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Run focused QtEDM visual regression tests.")
  parser.add_argument("--qtedm", required=True, help="Path to qtedm binary")
  parser.add_argument(
      "--compare-tool", required=True, help="Path to qtedm image compare tool")
  parser.add_argument(
      "--run-local-ioc", required=True, help="Path to tests/run_local_ioc.sh")
  parser.add_argument("--cases", required=True, help="JSON case manifest")
  parser.add_argument(
      "--case", action="append", default=[],
      help="Specific case name to run (may be repeated)")
  parser.add_argument(
      "--update-goldens", action="store_true",
      help="Replace golden images with the newly captured screenshots")
  args = parser.parse_args()

  qtedm_bin = Path(args.qtedm).expanduser().resolve()
  compare_tool = Path(args.compare_tool).expanduser().resolve()
  run_local_ioc = Path(args.run_local_ioc).expanduser().resolve()
  cases_path = Path(args.cases).expanduser().resolve()
  repo_root = Path(__file__).resolve().parents[1]

  for path in (qtedm_bin, compare_tool, run_local_ioc, cases_path):
    if not path.exists():
      raise CaseFailure(f"Required path not found: {path}")

  if not qtedm_bin.is_file():
    raise CaseFailure(f"QtEDM binary not found: {qtedm_bin}")
  if not compare_tool.is_file():
    raise CaseFailure(f"Image compare tool not found: {compare_tool}")
  if not os.access(run_local_ioc, os.X_OK):
    raise CaseFailure(f"run_local_ioc helper is not executable: {run_local_ioc}")

  selected_names = set(args.case)
  cases = load_cases(cases_path, selected_names)
  needs_ioc = any(case.get("use_ioc") for case in cases)

  # Visual goldens can render channel labels, so keep the IOC prefix stable.
  prefix = os.environ.get("QTEDM_VISUAL_PV_PREFIX", "qtedm_visual:")
  temp_dir = Path(tempfile.mkdtemp(prefix="qtedm-visual."))
  keep_temp_dir = False
  ioc_process: Optional[subprocess.Popen] = None
  ioc_log = temp_dir / "local_ioc.log"
  ioc_runner_log = temp_dir / "run_local_ioc.out"
  ioc_ready_file = temp_dir / "ioc.ready"
  runner_log_handle = None

  try:
    if needs_ioc:
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
          universal_newlines=True,
      )
      wait_for_file(ioc_ready_file, ioc_process, timeout_seconds=120)

    for case in cases:
      run_case(case, repo_root, qtedm_bin, compare_tool, prefix, temp_dir,
          args.update_goldens)
  except Exception as exc:
    keep_temp_dir = True
    terminate_process(ioc_process)
    if runner_log_handle is not None:
      runner_log_handle.close()
      runner_log_handle = None
    if isinstance(exc, CaseFailure):
      message = str(exc)
    else:
      message = f"Unexpected visual test failure: {exc}"
    log_hint = [f"Preserved visual temp dir: {temp_dir}"]
    if ioc_log.exists():
      log_hint.append(f"IOC log: {ioc_log}")
    if ioc_runner_log.exists():
      log_hint.append(f"IOC runner log: {ioc_runner_log}")
    sys.stderr.write(f"{message}\n" + "\n".join(log_hint) + "\n")
    return 1
  finally:
    terminate_process(ioc_process)
    if runner_log_handle is not None:
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
