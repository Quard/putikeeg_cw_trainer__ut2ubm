# SPI flash for a real-word vocabulary: size research

Date: 2026-10-12

## Purpose

This research selects the SPI flash size for an optional hardware mod. The mod adds a Winbond W25Q flash chip
that holds English words of 2 to 8 letters. In word sessions, the user then selects random groups (the current
behavior) or real words.

Candidate chips: W25Q80DVSIG, W25Q16JVSIQ, W25Q32JVSIQ, W25Q64JVSIQ and W25Q128JVSIQ.

## Result

Capacity does not limit the vocabulary. The smallest chip, W25Q80 (1 MiB), holds every English word of 2 to 8
letters from a full word-game dictionary. It also holds two smaller frequency tiers and still has 320 KiB free. A larger
chip adds only free space. Select the chip by price, stock and the footprint. If the free space has a planned use
(a second language, call signs, Q codes), W25Q16 doubles the margin.

If the MCU runs from the battery rail, a 2 Mbit serial EEPROM is the simpler mod. It needs no level shifting and
holds the Common tier (see "Serial EEPROM instead of flash").

The real limit is the Koch order. In the first word sessions, the learned letters make almost no real words. The
firmware needs a fallback to random groups at those levels (see "Words per word session").

## Word sources

The counts come from three public lists. The counts include only words with the letters `a` to `z`.

| Tier | Source | Content |
| --- | --- | --- |
| Full | ENABLE word list (`enable1.txt`, 172 823 words) | Word-game dictionary. It has many rare words, for example `aas`, `kas`, `sau`. |
| Common | Full ∩ the 50 000 most frequent words of OpenSubtitles 2018 (hermitdave/FrequencyWords `en_50k.txt`) | Words that people use in speech. |
| Core | Full ∩ Google 10 000 words without swear words (first20hours/google-10000-english) | The most frequent written words. |

The Common and Core tiers use the Full list as a filter, so names and typing errors from the frequency lists stay
out. Words never contain digits, punctuation or prosigns.

## Words per length

Size is the raw data with one byte per letter (uppercase ASCII) and no separator.

| Length | Full | Common | Core |
| ---: | ---: | ---: | ---: |
| 2 | 96 | 96 | 86 |
| 3 | 972 | 810 | 369 |
| 4 | 3 903 | 2 385 | 925 |
| 5 | 8 636 | 3 791 | 1 178 |
| 6 | 15 232 | 5 040 | 1 270 |
| 7 | 23 109 | 5 472 | 1 232 |
| 8 | 28 420 | 4 909 | 1 009 |
| Total words | 80 368 | 22 503 | 6 069 |
| Size, 8 bits per letter | 542 415 B (530 KiB) | 138 933 B (136 KiB) | 35 185 B (34 KiB) |
| Size, 5 bits per letter | 366 962 B (358 KiB) | 96 196 B (94 KiB) | 24 682 B (24 KiB) |

The three tiers together take 716 533 B (700 KiB) at 8 bits per letter. The average Full word is 6.75 bytes.
A packed 5-bit letter code saves one third, but it needs decode code in flash. No chip needs it.

## Comparison of the chips

Usable space is the capacity minus one 4 KiB sector for the map (see "Flash layout"). All five chips hold all
real words of each length, so the "words stored" columns are the same for every chip.

