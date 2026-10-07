#!/usr/bin/env bash
# Scans SDCC-generated .asm files for operations that break the interrupt and port-sharing rules in AGENTS.md.
#
# Usage:
#   check_asm.sh <asm-root>                 Scan every *.asm under <asm-root>. Exit 0 clean, 1 violation, 2 error.
#   check_asm.sh --self-test [<fixtures>]   Run the built-in cases, then the fixture cases (see below).
#
# The module name is the file name without ".asm", for example main.c or lcd_spi.c.
#
# Every direct operand is resolved to a number first: hex (0xa0, 0x00a0, 0a0h, a0h), decimal (160), binary
# (0b...), names from the file's "name = value" equates (with or without the leading _, any case), the
# predefined names p2, ie, ea, ie2, t4t3m, auxintif, p2m0, p2m1, p0m0, p0m1, p3m0, p3m1, p_sw2, pcon, rstcfg, p1,
# p1m1, p1m0, tmod, auxr, p5, p5m1, p5m0, dotted bits (p2.3, ie.7, 0xa8.7) and +/- sums of these. The instruction
# decides the meaning: in setb, clr, cpl, jbc, jb, jnb, "mov bit,c" and "anl/orl c,bit" the number is a bit address,
# everywhere else a byte address.
#
# Rules on resolved addresses:
#   R1  No byte write to 0xA0 (P2): mov, orl, anl, xrl, inc, dec, djnz, pop or xch.
#   R2  No byte write to, push of or pop of 0xA8 (IE). No clr, cpl, jbc or "mov bit,c" on bit 0xAF (EA).
#       Bits 0xA0-0xA7 (P2 pins) and 0xA8-0xAE (single interrupt enables) stay allowed.
#   R3  Outside main.c: no setb on bit 0xAF and no orl into byte 0xA8.
#   R4  Byte writes to 0x95 (P2M1) and 0x96 (P2M0) only in lcd_spi.c, as exactly one "orl 0x96,#0x1f" and one
#       "anl 0x95,#0xe0"; the masks are compared as numbers.
#   R5  Byte 0xAF (IE2), 0xD1 (T4T3M), 0xEF (AUXINTIF), 0x93 (P0M1), 0x94 (P0M0), 0xB1 (P3M1), 0xB2 (P3M0),
#       0xBA (P_SW2), 0x87 (PCON), 0xFF (RSTCFG), 0x90 (P1), 0x91 (P1M1), 0x92 (P1M0), 0x89 (TMOD), 0x8E (AUXR),
#       0xC8 (P5), 0xC9 (P5M1) and 0xCA (P5M0) change only with orl or anl.
# Fail closed: a written operand that the scan cannot resolve but that contains a protected name or address is a
# violation. Data operands such as (_tick_count_ms + 1) contain no protected name and stay accepted.
#
# Rules on function bodies. A body starts at its label line (_name: at column 0) and ends before the next column-0
# label that starts with _, or at the end of the file:
#   R6  Module sidetone.c has the label _sidetone_timer0_isr (a missing label is a violation). Its body has exactly
#       three calls (lcall or acall): _keyer_service, _player_service and _sidetone_service, in this order, no
#       ljmp, ajmp or sjmp to a label that starts with _, and a reti. In every module, the bodies of _keyer_service,
#       _player_service and _sidetone_service have no lcall, no acall and no ljmp, ajmp or sjmp to a label that
#       starts with _. Other functions are not checked.
#   R7  No line of any file contains sdcc_atomic, in any case.
#
# Fixture convention for --self-test: <fixtures>/reject/<case>/ and <fixtures>/accept/<case>/ are scan roots.
# Every reject case must exit 1, every accept case must exit 0. A reject case named r<N>_<anything> must also be
# reported under rule R<N>, so it cannot pass by tripping another rule. <fixtures> defaults to
# firmware/Tests/tools/check_asm; a missing default directory is skipped, a missing given directory is an error.
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
default_fixtures="${script_dir}/../Tests/tools/check_asm"

