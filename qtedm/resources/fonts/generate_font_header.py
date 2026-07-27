#!/usr/bin/env python3
"""Generate a C++ byte-array header from a font file."""

import argparse
import hashlib
from pathlib import Path


def main() -> int:
  parser = argparse.ArgumentParser()
  parser.add_argument("font", type=Path)
  parser.add_argument("header", type=Path)
  parser.add_argument("--symbol", required=True)
  args = parser.parse_args()

  data = args.font.read_bytes()
  digest = hashlib.sha256(data).hexdigest()
  byte_lines = []
  for offset in range(0, len(data), 12):
    values = ", ".join(f"0x{value:02x}" for value in data[offset:offset + 12])
    byte_lines.append(f"    {values},")

  output = [
      f"// Auto-generated from {args.font.name}; do not edit manually.",
      f"// SHA-256: {digest}",
      "#pragma once",
      "",
      "#include <cstddef>",
      "",
      f"static const unsigned char {args.symbol}Data[] = {{",
      *byte_lines,
      "};",
      "",
      f"static const std::size_t {args.symbol}Size =",
      f"    sizeof({args.symbol}Data);",
      "",
  ]
  args.header.write_text("\n".join(output), encoding="utf-8")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