| Chip | Capacity | Usable | 2 | 3 | 4 | 5 | 6 | 7 | 8 | Words stored (Full) | Full set fill | All three tiers fill | Free after three tiers |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| W25Q80DVSIG | 8 Mbit = 1 MiB | 1 020 KiB | 96 | 972 | 3 903 | 8 636 | 15 232 | 23 109 | 28 420 | 80 368 | 51.9 % | 68.6 % | 320 KiB |
| W25Q16JVSIQ | 16 Mbit = 2 MiB | 2 044 KiB | 96 | 972 | 3 903 | 8 636 | 15 232 | 23 109 | 28 420 | 80 368 | 25.9 % | 34.2 % | 1.31 MiB |
| W25Q32JVSIQ | 32 Mbit = 4 MiB | 4 092 KiB | 96 | 972 | 3 903 | 8 636 | 15 232 | 23 109 | 28 420 | 80 368 | 12.9 % | 17.1 % | 3.31 MiB |
| W25Q64JVSIQ | 64 Mbit = 8 MiB | 8 188 KiB | 96 | 972 | 3 903 | 8 636 | 15 232 | 23 109 | 28 420 | 80 368 | 6.5 % | 8.5 % | 7.31 MiB |
| W25Q128JVSIQ | 128 Mbit = 16 MiB | 16 380 KiB | 96 | 972 | 3 903 | 8 636 | 15 232 | 23 109 | 28 420 | 80 368 | 3.2 % | 4.3 % | 15.31 MiB |

The next table shows the theoretical word slots of each chip. It assumes that the chip is full of words with the
English length mix of the Full tier, at 8 bits per letter. English has fewer real words than any chip has slots.

| Chip | 2 | 3 | 4 | 5 | 6 | 7 | 8 | Total slots | Full sets that fit |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| W25Q80 | 184 | 1 871 | 7 515 | 16 629 | 29 330 | 44 498 | 54 725 | 154 752 | 1.9 |
| W25Q16 | 370 | 3 750 | 15 060 | 33 324 | 58 776 | 89 172 | 109 666 | 310 118 | 3.9 |
| W25Q32 | 741 | 7 508 | 30 151 | 66 713 | 117 668 | 178 519 | 219 547 | 620 847 | 7.7 |
| W25Q64 | 1 483 | 15 024 | 60 331 | 133 493 | 235 452 | 357 212 | 439 309 | 1 242 304 | 15.5 |
| W25Q128 | 2 968 | 30 057 | 120 692 | 267 051 | 471 019 | 714 600 | 878 832 | 2 485 219 | 30.9 |

## Words per word session

A word of a session can use only learned letters. The table counts the words of the session length at each word
session of the level model (`training_spec.md` section 6), with `Max Word Length` = 8. "Last" is the newest
learned Koch character.

| Shown level | Learned | Last | Length | Full | Common | Core |
| ---: | ---: | --- | ---: | ---: | ---: | ---: |
| 3 | 3 | R | 2 | 1 | 1 | 1 |
| 6 | 5 | U | 2 | 4 | 4 | 4 |
| 9 | 7 | P | 3 | 37 | 30 | 16 |
| 12 | 9 | L | 3 | 75 | 61 | 32 |
| 15 | 11 | W | 4 | 407 | 262 | 91 |
| 18 | 13 | . | 4 | 579 | 368 | 124 |
| 21 | 15 | J | 5 | 1 124 | 447 | 136 |
| 24 | 17 | F | 5 | 2 538 | 1 060 | 344 |
| 27 | 19 | Y | 6 | 4 233 | 1 308 | 352 |
| 30 | 21 | V | 6 | 4 696 | 1 454 | 406 |
| 33 | 23 | 5 | 7 | 8 368 | 1 896 | 448 |
| 36 | 25 | Q | 7 | 8 543 | 1 941 | 460 |
| 39 | 27 | Z | 8 | 9 402 | 1 491 | 332 |
| 42 | 29 | 3 | 8 | 11 530 | 1 862 | 388 |
| 45 | 31 | B | 8 | 14 019 | 2 205 | 444 |
| 48 | 33 | 4 | 8 | 14 019 | 2 205 | 444 |
| 51 | 35 | 7 | 8 | 14 019 | 2 205 | 444 |
| 54 | 37 | 1 | 8 | 19 898 | 3 305 | 682 |
| 57 | 39 | 6 | 8 | 27 760 | 4 822 | 981 |
| 60 and up | 40 | X | 8 | 28 420 | 4 909 | 1 009 |

Facts from this table:

- Shown level 3 has one word (`mm`). Shown level 6 has four (`mm`, `mu`, `um`, `us`). A 10-word session at
  these levels is not useful, so the firmware falls back to random groups below a minimum count.
- Shown level 9 has real words, but many are rare in the Full tier (`aas`, `ama`, `kas`, `sau`). The Common tier
  is a better default.
- Digits and punctuation unlock no words. So levels 45, 48 and 51 have the same word set.
- The vowels arrive late in the Koch order: E is character 16, I is character 12. Few words exist before them.

## Flash layout

The layout keeps the firmware RAM use near zero. One random word needs two flash reads: one 16-bit count from the
map and one word. No table goes into `xdata`.

1. Sector 0 (4 KiB) holds a header and the maps. The header has a magic value, a format version, the tier count
   and, for each tier, the address of its map.
2. Each tier has one data section for each length 2 to 8. Inside a section, all words have the same length, so
   word `i` is at `section_start + i * length`.
3. Inside a section, the words are sorted by their unlock index. The unlock index of a word is the highest Koch
   index of its letters. So the words that `m` learned characters allow are a prefix of the section.
4. The map of a tier has, for each length, the section start (4 bytes) and 40 cumulative counts (2 bytes each),
   one for each Koch index of the word alphabet (`KOCH_WORD_ALPHABET_MAX` = 40). Count `c[len][m]` is the number
   of words of that length that the first `m` characters allow. A digit or punctuation index repeats the count of
   the index before it.

Map size of one tier: 7 × 4 + 7 × 40 × 2 = 588 bytes. Three tiers with the header use less than 2 KiB of sector 0.
The largest count, 28 420, fits 16 bits.

A random word of length `len` at `m` learned characters:

```
n = c[len][m]                      (one 2-byte read)
if n < minimum: use a random group
i = rng_below(n)
read len bytes at section_start[len] + i * len
```

The new-character rule of section 7 also works: the words that contain the new character have the indexes
`c[len][m - 1]` to `c[len][m] - 1`.

## Hardware notes

These notes do not affect the size choice, but the mod needs them.