# Pass 1 reads the equates; pass 2 checks the instructions. Prints "R<N><TAB><line>:<instruction>".
# shellcheck disable=SC2016
readonly SCAN_AWK='
function trim(text) { gsub(/^[ \t]+|[ \t]+$/, "", text); return text }
function digits(text, radix,   i, d, value) {
  if (text == "") return -1
  value = 0
  for (i = 1; i <= length(text); i++) {
    d = index("0123456789abcdef", substr(text, i, 1)) - 1
    if (d < 0 || d >= radix) return -1
    value = value * radix + d
  }
  return value
}
function number(text) {
  if (text ~ /^0x[0-9a-f]+$/) return digits(substr(text, 3), 16)
  if (text ~ /^0b[01]+$/) return digits(substr(text, 3), 2)
  if (text ~ /^[0-9]+$/) return digits(text, 10)
  if (text ~ /^[0-9a-f]+h$/) return digits(substr(text, 1, length(text) - 1), 16)
  return -1
}
function term(text,   key) {
  text = trim(text)
  if (text in equates) return equates[text]
  key = text
  sub(/^_/, "", key)
  if (key in equates) return equates[key]
  if (key in predefined) return predefined[key]
  return number(text)
}
function is_protected(address) {
  return (address >= 160 && address <= 175) || address == 149 || address == 150 || (address in shared_bytes)
}
# Returns the address, -1 when it is unknown (a data label), or -2 when it cannot be resolved but contains a
# protected token; the token is then in protected_token.
function resolve(operand,   base, count, parts, i, value, total, piece, sign) {
  operand = trim(operand)
  while (operand ~ /^\(.*\)$/) operand = trim(substr(operand, 2, length(operand) - 2))
  if (operand ~ /^[a-z0-9_$]+\.[0-7]$/) {
    base = term(substr(operand, 1, length(operand) - 2))
    if (base >= 128 && base % 8 == 0) return base + substr(operand, length(operand), 1)
    if (base >= 32 && base <= 47) return (base - 32) * 8 + substr(operand, length(operand), 1)
    return -1
  }
  if (operand !~ /[-+*\/&|^<>()~ ]/) return term(operand)
  if (operand ~ /^[-+]?[^-+*\/&|^<>()~]+([-+][^-+*\/&|^<>()~]+)*$/) {
    piece = operand
    gsub(/-/, "+-", piece)
    count = split(piece, parts, "+")
    total = 0
    for (i = 1; i <= count; i++) {
      if (trim(parts[i]) == "") continue
      sign = 1
      if (substr(trim(parts[i]), 1, 1) == "-") { sign = -1; parts[i] = substr(trim(parts[i]), 2) }
      value = term(parts[i])
      if (value < 0) { total = -1; break }
      total += sign * value
    }
    if (total >= 0) return total
  }
  count = split(operand, parts, /[-+*\/&|^<>()~ ]+/)
  for (i = 1; i <= count; i++) {
    value = term(parts[i])
    if (value >= 0 && is_protected(value)) { protected_token = value; return -2 }
  }
  return -1
}
function is_register(operand) {
  return operand ~ /^(a|c|ab|dptr|r[0-7]|@r[01]|@dptr|@a\+dptr|@a\+pc)$/ || operand ~ /^[#\/]/ || operand == ""
}
function emit(rule) { print rule "\t" shown; }
function byte_write(op, destination, source,   address, mask) {
  address = resolve(destination)
  if (address == -2) {
    if (protected_token >= 160 && protected_token <= 167) emit("R1")
    else if (protected_token >= 168 && protected_token <= 174) emit("R2")
    else if (protected_token == 149 || protected_token == 150) emit("R4")
    else emit("R5")
    return
  }
  if (address == 160) emit("R1")
  else if (address == 168) {
    emit("R2")
    if (op == "orl" && module != "main.c") emit("R3")
  } else if (address == 149 || address == 150) {
    if (module != "lcd_spi.c") { emit("R4"); return }
    mask = (source ~ /^#/) ? resolve(substr(source, 2)) : -1
    if (op == "orl" && address == 150 && mask == 31) orl_count++
    else if (op == "anl" && address == 149 && mask == 224) anl_count++
    else emit("R4")
  } else if (address in shared_bytes) {
    if (op != "orl" && op != "anl") emit("R5")
  }
}
function bit_write(op, destination,   address) {
  address = resolve(destination)
  if (address == -2) {
    if (protected_token >= 160 && protected_token <= 175) emit("R2")
    return
  }
  if (address != 175) return
  if (op == "setb") { if (module != "main.c") emit("R3") }
  else emit("R2")
}
BEGIN {
  isr_label = "_sidetone_timer0_isr"
  split("_keyer_service _player_service _sidetone_service", isr_order, " ")
  leaf_services["_keyer_service"] = 1; leaf_services["_player_service"] = 1; leaf_services["_sidetone_service"] = 1
  predefined["p2"] = 160; predefined["ie"] = 168; predefined["ea"] = 175; predefined["ie2"] = 175
  predefined["t4t3m"] = 209; predefined["auxintif"] = 239; predefined["p2m0"] = 150; predefined["p2m1"] = 149
  predefined["p0m1"] = 147; predefined["p0m0"] = 148; predefined["p3m1"] = 177; predefined["p3m0"] = 178
  predefined["p_sw2"] = 186; predefined["pcon"] = 135; predefined["rstcfg"] = 255
  predefined["p1"] = 144; predefined["p1m1"] = 145; predefined["p1m0"] = 146; predefined["tmod"] = 137
  predefined["auxr"] = 142; predefined["p5"] = 200; predefined["p5m1"] = 201; predefined["p5m0"] = 202
  # R5: bytes that several modules or an interrupt share, so they change only with orl or anl. TMOD holds Timer 1
  # in its high nibble, AUXR holds EXTRAM, port 1 holds the audio pins and the plug input, port 5 holds the
  # paddle ring input.
  split("175 209 239 147 148 177 178 186 135 255 144 145 146 137 142 200 201 202", shared_list, " ")
  for (i in shared_list) shared_bytes[shared_list[i] + 0] = 1
}
FNR == NR {
  line = tolower($0)
  sub(/;.*/, "", line)
  if (match(line, /^[ \t]*[a-z_$.][a-z0-9_$.]*[ \t]*==?[ \t]*/)) {
    name = line
    sub(/^[ \t]*/, "", name)
    sub(/[ \t]*==?.*/, "", name)
    value = resolve(substr(line, RLENGTH + 1))
    if (value >= 0) {
      equates[name] = value
      if (sub(/^_/, "", name)) equates[name] = value
    }
  }
  next
}
{
  text = tolower($0)
  if (index(text, "sdcc_atomic") > 0) {
    shown = FNR ":" trim(text)
    emit("R7")
  }
  if (match(text, /^_[a-z0-9_$]*::?/)) {
    function_name = substr(text, 1, RLENGTH)
    sub(/:+$/, "", function_name)
    if (function_name == isr_label) has_isr = 1
  }
  sub(/;.*/, "", text)
  gsub(/\t/, " ", text)
  sub(/^ *[a-z0-9_$.]+::? */, "", text)
  gsub(/ +/, " ", text)
  text = trim(text)
  if (text == "" || text ~ /^\./ || text ~ /=/) next
  op = text
  sub(/ .*/, "", op)
  rest = (index(text, " ") > 0) ? substr(text, index(text, " ") + 1) : ""
  argc = split(rest, args, ",")
  for (i = 1; i <= argc; i++) args[i] = trim(args[i])
  for (i = argc + 1; i <= 3; i++) args[i] = ""
  shown = FNR ":" text
  is_call = (op == "lcall" || op == "acall")
  is_function_jump = (op == "ljmp" || op == "ajmp" || op == "sjmp") && (args[1] ~ /^_/)
  if (function_name == isr_label) {
    if (is_call) {
      isr_call_count++
      if (isr_call_count > 3 || args[1] != isr_order[isr_call_count]) emit("R6")
    }
    if (is_function_jump) emit("R6")
    if (op == "reti") has_isr_reti = 1
  }
  if ((function_name in leaf_services) && (is_call || is_function_jump)) emit("R6")
  if (op == "setb" || op == "clr" || op == "cpl" || op == "jbc") {
    if (!is_register(args[1])) bit_write(op, args[1])
  } else if (op == "mov") {
    if (args[2] == "c") bit_write(op, args[1])
    else if (!is_register(args[1])) byte_write(op, args[1], args[2])
  } else if (op == "orl" || op == "anl" || op == "xrl") {
    if (!is_register(args[1])) byte_write(op, args[1], args[2])
  } else if (op == "inc" || op == "dec" || op == "djnz" || op == "pop") {
    if (!is_register(args[1])) byte_write(op, args[1], args[2])
  } else if (op == "xch") {
    if (args[1] == "a" && !is_register(args[2])) byte_write(op, args[2], "")
  } else if (op == "push") {
    if (resolve(args[1]) == 168 || (resolve(args[1]) == -2 && protected_token == 168)) emit("R2")
  }
}
END {
  if (module == "lcd_spi.c" && (orl_count != 1 || anl_count != 1)) {
    shown = "0: expected one orl 0x96,#0x1f and one anl 0x95,#0xe0, found " (orl_count + 0) " and " (anl_count + 0)
    emit("R4")
  }
  if (module == "sidetone.c") {
    if (!has_isr) {
      shown = "0: no " isr_label " label"
      emit("R6")
    } else {
      if (isr_call_count != 3) {
        shown = "0: expected 3 calls in " isr_label ", found " (isr_call_count + 0)
        emit("R6")
      }
      if (!has_isr_reti) {
        shown = "0: no reti in " isr_label
        emit("R6")
      }
    }
  }
}
'

violation_count=0

scan_file() {
  local file="$1"
  local module rule location
  module="$(basename "${file}" .asm)"
  while IFS=$'\t' read -r rule location; do
    [[ -z "${rule}" ]] && continue
    echo "check_asm: ${rule}: ${file}:${location}" >&2
    violation_count=$((violation_count + 1))
  done < <(awk -v module="${module}" "${SCAN_AWK}" "${file}" "${file}")
}

# Scans one root. Exit status: 0 clean, 1 violations, 2 no .asm files.
scan_root() {
  local root="$1"
  local file
  local file_count=0
  violation_count=0
  while IFS= read -r -d '' file; do
    scan_file "${file}"
    file_count=$((file_count + 1))
  done < <(find "${root}" -name '*.asm' -type f -print0 | sort -z)
  if [[ "${file_count}" -eq 0 ]]; then
    echo "check_asm: no .asm files under ${root}" >&2
    return 2
  fi
  if [[ "${violation_count}" -ne 0 ]]; then
    echo "check_asm: ${violation_count} violation(s) in ${root}" >&2
    return 1
  fi
  return 0
}

# Runs scan_root in a subshell and compares the exit status (and for r<N>_ cases the rule) with the expectation.
expect_status() {
  local expected="$1" root="$2" name="$3"
  local status=0 case_name rule
  (scan_root "${root}") 2> "${self_test_log}" || status=$?
  if [[ "${status}" -ne "${expected}" ]]; then
    echo "check_asm self-test: FAIL ${name}: expected exit ${expected}, got ${status}" >&2
    cat "${self_test_log}" >&2
    return 1
  fi
  case_name="$(basename "${name}")"
  if [[ "${expected}" -eq 1 && "${case_name}" =~ ^r([0-9]+)_ ]]; then
    rule="R${BASH_REMATCH[1]}"
    if ! grep -q "^check_asm: ${rule}: " "${self_test_log}"; then
      echo "check_asm self-test: FAIL ${name}: rejected, but not under ${rule}" >&2
      cat "${self_test_log}" >&2
      return 1
    fi
  fi
  echo "check_asm self-test: ok   ${name}"
  case_count=$((case_count + 1))
}

# Writes one built-in case: $1 case dir, $2 module file name, $3 assembly text.
write_case() {
  mkdir -p "$1"
  printf '%s\n' "$3" > "$1/$2.asm"
}

# Leaf services and the three-call Timer 0 ISR (rule R6). Every sidetone.c case needs them.
readonly SERVICES=$'_keyer_service:\n\tjb\t_keyer_is_enabled,00102$\n\tret\n00102$:\n\tsjmp\t00103$\n00103$:\n\tret\n_player_service:\n\tret\n_sidetone_service:\n\tret'
readonly ISR_HEAD=$'\n_sidetone_timer0_isr:\n\tpush\tacc\n\tmov\tpsw,#0x00'
readonly ISR_TAIL=$'\n\tpop\tacc\n\treti'
readonly ISR_OK="${ISR_HEAD}"$'\n\tlcall\t_keyer_service\n\tlcall\t_player_service\n\tlcall\t_sidetone_service'"${ISR_TAIL}"
readonly SIDETONE_OK=$'\n'"${SERVICES}${ISR_OK}"

readonly CLEAN_SPI=$'\torl\t_P2M0,#0x1f\n\tanl\t_P2M1,#0xe0\n\tclr\t_P2_3\n\tmov\t_P2_4,c\n\tsetb\t_P2_0'

# Built-in cases as "name|module|assembly". Names start with accept_ or r<N>_.
readonly -a BUILTIN_CASES=(
  "accept_clean|lcd_spi.c|${CLEAN_SPI}"
  "accept_main_enables_ea|main.c|"$'\tsetb\t_EA\n\tmov\ta,_P2\t; read only\n\torl\t_T4T3M,#0x08\n\tsetb\tea'
  "accept_shared_orl_anl|tick_timer3.c|"$'\tanl\t_AUXINTIF,#0xfd\n\torl\t_IE2,#0x20\n\tmov\t_T3H,#0xf8\n\tanl\t0xaf,#0xdf'
  "accept_bit_ops_on_raw_addresses|lcd.c|"$'\tsetb\t0xa0\n\tmov\t0xa0,c\n\tmov\tc,0xaf\n\tjb\tea,00101$\n\tmov\t0xa8,c\n\tpush\t_P2'
  "accept_mask_spellings|lcd_spi.c|"$'\tORL\tP2M0, #0x1F\n\tanl\t_p2m1,#224'
  "accept_mask_suffix_hex|lcd_spi.c|"$'\torl\t0x96,#01fh\n\tanl\t0x95,#0e0h'
  "r1_mov_p2|lcd.c|"$'\tmov\t_P2,#0xff'
  "r1_orl_p2|lcd.c|"$'\torl\t_P2,#0x01'
  "r1_anl_p2|lcd.c|"$'\tanl\t_P2,a'
  "r1_xrl_p2|lcd.c|"$'\txrl\t_P2,#0x10'
  "r1_inc_p2|lcd.c|"$'\tinc\t_P2'
  "r1_dec_p2|lcd.c|"$'\tdec\t_P2'
  "r1_xch_p2|lcd.c|"$'\txch\ta,_P2'
  "r1_pop_p2|lcd.c|"$'\tpop\t_P2'
  "r1_builtin_name|lcd.c|"$'\tmov\tp2,#0xff'
  "r1_upper_case|lcd.c|"$'\tMOV\tP2,A'
  "r1_raw_address|lcd.c|"$'\tmov\t0xa0,a'
  "r1_raw_suffix_address|lcd.c|"$'\tORL\t0A0H,#1'
  "r2_clr_ea|lcd.c|"$'\tclr\t_EA'
  "r2_cpl_ea|lcd.c|"$'\tcpl\t_EA'
  "r2_jbc_ea|lcd.c|"$'\tjbc\t_EA,00101$'
  "r2_mov_ea_c|lcd.c|"$'\tmov\t_EA,c'
  "r2_sdcc_critical|lcd.c|"$'\tsetb\tc\n\tjbc\tea,00103$\n\tclr\tc\n00103$:\n\tmov\tea,c'
  "r2_clr_ea_raw_bit|lcd.c|"$'\tclr\t0xaf'
  "r2_clr_ie_7|lcd.c|"$'\tclr\tie.7'
  "r2_clr_ea_main|main.c|"$'\tclr\t_EA'
  "r2_clr_ea_after_label|lcd.c|"$'00101$:\tclr\t_EA\t; comment'
  "r2_mov_ie|main.c|"$'\tmov\t_IE,#0x80'
  "r2_anl_ie|main.c|"$'\tanl\tie,#0x7f'
  "r2_orl_ie_main|main.c|"$'\torl\t_IE,#0x02'
  "r2_raw_ie|lcd.c|"$'\tmov\t0xa8,a'
  "r2_push_ie|lcd.c|"$'\tpush\t_IE'
  "r2_pop_ie|lcd.c|"$'\tpop\tIE'
  "r3_setb_ea|lcd.c|"$'\tsetb\t_EA'
  "r3_setb_ea_raw_bit|lcd.c|"$'\tsetb\t0xaf'
  "r3_orl_ie|lcd.c|"$'\torl\t_IE,#0x80'
  "r4_p2m_outside|lcd.c|"$'\torl\t_P2M0,#0x1f'
  "r4_p2m_raw_outside|main.c|"$'\tmov\t0x96,#0x00'
  "r4_p2m_mov|lcd_spi.c|"$'\tmov\t_P2M0,#0x1f\n\tanl\t_P2M1,#0xe0'
  "r4_p2m_twice|lcd_spi.c|${CLEAN_SPI}"$'\n\torl\t_P2M0,#0x01'
  "r4_p2m_missing|lcd_spi.c|"$'\tclr\t_P2_3'
  "r4_p2m0_wrong_mask|lcd_spi.c|"$'\torl\t_P2M0,#0xff\n\tanl\t_P2M1,#0xe0'
  "r4_p2m1_wrong_mask|lcd_spi.c|"$'\torl\t_P2M0,#0x1f\n\tanl\t_P2M1,#0x00'
  "accept_mask_in_parentheses|lcd_spi.c|"$'\torl\t_P2M0,#(0x1f)\n\tanl\t_P2M1,#(0xe0)'
  "r4_p2m0_unresolved_mask|lcd_spi.c|"$'\torl\t_P2M0,#(MASK * 1)\n\tanl\t_P2M1,#0xe0'
  "r4_p2m0_register_operand|lcd_spi.c|"$'\torl\t_P2M0,a\n\tanl\t_P2M1,#0xe0'
  "r5_mov_t4t3m|tick_timer3.c|"$'\tmov\t_T4T3M,#0x08'
  "r5_mov_ie2|tick_timer3.c|"$'\tmov\t_IE2,#0x20'
  "r5_mov_ie2_raw_byte|tick_timer3.c|"$'\tmov\t0xaf,#0x20'
  "r5_mov_auxintif|tick_timer3.c|"$'\tmov\t_AUXINTIF,#0x00'
  "r5_pop_ie2|tick_timer3.c|"$'\tpop\t_IE2'
  "r5_xrl_t4t3m|tick_timer3.c|"$'\txrl\tT4T3M,#0x08'
  "r5_inc_auxintif|tick_timer3.c|"$'\tinc\t0xef'
  "accept_ie_enable_bits|lcd.c|"$'_ET0\t=\t0x00a9\n\tsetb\t_ET0\n\tclr\t0xa9\n\tsetb\tie.1\n\tclr\t0xa8.4\n\tcpl\t0xae'
  "accept_dotted_p2_bit|lcd.c|"$'\tsetb\tp2.3\n\tclr\t_P2.3\n\tmov\tP2.4,c'
  "accept_data_expressions|lcd.c|"$'\tmov\t(_x + 1),a\n\tinc\t(_tick_count_ms + 1)\n\tmov\t_x,#0xa0'
  "r1_padded_address|lcd.c|"$'\tmov\t0x00a0,a'
  "r1_decimal_address|lcd.c|"$'\tmov\t160,a'
  "r1_bare_suffix_address|lcd.c|"$'\tmov\ta0h,a'
  "r1_c_alias|lcd.c|"$'_LCD_PORT\t=\t0x00a0\n\tmov\t_LCD_PORT,#0xff'
  "r1_alias_of_alias|lcd.c|"$'PORT_A\t=\t0xa0\nPORT_B\t=\tPORT_A\n\tmov\tPORT_B,a'
  "r1_resolved_expression|lcd.c|"$'_LCD_PORT\t=\t0x00a0\n\tmov\t(_LCD_PORT + 0),a'
  "r1_unresolved_expression|lcd.c|"$'_LCD_PORT\t=\t0x00a0\n\tmov\t(_LCD_PORT * 1),a'
  "r1_bit_symbol_as_byte|lcd.c|"$'_P2_0\t=\t0x00a0\n\tmov\t_P2_0,#0x01'
  "r2_c_alias_sbit|lcd.c|"$'_IRQ_ENABLE\t=\t0x00af\n\tclr\t_IRQ_ENABLE'
  "r2_c_alias_ie|lcd.c|"$'_INT_EN\t=\t0x00a8\n\tanl\t_INT_EN,#0x7f'
  "r2_bit_sum_reaches_ea|lcd.c|"$'_EX0\t=\t0x00a8\n\tclr\t(_EX0 + 7)'
  "r2_bit_unresolved|lcd.c|"$'_EX0\t=\t0x00a8\n\tclr\t(_EX0 * 2)'
  "r2_push_ie_raw|lcd.c|"$'\tpush\t168'
  "r3_setb_alias|lcd.c|"$'IRQ\t=\t0xaf\n\tsetb\tIRQ'
  "r4_alias_outside|lcd.c|"$'MODE\t=\t0x96\n\torl\tMODE,#0x1f'
  "r5_c_alias_t4t3m|tick_timer3.c|"$'_TIMER34\t=\t0x00d1\n\tmov\t_TIMER34,#0x08'
  "r5_decimal_ie2|tick_timer3.c|"$'\tmov\t175,#0x20'
  "accept_p0m1_orl_anl|buttons.c|"$'\torl\t_P0M1,#0x07\n\tanl\tp0m1,#0xf7'
  "r5_mov_p0m1|buttons.c|"$'\tmov\t_P0M1,#0x07'
  "accept_p0m0_orl_anl|backlight.c|"$'\tanl\t_P0M0,#0xf8\n\torl\t0x94,#0x08'
  "r5_mov_p0m0|backlight.c|"$'\tmov\t0x94,a'
  "accept_p3m1_orl_anl|buttons.c|"$'\torl\t_P3M1,#0x04\n\tanl\tp3m1,#0xff'
  "r5_xch_p3m1|buttons.c|"$'\txch\ta,_P3M1'
  "accept_p3m0_orl_anl|buttons.c|"$'\tanl\t_P3M0,#0xfb\n\torl\t0b2h,#0x00'
  "r5_mov_p3m0|buttons.c|"$'\tmov\t_P3M0,#0x00'
  "accept_p_sw2_orl_anl|backlight.c|"$'\torl\t_P_SW2,#0x80\n\tanl\t186,#0xff'
  "r5_mov_p_sw2|backlight.c|"$'\tmov\t_P_SW2,#0x80'
  "accept_pcon_orl_anl|eeprom.c|"$'\tanl\t_PCON,#0xdf\n\torl\tpcon,#0x00'
  "r5_mov_pcon|eeprom.c|"$'\tmov\t_PCON,#0x00'
  "r5_pcon_unresolved_expression|eeprom.c|"$'\tmov\t(_PCON * 1),a'
  "accept_rstcfg_orl_anl|eeprom.c|"$'\torl\t_RSTCFG,#0x03\n\tanl\t0xff,#0xfc'
  "r5_mov_rstcfg|eeprom.c|"$'\tmov\t_RSTCFG,#0x03'
  "r5_pop_rstcfg|eeprom.c|"$'\tpop\t0xff'
  "accept_p0_bits_near_pcon|buttons.c|"$'\tsetb\t0x87\n\tclr\t_P0_7\n\tmov\ta,_PCON'
  "accept_p1_orl_anl|sidetone.c|"$'\torl\t_P1,#0x06\n\tanl\tp1,#0xf9'"${SIDETONE_OK}"
  "r5_mov_p1|sidetone.c|"$'\tmov\t_P1,#0xff'"${SIDETONE_OK}"
  "accept_p1m1_orl_anl|sidetone.c|"$'\tanl\t_P1M1,#0xf9\n\torl\t0x91,#0x01'"${SIDETONE_OK}"
  "r5_mov_p1m1|sidetone.c|"$'\tmov\t_P1M1,#0x01'"${SIDETONE_OK}"
  "accept_p1m0_orl_anl|sidetone.c|"$'\torl\t_P1M0,#0x06\n\tanl\tp1m0,#0xfe'"${SIDETONE_OK}"
  "r5_mov_p1m0|sidetone.c|"$'\tmov\t0x92,a'"${SIDETONE_OK}"
  "accept_tmod_orl_anl|sidetone.c|"$'\tanl\t_TMOD,#0xf0\n\torl\ttmod,#0x00'"${SIDETONE_OK}"
  "r5_mov_tmod|sidetone.c|"$'\tmov\t_TMOD,#0x03'"${SIDETONE_OK}"
  "accept_auxr_orl_anl|sidetone.c|"$'\tanl\t_AUXR,#0x7f\n\torl\t0x8e,#0x00'"${SIDETONE_OK}"
  "r5_mov_auxr|sidetone.c|"$'\tmov\t_AUXR,#0x02'"${SIDETONE_OK}"
  "accept_p1_bits_and_reads|sidetone.c|"$'\tsetb\t_P1_1\n\tclr\t0x92\n\tmov\t_P1_2,c\n\tmov\tc,_P1_0\n\tmov\ta,_TMOD'"${SIDETONE_OK}"
  "accept_p5_orl_anl|keyer.c|"$'\torl\t_P5,#0x20\n\tanl\tp5,#0xdf'
  "r5_mov_p5|keyer.c|"$'\tmov\t_P5,#0xff'
  "accept_p5m1_orl_anl|keyer.c|"$'\torl\t_P5M1,#0x20\n\tanl\t0xc9,#0xff'
  "r5_mov_p5m1|keyer.c|"$'\tmov\t_P5M1,#0x20'
  "accept_p5m0_orl_anl|keyer.c|"$'\tanl\t_P5M0,#0xdf\n\torl\tp5m0,#0x00'
  "r5_mov_p5m0|keyer.c|"$'\tmov\t0xca,a'
  "accept_p5_5_bit_reads|sidetone.c|"$'_P5_5\t=\t0x00cd\n\tmov\tc,_P5_5\n\tjb\t_P5_5,00101$\n\tjnb\t0xcd,00102$'"${SIDETONE_OK}"
  "accept_isr_three_calls|sidetone.c|${SERVICES}${ISR_OK}"
  "r6_isr_two_calls|sidetone.c|${SERVICES}${ISR_HEAD}"$'\n\tlcall\t_keyer_service\n\tlcall\t_sidetone_service'"${ISR_TAIL}"
  "r6_isr_wrong_order|sidetone.c|${SERVICES}${ISR_HEAD}"$'\n\tlcall\t_player_service\n\tlcall\t_keyer_service\n\tlcall\t_sidetone_service'"${ISR_TAIL}"
  "r6_isr_extra_call|sidetone.c|${SERVICES}${ISR_HEAD}"$'\n\tlcall\t_keyer_service\n\tlcall\t_player_service\n\tlcall\t_sidetone_service\n\tlcall\t_tick_service'"${ISR_TAIL}"
  "r6_isr_acall_extra|sidetone.c|${SERVICES}${ISR_HEAD}"$'\n\tlcall\t_keyer_service\n\tacall\t_keyer_service\n\tlcall\t_player_service\n\tlcall\t_sidetone_service'"${ISR_TAIL}"
  "r6_isr_tail_jump|sidetone.c|${SERVICES}${ISR_HEAD}"$'\n\tlcall\t_keyer_service\n\tlcall\t_player_service\n\tlcall\t_sidetone_service\n\tljmp\t_x'"${ISR_TAIL}"
  "r6_isr_no_reti|sidetone.c|${SERVICES}${ISR_HEAD}"$'\n\tlcall\t_keyer_service\n\tlcall\t_player_service\n\tlcall\t_sidetone_service\n\tret'
  "r6_isr_missing|sidetone.c|${SERVICES}"
  "r6_service_calls|sidetone.c|"$'_keyer_service:\n\tret\n_player_service:\n\tlcall\t__mulint\n\tret\n_sidetone_service:\n\tret'"${ISR_OK}"
  "r6_service_tail_jump|sidetone.c|"$'_keyer_service:\n\tret\n_player_service:\n\tret\n_sidetone_service:\n\tsjmp\t_sidetone_poll'"${ISR_OK}"
  "r6_keyer_service_calls|keyer.c|"$'_keyer_start:\n\tlcall\t_keyer_stop\n\tret\n_keyer_service:\n\tacall\t_keyer_read_event\n\tret'
  "accept_other_functions_call|keyer.c|"$'_keyer_start:\n\tlcall\t_keyer_stop\n\tljmp\t_cw_timing_compute\n_keyer_service:\n\tret\n_keyer_stop::\n\tlcall\t_sidetone_set_on\n\tret'
  "r7_sdcc_atomic|main.c|"$'\tlcall\tsdcc_atomic_maybe_rollback'
  "r7_sdcc_atomic_upper_case_comment|lcd.c|"$'\tnop\t; SDCC_ATOMIC_MAYBE_ROLLBACK'
  "accept_p5_5_bit_writes|sidetone.c|"$'_P5_5\t=\t0x00cd\n\tsetb\t0xcd\n\tclr\t_P5_5\n\tmov\ta,_P5'"${SIDETONE_OK}"
)

self_test() {
  local fixtures="$1" is_fixtures_required="$2"
  local work failures=0 case_dir entry name module text expected
  case_count=0
  work="$(mktemp -d)"
  self_test_log="${work}/case.log"

  for entry in "${BUILTIN_CASES[@]}"; do
    name="${entry%%|*}"
    module="${entry#*|}"
    text="${module#*|}"
    module="${module%%|*}"
    write_case "${work}/${name}" "${module}" "${text}"
    expected=1
    [[ "${name}" == accept_* ]] && expected=0
    expect_status "${expected}" "${work}/${name}" "builtin/${name}" || failures=$((failures + 1))
  done

  if [[ -d "${fixtures}" ]]; then
    for case_dir in "${fixtures}"/reject/*/; do
      [[ -d "${case_dir}" ]] || continue
      expect_status 1 "${case_dir%/}" "reject/$(basename "${case_dir}")" || failures=$((failures + 1))
    done
    for case_dir in "${fixtures}"/accept/*/; do
      [[ -d "${case_dir}" ]] || continue
      expect_status 0 "${case_dir%/}" "accept/$(basename "${case_dir}")" || failures=$((failures + 1))
    done
  elif [[ "${is_fixtures_required}" == "true" ]]; then
    echo "check_asm self-test: fixture directory ${fixtures} not found" >&2
    rm -rf "${work}"
    return 2
  else
    echo "check_asm self-test: no fixtures at ${fixtures}, built-in cases only"
  fi

  rm -rf "${work}"
  if [[ "${failures}" -ne 0 ]]; then
    echo "check_asm self-test: ${failures} case(s) failed, ${case_count} passed" >&2
    return 1
  fi
  echo "check_asm self-test: all ${case_count} cases passed"
}

if [[ "${1:-}" == "--self-test" ]]; then
  if [[ $# -ge 2 ]]; then
    self_test "$2" true
  else
    self_test "${default_fixtures}" false
  fi
elif [[ $# -eq 1 && -d "$1" ]]; then
  scan_root "$1"
  echo "check_asm: no violations in $1"
else
  echo "usage: check_asm.sh <asm-root> | --self-test [<fixtures-dir>]" >&2
  exit 2
fi
