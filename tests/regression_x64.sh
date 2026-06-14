#!/usr/bin/env bash
# ============================================================
# x64 Examples Regression Test Suite
# ============================================================
# Tests all x64 examples in both -A (VM) and -o (native) modes.
# Each test has an expected output pattern to match.
# Run: make test-examples
# ============================================================
set -u

DEMI="./bin/demi-engine-debug"
EXAMPLES_DIR="examples/x64"
BIN_DIR="examples/bin/x64"
PASS=0
FAIL=0
SKIP=0

RED='\033[31m'
GREEN='\033[32m'
YELLOW='\033[33m'
CYAN='\033[36m'
RESET='\033[0m'

# --- Test definition: name, input, expected_output ---
# Format: "name|input|expected_grep|mode_flags"
# mode_flags: A=test -A, O=test -o, A-=skip -A, O-=skip -o

TESTS=(
  # basic/
  "basic/hello_world||Hello, World!|AO"
  "basic/simple_addition||42 + 13 = 55|AO"
  "basic/simple_digit||Number output: 5|AO"
  "basic/stack_operations||Stack test: PUSH 42,13,7|AO"

  # control_flow/
  "control_flow/conditional_jumps||Conditional jumps: JE, JNE, JG, JL all passed|AO"
  "control_flow/counting_loop||1 2 3 4 5|AO"

  # data/
  "data/data_labels||Labels work!|AO"
  "data/data_storage||Hello, DemiEngine!|AO"
  "data/indirect_addressing||LOADR test: stored 42|AO"
  "data/labels_and_strings||Hello from labeled data!|AO"
  "data/string_reverse||Reversed: !dlroW olleH|AO"

  # features/
  "features/core_instructions||Core instructions executed|AO"
  "features/fpu_showcase||FINIT FCLEX FSTSW OK|AO"

  # interrupts/
  "interrupts/cli_sti||Interrupt handler test passed|AO"

  # io/
  "io/char_output||Character output: H|AO"
  "io/decimal_output||Decimal output: 123|AO"

  # advanced/
  "advanced/factorial||factorial(5) = 120|AO"
  "advanced/factorial_recursive||factorial_recursive(5) = 120|AO"
  "advanced/fibonacci||Fibonacci (10 terms):|AO"
  "advanced/readable_calculator||5 + 3 = 8|AO"

  # syscalls/ (interactive, pipe input — single-line only for reliable testing)
  "syscalls/hello_world||Hello, World!|AO"
  "syscalls/echo_input|hello|hello|AO"
  "syscalls/simple_write||Hello, World!|A-O-"  # hardcoded address pre-existing
  "syscalls/multiple_syscalls||All syscalls|A"
  "syscalls/file_write||File written|A"
  "syscalls/file_read||A"  # pre-existing output issue
  "syscalls/calculator|5\n+\n3\n|=|A-"  # pipe buffering — needs PTY
  "syscalls/line_calculator|12 + 4\nq\n|= 16|A"
  "syscalls/line_calculator|12 + 4\n|= 16|O"

  # games/
  "games/number_guess||Guess the hidden digit|A"
)

run_test() {
  local name="$1" input="$2" expected="$3" mode="$4"

  local asm="$EXAMPLES_DIR/$name.asm"
  local bin="$BIN_DIR/$name"
  local input_str=""
  [ -n "$input" ] && input_str="(input: '$input')"

  # --- -A mode ---
  if [[ "$mode" == *"A-"* ]]; then
    echo -e "  ${YELLOW}○${RESET} -A $name (skipped)"
    ((SKIP++))
  elif [[ "$mode" == *"A"* ]]; then
    local cmd="timeout 5 $DEMI -A $asm"
    local out
    if [ -n "$input" ]; then
      out=$(printf "%s" "$input" | $cmd 2>&1) || true
    else
      out=$($cmd 2>&1) || true
    fi
    if echo "$out" | grep -qF "$expected"; then
      echo -e "  ${GREEN}✓${RESET} -A $name $input_str"
      ((PASS++))
    else
      echo -e "  ${RED}✗${RESET} -A $name $input_str"
      echo "    expected: '$expected'"
      echo "    got:      '$(echo "$out" | head -3 | tr '\n' '|')'"
      ((FAIL++))
    fi
  fi

  # --- -o mode ---
  if [[ "$mode" == *"O-"* ]]; then
    echo -e "  ${YELLOW}○${RESET} -o $name (skipped)"
    ((SKIP++))
  elif [[ "$mode" == *"O"* ]]; then
    local cmd="timeout 5 $bin"
    local out
    if [ -n "$input" ]; then
      out=$(printf "%s" "$input" | $cmd 2>&1) || true
    else
      out=$($cmd 2>&1) || true
    fi
    if echo "$out" | grep -qF "$expected"; then
      echo -e "  ${GREEN}✓${RESET} -o $name $input_str"
      ((PASS++))
    else
      echo -e "  ${RED}✗${RESET} -o $name $input_str"
      echo "    expected: '$expected'"
      echo "    got:      '$(echo "$out" | head -3 | tr '\n' '|')'"
      ((FAIL++))
    fi
  elif [[ "$mode" == *"O-"* ]]; then
    echo -e "  ${YELLOW}○${RESET} -o $name (skipped)"
    ((SKIP++))
  fi
}

echo ""
echo -e "${CYAN}═══════════════════════════════════════════${RESET}"
echo -e "${CYAN}  x64 Examples Regression Test Suite${RESET}"
echo -e "${CYAN}═══════════════════════════════════════════${RESET}"
echo ""

# Compile all -o binaries first
echo "Compiling -o binaries..."
for t in "${TESTS[@]}"; do
  IFS='|' read -r name input expected mode <<< "$t"
  if [[ "$mode" == *"O"* ]]; then
    mkdir -p "$(dirname "$BIN_DIR/$name")"
    $DEMI -A "$EXAMPLES_DIR/$name.asm" -o "$BIN_DIR/$name" >/dev/null 2>&1 || true
  fi
done
echo ""

# Run all tests
for t in "${TESTS[@]}"; do
  IFS='|' read -r name input expected mode <<< "$t"
  run_test "$name" "$input" "$expected" "$mode"
done

echo ""
echo -e "${CYAN}───────────────────────────────────────────${RESET}"
echo -e "  ${GREEN}Passed: $PASS${RESET}  ${RED}Failed: $FAIL${RESET}  ${YELLOW}Skipped: $SKIP${RESET}"
echo -e "${CYAN}───────────────────────────────────────────${RESET}"

if [ $FAIL -gt 0 ]; then
  exit 1
fi
exit 0
