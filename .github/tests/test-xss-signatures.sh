#!/usr/bin/env bash
###############################################################
#
## Copyright (c) 2024-2025 David H Hoyt. All rights reserved.
##                 https://srd.cx
##
## Last Updated:  16-DEC-2025-2025 1400Z by David Hoyt
#
## Intent:test-xss-signatures.sh
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
# shellcheck source=.github/scripts/sanitize-sed.sh
# shellcheck disable=SC1091
source "${SCRIPT_DIR}/../scripts/sanitize-sed.sh"

# Disable errexit for test execution (we want to continue on test failures)
set +e

# Colors for output (if tty)
if [[ -t 1 ]]; then
  RED='\033[0;31m'
  GREEN='\033[0;32m'
  NC='\033[0m' # No Color
else
  RED=''
  GREEN=''
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
  printf "${GREEN}[PASS]${NC}: %s\n" "$name"
}

test_failed() {
  local name="$1"
  local reason="$2"
  ((FAILED_TESTS++))
  ((TOTAL_TESTS++))
  printf "${RED}[FAIL]${NC}: %s\n  Reason: %s\n" "$name" "$reason"
}

# Assert that output does NOT contain dangerous patterns
assert_safe_html() {
  local test_name="$1"
  local output="$2"

  # The key insight: if < and > are properly escaped to &lt; and &gt;,
  # then the content is safe even if it contains strings like "onerror="
  # because the browser won't interpret it as HTML.
  #
  # We check for UNESCAPED tags - actual < and > characters that would
  # allow browser interpretation.

  # Check for unescaped < followed by tag names
  if echo "$output" | grep -q '<script' || echo "$output" | grep -q '</script'; then
    test_failed "$test_name" "Contains unescaped <script> tag"
    return 1
  fi

  if echo "$output" | grep -q '<img '; then
    test_failed "$test_name" "Contains unescaped <img> tag"
    return 1
  fi

  if echo "$output" | grep -q '<iframe '; then
    test_failed "$test_name" "Contains unescaped <iframe> tag"
    return 1
  fi

  if echo "$output" | grep -q '<svg '; then
    test_failed "$test_name" "Contains unescaped <svg> tag"
    return 1
  fi

  # Check for javascript: protocol in an actual href attribute (unescaped)
  # If it's escaped as &lt;...&gt; then it's safe
  if echo "$output" | grep -Eq '<[^>]*href=['\''"]?javascript:'; then
    test_failed "$test_name" "Contains unescaped javascript: in href"
    return 1
  fi

  # Check for unescaped event handlers in actual tag attributes
  # Pattern: < tag ... on[event]= ... >
  if echo "$output" | grep -Eq '<[^>]*on[a-z]+=['\''"]?[a-zA-Z(]'; then
    test_failed "$test_name" "Contains unescaped event handler in tag"
    return 1
  fi

  test_passed "$test_name"
  return 0
}

# Assert that output properly escapes HTML entities
assert_html_escaped() {
  local test_name="$1"
  local input="$2"
  local output="$3"

  # If input contains <, output should contain &lt;
  if [[ "$input" == *"<"* ]] && [[ "$output" != *"&lt;"* ]]; then
    test_failed "$test_name" "< not escaped to &lt;"
    return 1
  fi

  # If input contains >, output should contain &gt;
  if [[ "$input" == *">"* ]] && [[ "$output" != *"&gt;"* ]]; then
    test_failed "$test_name" "> not escaped to &gt;"
    return 1
  fi

  # If input contains &, output should escape it
  if [[ "$input" == *"&"* ]] && [[ "$output" != *"&amp;"* ]]; then
    test_failed "$test_name" "& not escaped to &amp;"
    return 1
  fi

  test_passed "$test_name"
  return 0
}

###############################################################
# Test Suite 1: Basic XSS vectors from no-experience-required
###############################################################

echo ""
echo "========================================="
echo "Test Suite 1: Basic XSS Attack Vectors"
echo "========================================="
echo ""

# Test 1: Simple script tag
input='<script>alert(1)</script>'
output=$(sanitize_line "$input")
assert_safe_html "Test 1.1: Simple script tag" "$output"
assert_html_escaped "Test 1.2: Script tag escaped" "$input" "$output"

# Test 2: IMG with onerror
input='<img src=x onerror=alert(1)>'
output=$(sanitize_line "$input")
assert_safe_html "Test 2.1: IMG onerror" "$output"
assert_html_escaped "Test 2.2: IMG tag escaped" "$input" "$output"

# Test 3: SVG with onload
input='<svg onload=alert(1)>'
output=$(sanitize_line "$input")
assert_safe_html "Test 3.1: SVG onload" "$output"
assert_html_escaped "Test 3.2: SVG tag escaped" "$input" "$output"

