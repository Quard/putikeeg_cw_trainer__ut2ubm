#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Vadym Zakovinko
# SPDX-License-Identifier: BSD-3-Clause
"""Writes Tests/fixtures/font_golden.h, the test oracle for the u8g2 font decoder in src/gfx/font_u8g2.c.

The decoder here follows the u8g2 font format description in references/internal/menu_subsystem_spec.md
section 4.2. It shares no code with the C decoder.

Usage: gen_font_golden.py [<u8g2_fonts.h> [<output.h>]]
"""

import pathlib
import re
import sys

FIRMWARE_DIR = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_FONT_HEADER = FIRMWARE_DIR / "inc" / "u8g2_fonts.h"
DEFAULT_OUTPUT = FIRMWARE_DIR / "Tests" / "fixtures" / "font_golden.h"

HEADER_SIZE = 23
# 8 x 16 glyphs: decoder samples, then the startup screen text "CW Trainer" (ui-ux.md section 5.17).
LARGE_GLYPHS = "Ag0CWTraine"
SIMPLE_ESCAPES = {
    "n": 10, "t": 9, "r": 13, "a": 7, "b": 8, "f": 12, "v": 11,
    "\\": 92, '"': 34, "'": 39, "?": 63,
}


def parse_c_string(body):
    """Returns the bytes of the body of one C string literal (without the quotes)."""
    data = bytearray()
    i = 0
    while i < len(body):
        char = body[i]
        if char != "\\":
            data.append(ord(char))
            i += 1
            continue
        i += 1
        char = body[i]
        if char in "01234567":
            match = re.match(r"[0-7]{1,3}", body[i:])
            data.append(int(match.group(0), 8))
            i += len(match.group(0))
        elif char == "x":
            match = re.match(r"[0-9a-fA-F]+", body[i + 1:])
            data.append(int(match.group(0), 16) & 0xFF)
            i += 1 + len(match.group(0))
        else:
            data.append(SIMPLE_ESCAPES[char])
            i += 1
    return bytes(data)


def read_font(header_text, name):
    """Returns the bytes of the u8g2 font array @p name, including the closing NUL of the literal."""
    match = re.search(re.escape(name) + r"\[\d+\][^=]*=\s*((?:\"(?:[^\"\\]|\\.)*\"\s*)+);", header_text)
    if match is None:
        raise SystemExit(f"font {name} not found")
    literals = re.findall(r"\"((?:[^\"\\]|\\.)*)\"", match.group(1))
    return b"".join(parse_c_string(literal) for literal in literals) + b"\0"


def signed8(value):
    return value - 256 if value >= 128 else value


class BitReader:
    def __init__(self, data, position):
        self.data = data
        self.bit_index = position * 8

    def unsigned(self, count):
        value = 0
        for bit in range(count):
            byte = self.data[self.bit_index >> 3]
            value |= ((byte >> (self.bit_index & 7)) & 1) << bit
            self.bit_index += 1
        return value

    def signed(self, count):
        return self.unsigned(count) - (1 << (count - 1)) if count else 0


class Font:
    def __init__(self, data):
        self.data = data
        (self.bits_zero, self.bits_one, self.bits_width, self.bits_height, self.bits_x, self.bits_y,
         self.bits_pitch) = data[2:9]
        self.bbox_width = data[9]
        self.bbox_height = data[10]
        self.bbox_x = signed8(data[11])
        self.bbox_y = signed8(data[12])
        self.offset_upper = (data[17] << 8) | data[18]
        self.offset_lower = (data[19] << 8) | data[20]

    def find(self, encoding):
        position = HEADER_SIZE
        if ord("A") <= encoding <= ord("Z"):
            position += self.offset_upper
        elif ord("a") <= encoding <= ord("z"):
            position += self.offset_lower
        while self.data[position + 1] != 0:
            if self.data[position] == encoding:
                return position + 2
            position += self.data[position + 1]
        return None

    def render(self, encoding, cell_width, cell_height):
        """Returns (rows, pitch): the cell drawn at (0, 0) with ink 1 on background 0, MSB = column 0."""
        position = self.find(encoding)
        if position is None:
            raise SystemExit(f"glyph 0x{encoding:02X} not in the font")
        bits = BitReader(self.data, position)
        width = bits.unsigned(self.bits_width)
        height = bits.unsigned(self.bits_height)
        x_offset = bits.signed(self.bits_x)
        y_offset = bits.signed(self.bits_y)
        pitch = bits.signed(self.bits_pitch)
        pixels = []
        while len(pixels) < width * height:
            zeros = bits.unsigned(self.bits_zero)
            ones = bits.unsigned(self.bits_one)
            while True:
                pixels.extend([0] * zeros + [1] * ones)
                if bits.unsigned(1) == 0:
                    break
        top = (self.bbox_height + self.bbox_y) - (height + y_offset)
        cell = [[0] * cell_width for _ in range(cell_height)]
        for index in range(width * height):
            row = top + index // width
            column = x_offset + index % width
            if pixels[index] and 0 <= row < cell_height and 0 <= column < cell_width:
                cell[row][column] = 1
        rows = [sum(bit << (cell_width - 1 - column) for column, bit in enumerate(line)) for line in cell]
        return rows, pitch


