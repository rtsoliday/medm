#!/usr/bin/env python3
"""Validate the QtEDM widget coverage manifest against repository sources."""

import json
import re
import sys
from pathlib import Path


class ManifestFailure(RuntimeError):
  """Raised when the coverage manifest is incomplete or inconsistent."""


def load_json(path: Path):
  try:
    return json.loads(path.read_text(encoding="utf-8"))
  except (OSError, json.JSONDecodeError) as exc:
    raise ManifestFailure(f"Unable to read {path}: {exc}") from exc


def parse_inventory_classes(header: Path):
  text = header.read_text(encoding="utf-8")
  return set(re.findall(r"QList<([A-Za-z0-9_]+)\s*\*>\s+\w+Elements_;", text))


def parse_extension_types(source: Path):
  text = source.read_text(encoding="utf-8")
  return set(re.findall(
      r'registerObject\(\{QStringLiteral\("([^"]+)"\)', text))


def parse_unit_tests(makefile: Path):
  text = makefile.read_text(encoding="utf-8")
  match = re.search(
      r"QTEDM_UNIT_TEST_NAMES\s*=\s*(.*?)\n\n", text, re.DOTALL)
  if not match:
    raise ManifestFailure("QTEDM_UNIT_TEST_NAMES is missing from qtedm/Makefile")
  return set(re.findall(r"test_[A-Za-z0-9_]+", match.group(1)))


def validate_reference(reference, prefix, known, field, widget_type):
  if not isinstance(reference, str) or not reference.startswith(prefix):
    raise ManifestFailure(
        f"{widget_type}: {field} must start with {prefix}")
  name = reference[len(prefix):].split("::", 1)[0]
  if name not in known:
    raise ManifestFailure(
        f"{widget_type}: unknown {field} reference {reference}")


def validate(repo_root: Path) -> None:
  manifest_path = repo_root / "tests/qtedm_widget_coverage.json"
  manifest = load_json(manifest_path)
  widgets = manifest.get("widgets")
  if not isinstance(widgets, list) or not widgets:
    raise ManifestFailure("Coverage manifest must contain a non-empty widgets array")

  inventory_classes = parse_inventory_classes(
      repo_root / "qtedm/display_window.h")
  extension_types = parse_extension_types(
      repo_root / "qtedm/extension_object_registry.cc")
  unit_tests = parse_unit_tests(repo_root / "qtedm/Makefile")
  ioc_cases = {
      case.get("name") for case in
      load_json(repo_root / "tests/qtedm_ioc_cases.json")
  }
  visual_cases = {
      case.get("name") for case in
      load_json(repo_root / "tests/qtedm_visual_cases.json")
  }
  roundtrip_fixtures = {
      line.strip() for line in
      (repo_root / "tests/qtedm_roundtrip_manifest.txt")
      .read_text(encoding="utf-8").splitlines()
      if line.strip() and not line.lstrip().startswith("#")
  }

  seen_types = set()
  seen_classes = set()
  seen_extensions = set()
  for entry in widgets:
    if not isinstance(entry, dict):
      raise ManifestFailure("Every coverage manifest entry must be an object")
    widget_type = entry.get("type")
    class_name = entry.get("class")
    if not isinstance(widget_type, str) or not widget_type:
      raise ManifestFailure("Every coverage entry needs a type")
    if widget_type in seen_types:
      raise ManifestFailure(f"Duplicate widget type in coverage manifest: {widget_type}")
    seen_types.add(widget_type)

    if entry.get("inventory", True):
      if not isinstance(class_name, str) or not class_name:
        raise ManifestFailure(f"{widget_type}: inventory entry needs a class")
      if class_name in seen_classes:
        raise ManifestFailure(f"Duplicate inventory class: {class_name}")
      seen_classes.add(class_name)

    extension_type = entry.get("extension_type")
    if extension_type:
      if extension_type in seen_extensions:
        raise ManifestFailure(f"Duplicate extension type: {extension_type}")
      seen_extensions.add(extension_type)

    fixture = entry.get("round_trip")
    if not isinstance(fixture, str) or fixture not in roundtrip_fixtures:
      raise ManifestFailure(
          f"{widget_type}: round_trip fixture is missing from "
          "qtedm_roundtrip_manifest.txt")
    if not (repo_root / "tests" / fixture).is_file():
      raise ManifestFailure(f"{widget_type}: missing round-trip fixture {fixture}")

    runtime = entry.get("runtime")
    if isinstance(runtime, str) and runtime.startswith("ioc:"):
      validate_reference(runtime, "ioc:", ioc_cases, "runtime", widget_type)
    else:
      validate_reference(runtime, "unit:", unit_tests, "runtime", widget_type)

    non_rendered = entry.get("non_rendered_reason")
    visual = entry.get("visual")
    if non_rendered:
      if visual:
        raise ManifestFailure(
            f"{widget_type}: non-rendered entries must not name visual coverage")
    elif isinstance(visual, str) and visual.startswith("visual:"):
      validate_reference(visual, "visual:", visual_cases, "visual", widget_type)
    else:
      validate_reference(
          visual, "unit_render:", unit_tests, "visual", widget_type)

  missing_classes = sorted(inventory_classes - seen_classes)
  unknown_classes = sorted(seen_classes - inventory_classes)
  missing_extensions = sorted(extension_types - seen_extensions)
  unknown_extensions = sorted(seen_extensions - extension_types)
  errors = []
  if missing_classes:
    errors.append("uncovered widget classes: " + ", ".join(missing_classes))
  if unknown_classes:
    errors.append("unknown widget classes: " + ", ".join(unknown_classes))
  if missing_extensions:
    errors.append("uncovered extension types: " + ", ".join(missing_extensions))
  if unknown_extensions:
    errors.append("unknown extension types: " + ", ".join(unknown_extensions))
  if errors:
    raise ManifestFailure("; ".join(errors))

  print(
      f"PASS qtedm_widget_coverage ({len(seen_classes)} widget classes, "
      f"{len(seen_extensions)} extension types)")


def main() -> int:
  repo_root = Path(__file__).resolve().parents[1]
  try:
    validate(repo_root)
  except ManifestFailure as exc:
    sys.stderr.write(f"QtEDM coverage manifest validation failed: {exc}\n")
    return 1
  return 0


if __name__ == "__main__":
  sys.exit(main())