# Test 4: Iframe with javascript: protocol
input='<iframe src="javascript:alert(1)"></iframe>'
output=$(sanitize_line "$input")
assert_safe_html "Test 4.1: Iframe javascript:" "$output"
assert_html_escaped "Test 4.2: Iframe escaped" "$input" "$output"

# Test 5: Anchor with javascript: href
input='<a href="javascript:alert(1)">Click</a>'
output=$(sanitize_line "$input")
assert_safe_html "Test 5.1: Anchor javascript:" "$output"
assert_html_escaped "Test 5.2: Anchor escaped" "$input" "$output"

# Test 6: Input with autofocus and onfocus
input='<input autofocus onfocus=alert(1)>'
output=$(sanitize_line "$input")
assert_safe_html "Test 6: Input autofocus" "$output"

# Test 7: Video with onerror
input='<video src=x onerror=alert(1)>'
output=$(sanitize_line "$input")
assert_safe_html "Test 7: Video onerror" "$output"

# Test 8: Body with onload
input='<body onload=alert(1)>'
output=$(sanitize_line "$input")
assert_safe_html "Test 8: Body onload" "$output"

# Test 9: Details with ontoggle
input='<details open ontoggle=alert(1)>'
output=$(sanitize_line "$input")
assert_safe_html "Test 9: Details ontoggle" "$output"

# Test 10: Marquee with onstart
input='<marquee onstart=alert(1)>'
output=$(sanitize_line "$input")
assert_safe_html "Test 10: Marquee onstart" "$output"

###############################################################
# Test Suite 2: Encoded and Obfuscated XSS
###############################################################

echo ""
echo "========================================="
echo "Test Suite 2: Encoded/Obfuscated XSS"
echo "========================================="
echo ""

# Test 11: HTML entity encoded script
input='&#60;script&#62;alert(1)&#60;/script&#62;'
output=$(sanitize_line "$input")
assert_safe_html "Test 11: HTML entities" "$output"

# Test 12: Unicode escapes
input='<script>\u0061\u006C\u0065\u0072\u0074(1)</script>'
output=$(sanitize_line "$input")
assert_safe_html "Test 12: Unicode escapes" "$output"

# Test 13: Hex encoding in href
input='<a href="&#x6A&#x61&#x76&#x61&#x73&#x63&#x72&#x69&#x70&#x74&#x3A&#x61&#x6C&#x65&#x72&#x74&#x28&#x31&#x29">Click</a>'
output=$(sanitize_line "$input")
assert_safe_html "Test 13: Hex encoded href" "$output"

# Test 14: Base64 data URL
input='<iframe src="data:text/html;base64,PHNjcmlwdD5hbGVydCgxKTwvc2NyaXB0Pg=="></iframe>'
output=$(sanitize_line "$input")
assert_safe_html "Test 14: Base64 data URL" "$output"

# Test 15: Mixed case to bypass filters
input='<ScRiPt>alert(1)</sCrIpT>'
output=$(sanitize_line "$input")
assert_safe_html "Test 15: Mixed case" "$output"

###############################################################
# Test Suite 3: Multi-line XSS attacks
###############################################################

echo ""
echo "========================================="
echo "Test Suite 3: Multi-line XSS Attacks"
echo "========================================="
echo ""

# Test 16: Multi-line script injection
input=$'Line 1\n<script>\nalert(1)\n</script>\nLine 2'
output=$(sanitize_print "$input")
assert_safe_html "Test 16: Multi-line script" "$output"

# Test 17: Comment-based XSS
input=$'<!-- comment --><script>alert(1)</script><!-- /comment -->'
output=$(sanitize_print "$input")
assert_safe_html "Test 17: Comment-based XSS" "$output"

# Test 18: Multiple XSS vectors in one payload
input=$'<img src=x onerror=alert(1)>\n<svg onload=alert(2)>\n<iframe src=javascript:alert(3)>'
output=$(sanitize_print "$input")
assert_safe_html "Test 18: Multiple vectors" "$output"

###############################################################
# Test Suite 4: Control characters and special cases
###############################################################

echo ""
echo "========================================="
echo "Test Suite 4: Control Characters"
echo "========================================="
echo ""

# Test 19: NULL byte injection
input=$'<script\x00>alert(1)</script>'
output=$(sanitize_line "$input")
assert_safe_html "Test 19: NULL byte" "$output"

# Test 20: Carriage return injection
input=$'<script\r>alert(1)</script>'
output=$(sanitize_line "$input")
assert_safe_html "Test 20: CR injection" "$output"