- All W25Q80DV and W25QxxJV parts accept 2.7 V to 3.6 V only. The schematic has an HT7833 3.3 V regulator and a
  `+3.3V` net. But the 4 V quiet level of P1.1 (`components_and_wiring.md`) suggests that the MCU runs from
  `VCC`, not from 3.3 V. Before the mod, find the MCU supply. If the MCU pins swing above 3.6 V, the flash needs a
  3.3 V supply and level shifting on its inputs. A serial EEPROM avoids both (see "Serial EEPROM instead of
  flash").
- All candidates use 24-bit addresses and the standard read command `03h`. One firmware driver serves every
  chip.
- Make sure that the package width (150 mil or 208 mil SOIC-8) of the ordered part agrees with the footprint.
- The flash needs four free MCU pins (CS, SCLK, MOSI, MISO). The hardware SPI pin sets conflict with used pins,
  so the flash needs a software SPI driver.
- The words go into the chip once. A programmer (for example a CH341A clip) writes the image before or after
  soldering. A UART loader in the firmware is another option, but it costs flash.

## Serial EEPROM instead of flash

The MCU runs at about 4 V (user report, 2026-10-12). So it likely runs from the Li-ion rail, 3.0 V to 4.2 V.
A W25Q chip accepts 3.6 V at most. Large serial EEPROMs accept up to 5.5 V, so they connect to the MCU pins directly. The cost is capacity:
the largest parts hold 2 Mbit (256 KiB) or, for one ST SPI part, 4 Mbit (512 KiB).

| Part | Bus | Capacity | Supply | Clock | Source |
| --- | --- | ---: | --- | --- | --- |
| Microchip AT24CM02 | I2C | 2 Mbit = 256 KiB | 1.7 V to 5.5 V (1.7 V grade), 2.5 V to 5.5 V (2.5 V grade) | 1 MHz at 2.5 V to 5.5 V, 400 kHz below | [datasheet](https://cdn.sparkfun.com/assets/2/4/8/7/f/AT24CM02-Datasheet.pdf) |
| Microchip AT25M02 | SPI | 2 Mbit = 256 KiB | 1.7 V to 5.5 V (1.7 V grade), 2.5 V to 5.5 V (2.5 V grade) | 5 MHz at 5 V | [datasheet](https://cdn.sparkfun.com/assets/b/b/a/6/e/AT25M02-Datasheet.pdf) |
| ST M95M02-DR | SPI | 2 Mbit = 256 KiB | 1.8 V to 5.5 V | See the datasheet | [ST product page](https://www.st.com/ja/memories/m95m02-dr.html) |
| ST M95M04-DR | SPI | 4 Mbit = 512 KiB | 1.8 V to 5.5 V | 5 MHz at 1.8 V or more, 10 MHz at 2.5 V or more | [datasheet](https://www.st.com.cn/resource/en/datasheet/m95m04-dr.pdf) |

Some distributor listings and clone parts give other voltage ranges. Make sure that the ordering suffix agrees
with the voltage grade in the datasheet.

### What fits

Usable space is the capacity minus 4 KiB for the map, the same as for the flash.

| Content | Size | 512 Kbit (64 KiB) | 2 Mbit (256 KiB) | 4 Mbit (512 KiB) |
| --- | ---: | --- | --- | --- |
| Core, 8 bits per letter | 34 KiB | Fits | Fits | Fits |
| Common, 8 bits per letter | 136 KiB | No | Fits | Fits |
| Common and Core, 8 bits per letter | 170 KiB | No | Fits | Fits |
| Full, 8 bits per letter | 530 KiB | No | No | No |
| Full, 5 bits per letter | 358 KiB | No | No | Fits |
| All three tiers, 8 bits per letter | 700 KiB | No | No | No |

A 2 Mbit EEPROM holds the Common tier, which is the recommended default. The Full tier needs the M95M04-DR and a
5-bit letter code, and the decode code costs MCU flash. The flash layout and the word map do not change.

### Bus and pins

- The STC8H has hardware I2C on P1.4/P1.5, P2.4/P2.5 and P3.3/P3.2 (LQFP32). None of these pin sets is
  available on this board (user report, 2026-10-12). The LCD uses P2.4, and the `enter` button uses P3.2.
- So the EEPROM uses a software (bit-banged) I2C driver on any two free pins. SDA is an open-drain pin with a
  pull-up resistor to `VCC`. SCL can be a push-pull pin, because the MCU is the only master and the AT24CM02 does
  not stretch the clock.
- I2C has no minimum clock rate. An interrupt between two pin changes makes a clock phase longer, and the transfer
  stays correct. So the driver follows the LCD rules of `AGENTS.md`: no interrupt lock, and single-bit port writes
  only.
- An SPI EEPROM needs four pins. The hardware SPI pin sets also conflict with used pins (P1.2 is the headphone
  route, P3.2 is `enter`), so an SPI part also needs a software driver. I2C needs two pins, so it is the better
  choice.
- In the AT24CM02, the two highest address bits go into the device address byte. The driver sends one device
  address and two address bytes for each read.
- One word read is about 12 bytes on the bus (device address, two address bytes, a repeated start, the device
  address again and up to 8 data bytes). A software driver at a few hundred kHz reads one word in less than 1 ms.

### Flash or EEPROM

A W25Q flash holds every tier, but it needs a 3.3 V supply and level shifting on CS, SCLK and MOSI. A 2 Mbit
EEPROM holds the Common tier only, but it connects directly to two free pins with a software I2C driver. Large EEPROMs usually cost more per
bit than NOR flash. Compare the current prices of the exact parts before the decision.

## Open questions

- Flash or EEPROM, after the MCU supply voltage is known.
- Which tiers go into the image: Common only, or Common and Full with a setting?
- The minimum word count of a level before the fallback to random groups.
- The MCU supply voltage.
- The free MCU pins: two for a software I2C EEPROM, four for a flash.
