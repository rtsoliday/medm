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
    if line.startswith(("+++", "---", "@@")):
      continue
    if line[0] in {"-", "+"}:
      payload = line[1:].strip().lower()
      if not payload:
        continue
      if "name" not in payload:
        return False
  return True


def report_first_difference(diff_output: str) -> None:
  """Print the first non-ignored difference for quick visibility."""
  ignore_tokens = ("name", "version=")

  def is_actionable(line: str) -> bool:
    if line.startswith(("+++", "---")):
      return False
    if not line or line[0] not in {"-", "+"}:
      return False
    payload = line[1:].strip().lower()
    if not payload:
      return False
    return not any(token in payload for token in ignore_tokens)

  removed_line = None
  removed_text = None
  added_line = None
  added_text = None
  orig_counter = 0
  new_counter = 0

  for line in diff_output.splitlines():
    if line.startswith("@@"):
      parts = line.split()
      if len(parts) >= 3:
        orig_range = parts[1]
        new_range = parts[2]
        try:
          orig_counter = int(orig_range.split(",")[0][2:]) - 1
          new_counter = int(new_range.split(",")[0][1:]) - 1
        except ValueError:
          orig_counter = new_counter = 0
      continue

    if line.startswith(" "):
      orig_counter += 1
      new_counter += 1
      continue

    if line.startswith("-") and not line.startswith("---"):
      orig_counter += 1
      if not removed_text and is_actionable(line):
        removed_text = line
        removed_line = orig_counter
      continue

    if line.startswith("+") and not line.startswith("+++"):
      new_counter += 1
      if not added_text and is_actionable(line):
        added_text = line
        added_line = new_counter
      continue

  if removed_text or added_text:
    print("First unexpected difference:")
    if removed_text:
      print(f"  original line {removed_line}: {removed_text}")
    if added_text:
      print(f"  new line {added_line}: {added_text}")


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


def _strip_text_basic_attribute(block_lines: list[str]) -> list[str]:
  """Remove fill= lines from text widget basic attribute blocks."""
  result: list[str] = []
  inside_basic = False
  depth = 0

  for line in block_lines:
    stripped = line.strip()
    normalized = stripped.lower()
    normalized_compact = normalized.replace(" ", "")
    if not inside_basic and stripped.startswith('"basic attribute"'):
      inside_basic = True
      depth = line.count("{") - line.count("}")
      result.append(line)
      if depth <= 0:
        inside_basic = False
      continue

    if inside_basic:
      if (not normalized_compact.startswith("fill=") and
          not normalized_compact.startswith("width=")):
        result.append(line)
      depth += line.count("{") - line.count("}")
      if depth <= 0:
        inside_basic = False
      continue

    result.append(line)

  return result


def strip_text_fill(lines: list[str]) -> list[str]:
  """Remove basic attribute fill lines added to text widgets."""
  result: list[str] = []
  i = 0
  while i < len(lines):
    line = lines[i]
    stripped = line.strip()
    normalized = stripped.lower()
    if normalized.startswith("text {"):
      block: list[str] = []
      depth = 0
      while i < len(lines):
        block_line = lines[i]
        block.append(block_line)
        depth += block_line.count("{") - block_line.count("}")
        i += 1
        if depth <= 0:
          break
      result.extend(_strip_text_basic_attribute(block))
      continue

    result.append(line)
    i += 1

  return result


def _strip_polyline_basic_attribute(block_lines: list[str]) -> list[str]:
  """Remove outline fill lines from polyline basic attribute blocks."""
  result: list[str] = []
  inside_basic = False
  depth = 0

  for line in block_lines:
    stripped = line.strip()
    if not inside_basic and stripped.startswith('"basic attribute"'):
      inside_basic = True
      depth = line.count("{") - line.count("}")
      result.append(line)
      if depth <= 0:
        inside_basic = False
      continue

    if inside_basic:
      if stripped != 'fill="outline"':
        result.append(line)
      depth += line.count("{") - line.count("}")
      if depth <= 0:
        inside_basic = False
      continue

    result.append(line)

  return result


def strip_polyline_outline_fill(lines: list[str]) -> list[str]:
  """Remove fill="outline" lines from polyline widgets."""
  result: list[str] = []
  i = 0
  while i < len(lines):
    line = lines[i]
    stripped = line.strip()
    if stripped.startswith("polyline"):
      block: list[str] = []
      depth = 0
      while i < len(lines):
        block_line = lines[i]
        block.append(block_line)
        depth += block_line.count("{") - block_line.count("}")
        i += 1
        if depth <= 0:
          break
      result.extend(_strip_polyline_basic_attribute(block))
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


def _is_empty_polyline(block_lines: list[str]) -> bool:
  """Return True if block has a points section with no coordinates."""
  inside_points = False
  saw_points = False
  points_depth = 0
  for line in block_lines:
    stripped = line.strip()
    if not inside_points and stripped.startswith("points"):
      inside_points = True
      saw_points = True
      points_depth = line.count("{") - line.count("}")
      if "(" in line:
        return False
      continue

    if inside_points:
      if "(" in line:
        return False
      points_depth += line.count("{") - line.count("}")
      if points_depth <= 0:
        inside_points = False
        break

  return saw_points and not inside_points


def strip_empty_polyline_blocks(lines: list[str]) -> list[str]:
  """Remove polyline blocks that define no points."""
  result: list[str] = []
  i = 0
  while i < len(lines):
    line = lines[i]
    stripped = line.strip()
    if stripped.startswith("polyline"):
      block: list[str] = []
      depth = 0
      while i < len(lines):
        block_line = lines[i]
        block.append(block_line)
        depth += block_line.count("{") - block_line.count("}")
        i += 1
        if depth <= 0:
          break
      if _is_empty_polyline(block):
        continue
      result.extend(block)
      continue

    result.append(line)
    i += 1

  return result


def strip_trailing_spaces_in_quotes(lines: list[str]) -> list[str]:
  """Trim trailing whitespace inside quoted attribute values."""
  result: list[str] = []
  for line in lines:
    first = line.find('"')
    last = line.rfind('"')
    if first == -1 or last == -1 or last <= first:
      result.append(line)
      continue
    value = line[first + 1:last]
    trimmed = value.rstrip()
    if trimmed != value:
      line = line[:first + 1] + trimmed + line[last:]
    result.append(line)
  return result


def normalize_lines_for_allowed_differences(lines: list[str]) -> list[str]:
  """Return lines with allowed-difference fields removed."""
  filtered = strip_cartesian_counts_with_pv(lines)
  filtered = strip_default_width_for_widgets(
      filtered, ("rectangle", "polyline"))
  filtered = strip_indicator_prec_default(filtered)
  filtered = strip_text_fill(filtered)
  filtered = strip_polyline_outline_fill(filtered)
  filtered = strip_empty_polyline_blocks(filtered)
  filtered = strip_trailing_spaces_in_quotes(filtered)
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
      ["diff", "-w", "-u", str(saved), str(original)],
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
  report_first_difference(result.stdout)
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