# Test 21: Tab characters
input=$'<img\tsrc=x\tonerror=alert(1)>'
output=$(sanitize_line "$input")
assert_safe_html "Test 21: Tab characters" "$output"

# Test 22: Newline in attribute
input=$'<img src=x onerror=\nalert(1)>'
output=$(sanitize_line "$input")
assert_safe_html "Test 22: Newline in attr" "$output"

###############################################################
# Test Suite 5: Edge cases and boundary conditions
###############################################################

echo ""
echo "========================================="
echo "Test Suite 5: Edge Cases"
echo "========================================="
echo ""

# Test 23: Empty input
input=''
output=$(sanitize_line "$input")
if [[ -z "$output" ]]; then
  test_passed "Test 23: Empty input"
else
  test_failed "Test 23: Empty input" "Output not empty: '$output'"
fi

# Test 24: Very long XSS payload
input=$(printf '<script>alert(%d)</script>' {1..100})
output=$(sanitize_line "$input")
assert_safe_html "Test 24: Long payload" "$output"

# Test 25: Special characters
input='& < > " '"'"' / \ ='
output=$(sanitize_line "$input")
assert_html_escaped "Test 25: Special chars" "$input" "$output"

# Test 26: Nested tags
input='<div><script>alert(1)</script></div>'
output=$(sanitize_line "$input")
assert_safe_html "Test 26: Nested tags" "$output"

# Test 27: Self-closing tags
input='<img src=x onerror=alert(1)/>'
output=$(sanitize_line "$input")
assert_safe_html "Test 27: Self-closing" "$output"

# Test 28: Multiple spaces
input='<script    >   alert(1)   </script>'
output=$(sanitize_line "$input")
assert_safe_html "Test 28: Multiple spaces" "$output"

###############################################################
# Test Suite 6: Real-world XSS from signature file
###############################################################

echo ""
echo "========================================="
echo "Test Suite 6: Real-world Signatures"
echo "========================================="
echo ""

# Load some signatures from the file if it exists
SIGNATURE_FILE="${SCRIPT_DIR}/sanitizer-test-file.txt"

if [[ -f "$SIGNATURE_FILE" ]]; then
  echo "Loading signatures from file..."

  # Test first 50 signatures from file
  line_num=0
  while IFS= read -r line && [[ $line_num -lt 50 ]]; do
    ((line_num++))

    # Skip empty lines and comments
    [[ -z "$line" ]] && continue
    [[ "$line" =~ ^# ]] && continue

    # Extract the XSS payload (remove line numbers like "1. ")
    payload=$(echo "$line" | sed -E 's/^[0-9]+\.[[:space:]]*//')

    # Skip if still empty
    [[ -z "$payload" ]] && continue

    # Test with sanitize_line
    output=$(sanitize_line "$payload")
    assert_safe_html "Signature #$line_num (line)" "$output"

  done < "$SIGNATURE_FILE"

  echo "Tested $line_num signatures from file"
else
  echo "${YELLOW}Warning${NC}: Signature file not found at $SIGNATURE_FILE"
  echo "Skipping real-world signature tests"
fi

###############################################################
# Test Suite 7: sanitize_ref and sanitize_filename
###############################################################

echo ""
echo "========================================="
echo "Test Suite 7: Ref/Filename Sanitization"
echo "========================================="
echo ""

# Test ref sanitization
input='feature/PR#123:bug fix!!'
expected='feature/PR-123-bug-fix'
output=$(sanitize_ref "$input")
if [[ "$output" == "$expected" ]]; then
  test_passed "Test Ref 1: Sanitize ref"
else
  test_failed "Test Ref 1: Sanitize ref" "Expected '$expected', got '$output'"
fi

# Test filename sanitization (no slashes)
input='path/to/file.txt'
expected='path_to_file.txt'
output=$(sanitize_filename "$input")
if [[ "$output" == "$expected" ]]; then
  test_passed "Test Ref 2: Sanitize filename"
else
  test_failed "Test Ref 2: Sanitize filename" "Expected '$expected', got '$output'"
fi

# Test XSS in ref name
input='<script>alert(1)</script>'
output=$(sanitize_ref "$input")
assert_safe_html "Test Ref 3: XSS in ref" "$output"

###############################################################
# Summary
###############################################################

echo ""
echo "========================================="
echo "Test Summary"
echo "========================================="
echo ""
echo "Total tests: $TOTAL_TESTS"
echo "${GREEN}Passed: $PASSED_TESTS${NC}"
if [[ $FAILED_TESTS -gt 0 ]]; then
  echo "${RED}Failed: $FAILED_TESTS${NC}"
  exit 1
else
  echo "${GREEN}All tests passed!${NC}"
  exit 0
fi
