#!/usr/bin/env python3
"""Generate the compact Unicode case-fold table used by the wx-free core."""

from pathlib import Path


def main() -> None:
    entries = []
    for source in range(0x110000):
        folded = chr(source).casefold()
        values = [ord(character) for character in folded]
        if values != [source]:
            entries.append((source, values))

    output = [
        "#pragma once",
        "",
        "#include <cstdint>",
        "",
        "namespace say_count::unicode_casefold_data {",
        "struct Mapping { std::uint32_t source; std::uint8_t length; std::uint32_t folded[3]; };",
        "inline constexpr Mapping kMappings[] = {",
    ]
    for source, values in entries:
        padded = values + [0] * (3 - len(values))
        output.append(
            f"    {{0x{source:X}, {len(values)}, {{0x{padded[0]:X}, 0x{padded[1]:X}, 0x{padded[2]:X}}}}},"
        )
    output.extend(["};", "}  // namespace say_count::unicode_casefold_data", ""])
    target = Path(__file__).resolve().parents[1] / "src/core/unicode_casefold.h"
    target.write_text("\n".join(output), encoding="utf-8")


if __name__ == "__main__":
    main()
