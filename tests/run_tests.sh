#!/bin/bash

# run_tests.sh
# LVLang Comprehensive Bash Test Suite
# Tests major opcode groups based on lvlang.h

# Navigate to the directory containing this script
cd "$(dirname "$0")" || exit 1
LVLC="../lvlc"

PASSED=0
FAILED=0
TOTAL=0

# Ensure lvlc exists
if [ ! -f "$LVLC" ]; then
    echo "Error: $LVLC not found."
    exit 1
fi

run_test() {
    local name="$1"
    local expected="$2"
    local bytecode="$3"

    TOTAL=$((TOTAL + 1))
    
    # Run the lvlc program and capture the actual output lines properly.
    # We use awk to extract everything between "Output: " and "[VM Halted]".
    # Newlines are preserved as literal '\n' sequences in the string to make exact matching simple in bash.
    local raw_output
    raw_output=$("$LVLC" "$bytecode" 2>/dev/null | awk '/^Output: /{flag=1; sub(/^Output: /, ""); printf "%s", $0; next} /^\[VM Halted\]/{flag=0; next} flag{printf "\\n%s", $0}')

    if [ "$raw_output" = "$expected" ]; then
        echo -e "[\033[32mPASS\033[0m] $name"
        PASSED=$((PASSED + 1))
    else
        echo -e "[\033[31mFAIL\033[0m] $name"
        echo "       Expected: '$expected'"
        echo "       Got:      '$raw_output'"
        FAILED=$((FAILED + 1))
    fi
}

echo "=== Running LVLang Test Suite ==="

# Group 0x01: Stack operations
run_test "0x01 PUSH_IMM" "42" "01AA050105FF"
run_test "0x01 POP" "1" "018101820101050105FF"
run_test "0x01 DUP" "10" "018501020201050105FF"
run_test "0x01 SWAP" "5" "0185018A01030202050105FF"
run_test "0x01 PUSH_SHIFT_8" "256" "01810104050105FF"
run_test "0x01 PUSH_SHIFT_16" "65536" "01810105050105FF"
run_test "0x01 LOAD/STORE REG" "47" "01AA0130018501100201050105FF"
run_test "0x01 LOAD/STORE RAM" "99" "0181013201E301520142050105FF"

# Group 0x02: Integer math
run_test "0x02 ADD" "30" "018A01940201050105FF"
run_test "0x02 SUB" "10" "0194018A0202050105FF"
run_test "0x02 MUL" "12" "018301840203050105FF"
run_test "0x02 DIV" "4" "019401850204050105FF"
run_test "0x02 MOD" "2" "019401830205050105FF"
run_test "0x02 INC REG" "6" "0185013102110111050105FF"
run_test "0x02 DEC REG" "4" "0185013202220112050105FF"

# Group 0x03: Comparisons
run_test "0x03 EQ (True)" "1" "018501850301050105FF"
run_test "0x03 NEQ (False)" "0" "018501850302050105FF"
run_test "0x03 GT (True)" "1" "018601850303050105FF"
run_test "0x03 LT (True)" "1" "018501860304050105FF"

# Group 0x05: I/O
run_test "0x05 PRINT_NUM" "9" "0189050105FF"
run_test "0x05 PRINT_CHAR" "A" "01C1050205FF"
run_test "0x05 PRINT_STR" "HI" "05034849000005FF"
run_test "0x05 PRINT_NL" 'A\nB' "01C10502050401C2050205FF"
run_test "0x05 HALT" "" "05FF"

# Group 0x07: Macros
run_test "0x07 MACRO_CLEAR_REG" "0" "01AA013007400110050105FF"
run_test "0x07 DEF_MACRO & EXEC_MACRO" "5" "0770018505010400078005FF"

# Group 0x09: Jump backward
run_test "0x09 JUMP BACKWARD" "5" "0B030185050105FF0904"

# Group 0x0A: Float math
run_test "0x0A FADD (I2F/F2I)" "5" "01820A0501830A050A010A06050105FF"
run_test "0x0A FSUB" "3" "01850A0501820A050A020A06050105FF"
run_test "0x0A FMUL" "12" "01830A0501840A050A030A06050105FF"
run_test "0x0A FDIV" "4" "01940A0501850A050A040A06050105FF"
run_test "0x0A FSQRT" "3" "01890A050A080A06050105FF"
run_test "0x0A FABS" "5" "0180018502020A050A090A06050105FF"
run_test "0x0A PRINT_FLOAT" "2.000000" "01820A050A0705FF"

# Group 0x0B: Jump forward
run_test "0x0B JUMP FORWARD" "9" "0B02018505010189050105FF"

# Group 0x0D: Conditional jump forward (JZ)
run_test "0x0D JZ (Taken)" "9" "01800D02018505010189050105FF"
run_test "0x0D JZ (Not Taken)" "59" "01810D02018505010189050105FF"

# Group 0x0F: Conditional jump forward (JNZ)
run_test "0x0F JNZ (Taken)" "9" "01810F02018505010189050105FF"
run_test "0x0F JNZ (Not Taken)" "59" "01800F02018505010189050105FF"

echo "=== Summary ==="
echo "$PASSED/$TOTAL tests passed"

if [ "$FAILED" -gt 0 ]; then
    exit 1
fi
exit 0
