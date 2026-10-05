# P.E.K. integration patch

This directory is treated as third-party driver code. P.E.K. keeps a narrow
local patch because the original pixel callback did not carry a drawing
context and used unsigned coordinates.

The local changes are limited to:

- passing a caller-owned context to foreground and background pixel callbacks;
- using signed pixel coordinates so the graphics layer can clip text at the
  framebuffer boundary;
- parsing the 23-byte u8g2 font header field by field instead of copying it
  into an implementation-defined bit-field layout;
- keeping decoder helpers private to the translation unit; and
- accepting read-only strings and using a 16-bit text width.

Text layout policy, colors, transparency, clipping, and the returned end
position remain owned by the project `graphics` driver. Keep future changes to
this directory limited to font-decoder correctness or the callback boundary,
and cover them with `firmware/Tests/graphics_test.c`.
