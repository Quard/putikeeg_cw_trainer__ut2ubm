# Display capability guidance

Use only the sections relevant to the device. These are design heuristics; project measurements, component data sheets, and human-factors requirements take precedence.

## LEDs and discrete indicators

- Define each indicator's semantic ownership, visible location, color, steady/blink/pulse patterns, and priority.
- Do not use color as the only distinction. Pair it with position, rhythm, symbol, text, audio, or a documented physical convention.
- Reserve blink rates and patterns so warnings, faults, activity, and pairing/communication states cannot be confused.
- Specify whether an indication is momentary, latched until acknowledgment, or cleared automatically, and what happens when multiple conditions compete.
- Consider daylight, glare, distance, viewing angle, color-vision differences, and whether the indicator is visible during the task that caused it.

## Seven/eight-segment and segmented displays

- Treat segments as a fixed vocabulary. List supported digits, signs, decimal points, letters, symbols, and ambiguous glyphs before defining labels.
- Prefer stable numeric formats and units over scrolling text. Specify leading zeros, decimal placement, negative values, overflow, invalid values, and blank/off behavior.
- Avoid relying on characters that are indistinguishable on the actual module. Verify glyphs at the intended distance and brightness.
- If an indicator must alternate between value, unit, and status, define the cycle, dwell time, interruption rules, and how the user requests each item.
- Preserve a predictable location for sign, decimal, status, and unit markers.

## Small monochrome bitmap displays

- Budget pixels for the actual font, icons, margins, focus treatment, and status region; nominal resolution is not all usable content area.
- Use strong hierarchy, short labels, stable alignment, and simple high-contrast shapes. Avoid thin strokes, dense borders, and icons that require color or fine detail.
- Specify whether the display is reflective or emissive, its lighting/backlight behavior, and the contrast change across temperature, angle, and battery state.
- Make selection, edit mode, disabled state, warning, and stale data distinguishable without relying on subtle inverse-video or animation alone.
- For scrolling or paging, define the user trigger, speed or step, pause behavior, wrap behavior, and what remains visible while moving.

## Small color displays

- Define the usable pixel area, orientation, refresh behavior, color depth, brightness range, and whether color is stable across angle and temperature.
- Use color to reinforce hierarchy or state, never as the sole carrier of critical meaning. Include a shape, label, position, or pattern for important distinctions.
- Budget for readable type and touch targets if the display is also an input surface. Do not assume a high pixel count makes small text usable.
- Specify behavior during partial redraws, dropped frames, startup, dimming, and low-power modes; transient artifacts must not resemble alarms or valid data.
- Treat animation as feedback only when it improves comprehension and does not hide a value, consume excessive power, or create a safety ambiguity.

## Cross-display checks

- Test the actual display at the actual resolution, brightness, viewing angle, enclosure, and expected distance.
- Check the fastest meaningful task and the most stressful fault state, not just an idle screen.
- Verify that critical states remain distinguishable when one channel fails: backlight off, color unavailable, buzzer muted, communication lost, or display partially damaged.
- Record measured limits and hardware-validation results in the applicable scoped `references/internal/ui-ux.md`; keep generic heuristics here.
