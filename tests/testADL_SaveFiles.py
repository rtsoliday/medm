#!/usr/bin/env python3
"""Validate round-trip ADL saves produced by qtedm."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path


def run_qtedm(adl_path: Path, qtedm_path: Path) -> None:
  """Invoke qtedm -testSave for the provided ADL file."""
  result = subprocess.run(
      [str(qtedm_path), "-testSave", str(adl_path)],
      capture_output=True,
      text=True,
      check=False,
  )
  if result.returncode != 0:
    sys.stderr.write(
        f"qtedm -testSave failed for {adl_path}\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}\n"
    )
    sys.exit(result.returncode)


def diff_has_only_name_changes(diff_output: str) -> bool:
  """Return True if diff output only shows changes to name fields."""
  for line in diff_output.splitlines():
    if not line:
      continue
    if line.startswith("<") or line.startswith(">"):
      if "name" not in line.lower():
        return False
  return True


def strip_cartesian_counts_with_pv(lines: list[str]) -> list[str]:
  """Remove count= lines for cartesian plots that define countPvName."""
  to_remove: set[int] = set()
  inside_cartesian = False
  depth = 0
  has_count_pv = False
  block_count_lines: list[int] = []

  for idx, line in enumerate(lines):
    if not inside_cartesian:
      if "\"cartesian plot\"" in line:
        inside_cartesian = True
        depth = line.count("{") - line.count("}")
        has_count_pv = False
        block_count_lines = []
        if depth <= 0:
          depth = 0
        continue
    stripped = line.strip()
    if inside_cartesian and depth == 1:
      lower = stripped.lower()
      if lower.startswith("count="):
        block_count_lines.append(idx)
      if "countpvname" in lower:
        has_count_pv = True
    if inside_cartesian:
      depth += line.count("{")
      depth -= line.count("}")
      if depth <= 0:
        if has_count_pv:
          to_remove.update(block_count_lines)
        inside_cartesian = False
        depth = 0

  return [line for idx, line in enumerate(lines) if idx not in to_remove]


def _strip_default_width_from_block(block_lines: list[str]) -> list[str]:
  """Return block lines omitting width=1 entries inside basic attribute."""
  result: list[str] = []
  inside_basic = False
  basic_depth = 0

  for line in block_lines:
    stripped = line.strip()
    if not inside_basic and stripped.startswith('"basic attribute"'):
      inside_basic = True
      basic_depth = line.count("{") - line.count("}")
      result.append(line)
      if basic_depth <= 0:
        inside_basic = False
      continue

    if inside_basic:
      if stripped != "width=1":
        result.append(line)
      basic_depth += line.count("{") - line.count("}")
      if basic_depth <= 0:
        inside_basic = False
      continue

    result.append(line)

  return result


def strip_default_width_for_widgets(
    lines: list[str], widget_names: tuple[str, ...]) -> list[str]:
  """Remove default width=1 entries added inside specified widgets."""
  result: list[str] = []
  i = 0
  while i < len(lines):
    line = lines[i]
    stripped = line.strip()
    if any(stripped.startswith(name) for name in widget_names):
      block: list[str] = []
      depth = 0
      while i < len(lines):
        block_line = lines[i]
        block.append(block_line)
        depth += block_line.count("{") - block_line.count("}")
        i += 1
        if depth <= 0:
          break
      result.extend(_strip_default_width_from_block(block))
      continue

    result.append(line)
    i += 1

  return result


def _strip_indicator_limits(block_lines: list[str]) -> list[str]:
  """Remove precDefault=1 entries inside indicator limits blocks."""
  result: list[str] = []
  inside_limits = False
  limits_depth = 0

  for line in block_lines:
    stripped = line.strip()
    if not inside_limits and stripped.startswith("limits"):
      inside_limits = True
      limits_depth = line.count("{") - line.count("}")
      result.append(line)
      if limits_depth <= 0:
        inside_limits = False
      continue

    if inside_limits:
      if stripped != "precDefault=1":
        result.append(line)
      limits_depth += line.count("{") - line.count("}")
      if limits_depth <= 0:
        inside_limits = False
      continue

    result.append(line)

  return result


def strip_indicator_prec_default(lines: list[str]) -> list[str]:
  """Remove default precision lines added within indicator widgets."""
  result: list[str] = []
  i = 0
  while i < len(lines):
    line = lines[i]
    stripped = line.strip()
    if stripped.startswith("indicator"):
      block: list[str] = []
      depth = 0
      while i < len(lines):
        block_line = lines[i]
        block.append(block_line)
        depth += block_line.count("{") - block_line.count("}")
        i += 1
        if depth <= 0:
          break
      result.extend(_strip_indicator_limits(block))
      continue

    result.append(line)
    i += 1

  return result


def normalize_lines_for_allowed_differences(lines: list[str]) -> list[str]:
  """Return lines with allowed-difference fields removed."""
  filtered = strip_cartesian_counts_with_pv(lines)
  filtered = strip_default_width_for_widgets(
      filtered, ("rectangle", "polyline"))
  filtered = strip_indicator_prec_default(filtered)
  normalized: list[str] = []
  for line in filtered:
    stripped = line.strip().lower()
    if "name" in stripped:
      continue
    if stripped.startswith("version="):
      continue
    normalized.append(line)
  return [line for line in normalized if line.strip()]


def files_match_with_allowed_variations(original: Path, saved: Path) -> bool:
  """Check if files only differ by acceptable variations."""
  try:
    original_lines = original.read_text().splitlines()
    saved_lines = saved.read_text().splitlines()
  except OSError as exc:
    sys.stderr.write(
        f"Failed to read files for secondary comparison: {exc}\n"
    )
    return False

  return (
      normalize_lines_for_allowed_differences(original_lines)
      == normalize_lines_for_allowed_differences(saved_lines)
  )


def compare_files(original: Path, saved: Path) -> bool:
  """Run diff and allow only approved differences between files."""
  result = subprocess.run(
      ["diff", "-w", str(saved), str(original)],
      capture_output=True,
      text=True,
      check=False,
  )
  if result.returncode == 0:
    return True
  if result.returncode == 1:
    if diff_has_only_name_changes(result.stdout):
      return True
    if files_match_with_allowed_variations(original, saved):
      return True
  if result.returncode not in (0, 1):
    sys.stderr.write(
        f"diff failed comparing {saved} and {original}: {result.stderr}\n"
    )
    sys.exit(result.returncode)
  print(f"Unexpected differences found in {original.name}:")
  print(result.stdout.rstrip())
  return False


def main() -> int:
  parser = argparse.ArgumentParser(
      description="Run qtedm -testSave on ADL files and check for changes."
  )
  parser.add_argument(
      "directory",
      help="Directory containing ADL files to validate",
  )
  args = parser.parse_args()

  target_dir = Path(args.directory).expanduser().resolve()
  if not target_dir.is_dir():
    sys.stderr.write(f"Provided path is not a directory: {target_dir}\n")
    return 1

  repo_root = Path(__file__).resolve().parents[1]
  qtedm_path = repo_root / "bin" / "Linux-x86_64" / "qtedm"
  if not qtedm_path.is_file():
    sys.stderr.write(f"qtedm binary not found at {qtedm_path}\n")
    return 1

  adl_files = sorted(target_dir.glob("*.adl"))
  if not adl_files:
    print(f"No ADL files found in {target_dir}")
    return 0

  tmp_path = Path("/tmp/qtedmTest.adl")
  for adl_path in adl_files:
    run_qtedm(adl_path, qtedm_path)
    if not tmp_path.is_file():
      sys.stderr.write(f"Expected output file not found: {tmp_path}\n")
      return 1
    if not compare_files(adl_path, tmp_path):
      return 1

  print("All ADL files processed successfully.")
  return 0


if __name__ == "__main__":
  sys.exit(main())
