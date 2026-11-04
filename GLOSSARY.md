# Putikeeg CW Trainer

A Morse code (CW) trainer. The user learns characters in Koch order and practices receiving and sending them.

## Learning

**Learning progress**:
The training level of each mode at each character speed. Only Receiving and Sending sessions change it.
_Avoid_: Statistics, score

**Learned characters**:
The characters that the level of a mode at its speed gives, counted from the start of the Koch order.
_Avoid_: Known characters, dictionary

**Mode**:
The direction of practice: Receiving (RX), where the user copies what the trainer plays, or Sending (TX), where the
user keys with the paddle.
_Avoid_: Direction, type

## Free Training

**Free Training**:
The menu section of training programs that never change the learning progress.
_Avoid_: Practice mode, extra training

**Program**:
One kind of practice in Free Training, with its own configuration screen before the session starts. Example:
Callsigns.
_Avoid_: Exercise, drill, game

**Character set**:
The program choice of the characters that it can use: Learned (the learned characters of the selected mode) or Full (the 40
word characters, with no prosigns). The screen label is `Chars`.
_Avoid_: Dictionary, alphabet

## Callsigns

**Callsign**:
A semi-real amateur radio station identifier: prefix, area digit, then a suffix.
_Avoid_: Call, station name

**Prefix**:
The first part of a callsign that names the country, taken from the prefix list. Some prefixes hold a digit
(`9A`, `S5`), and some hold their own area digit (`KH6`, `3D2`).
_Avoid_: Country code

**Area digit**:
The digit after the prefix. A prefix that holds its own area digit gets no extra digit.
_Avoid_: District number, region

**Suffix**:
The last part of a callsign: one to three letters, with no digits.
_Avoid_: Tail, personal part

**Prefix list**:
The fixed list of common real prefixes from which every callsign starts.
_Avoid_: Dictionary, prefix dictionary
