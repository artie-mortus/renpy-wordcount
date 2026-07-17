#!/usr/bin/env python3
"""Count Unicode characters in text without invoking Say Count."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Count Unicode code points, including whitespace and newlines. "
            "Pass text as an argument, use --file, or pipe text to stdin."
        )
    )
    source = parser.add_mutually_exclusive_group()
    source.add_argument("text", nargs="?", help="text to count")
    source.add_argument("--file", "-f", type=Path, help="UTF-8 text file to count")
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.file is not None:
        try:
            text = args.file.read_text(encoding="utf-8")
        except (OSError, UnicodeError) as error:
            print(f"count-characters: {error}", file=sys.stderr)
            return 2
    elif args.text is not None:
        text = args.text
    elif not sys.stdin.isatty():
        text = sys.stdin.read()
    else:
        print("count-characters: provide text, --file, or stdin", file=sys.stderr)
        return 2

    print(len(text))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