def c_rows(rows):
    return ", ".join(f"0x{row:02X}U" for row in rows)


def main():
    font_header = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_FONT_HEADER
    output = pathlib.Path(sys.argv[2]) if len(sys.argv) > 2 else DEFAULT_OUTPUT
    text = font_header.read_text(encoding="ascii")
    small = Font(read_font(text, "u8g2_font_spleen5x8_mf"))
    large = Font(read_font(text, "u8g2_font_spleen8x16_mf"))

    lines = [
        "/*",
        " * SPDX-FileCopyrightText: 2026 Vadym Zakovinko",
        " * SPDX-License-Identifier: BSD-3-Clause",
        " */",
        "",
        "#ifndef FONT_GOLDEN_H",
        "#define FONT_GOLDEN_H",
        "",
        "/*",
        " * Golden glyph cells for the u8g2 font decoder tests. Generated by firmware/Tests/tools/gen_font_golden.py",
        " * from firmware/inc/u8g2_fonts.h. Do not edit. Command, from firmware/:",
        " *   python3 Tests/tools/gen_font_golden.py",
        " * Each row byte holds one cell row drawn at (0, 0); the most significant used bit is cell column 0",
        " * (bit 4 for the 5 x 8 font, bit 7 for the 8 x 16 font).",
        " */",
        "",
        "#include <stdint.h>",
        "",
        "#define FONT_GOLDEN_5X8_FIRST 0x20U",
        "#define FONT_GOLDEN_5X8_LAST 0x7EU",
        "#define FONT_GOLDEN_5X8_COUNT (FONT_GOLDEN_5X8_LAST - FONT_GOLDEN_5X8_FIRST + 1U)",
        "#define FONT_GOLDEN_5X8_WIDTH 5U",
        "#define FONT_GOLDEN_5X8_ROWS 8U",
        f"#define FONT_GOLDEN_5X8_CELL_HEIGHT {small.bbox_height}U",
        "#define FONT_GOLDEN_8X16_WIDTH 8U",
        "#define FONT_GOLDEN_8X16_ROWS 16U",
        f"#define FONT_GOLDEN_8X16_CELL_HEIGHT {large.bbox_height}U",
        f"#define FONT_GOLDEN_8X16_COUNT {len(LARGE_GLYPHS)}U",
        "",
        "typedef struct {",
        "  uint8_t rows[FONT_GOLDEN_5X8_ROWS];",
        "  int8_t pitch;",
        "} font_golden_5x8_t;",
        "",
        "typedef struct {",
        "  uint8_t encoding;",
        "  uint8_t rows[FONT_GOLDEN_8X16_ROWS];",
        "  int8_t pitch;",
        "} font_golden_8x16_t;",
        "",
        "/* Index = encoding - FONT_GOLDEN_5X8_FIRST. */",
        "static const font_golden_5x8_t font_golden_5x8[FONT_GOLDEN_5X8_COUNT] = {",
    ]
    for encoding in range(0x20, 0x7F):
        rows, pitch = small.render(encoding, 5, 8)
        lines.append(f"    {{{{{c_rows(rows)}}}, {pitch}}}, /* 0x{encoding:02X} */")
    lines.append("};")
    lines.append("")
    lines.append("static const font_golden_8x16_t font_golden_8x16[FONT_GOLDEN_8X16_COUNT] = {")
    for char in LARGE_GLYPHS:
        rows, pitch = large.render(ord(char), 8, 16)
        lines.append(f"    {{0x{ord(char):02X}U, {{{c_rows(rows)}}}, {pitch}}}, /* '{char}' */")
    lines.append("};")
    lines.append("")
    lines.append("#endif /* FONT_GOLDEN_H */")
    output.write_text("\n".join(lines) + "\n", encoding="ascii")


if __name__ == "__main__":
    main()
