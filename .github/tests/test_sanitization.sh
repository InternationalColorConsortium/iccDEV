#!/bin/bash
###############################################################################
# Copyright (c) David H Hoyt LLC
#
# Last Updated:  16-DEC-2025-2025 1400Z by David Hoyt
#
# Intent: test_sanitization.sh
#
#
#
###############################################################################

set -euo pipefail

# Source the canonical sanitizer
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SANITIZER="$SCRIPT_DIR/../scripts/sanitize-sed.sh"
if [ -r "$SANITIZER" ]; then
  # shellcheck disable=SC1091
  source "$SANITIZER"
else
  echo "ERROR: Cannot find sanitize-sed.sh at $SANITIZER" >&2
  exit 1
fi

echo "=========================================="
echo "Testing Sanitization Functions"
echo "=========================================="
echo ""

pass=0
fail=0

run_test() {
  local test_name="$1"
  local input="$2"
  local expected="$3"
  local func="${4:-sanitize_line}"
  
  echo "Test $((pass + fail + 1)): $test_name"
  echo "  Input:    $input"
  echo "  Expected: $expected"
  
  local result
  result=$("$func" "$input")
  echo "  Result:   $result"
  
  if [ "$result" = "$expected" ]; then
    echo "  ✅ PASS"
    pass=$((pass + 1))
  else
    echo "  ❌ FAIL"
    fail=$((fail + 1))
  fi
  echo ""
}

# =============================================================================
# HTML Entity Escaping Tests
# =============================================================================

run_test "Basic XSS payload" \
  "<script>alert('xss')</script>" \
  "&lt;script&gt;alert(&#39;xss&#39;)&lt;/script&gt;"

run_test "All HTML special chars" \
  "A&B <tag> \"quoted\" 'single'" \
  "A&amp;B &lt;tag&gt; &quot;quoted&quot; &#39;single&#39;"

run_test "Realistic cppcheck output" \
  "[IccTagBasic.cpp:42]: (warning) Member variable 'IccTag::m_nReserved' is not initialized." \
  "[IccTagBasic.cpp:42]: (warning) Member variable &#39;IccTag::m_nReserved&#39; is not initialized."

run_test "Empty string" \
  "" \
  ""

run_test "Normal text (no special chars)" \
  "Normal text with numbers 123 and letters abc" \
  "Normal text with numbers 123 and letters abc"

# =============================================================================
# Unicode and Charset Tests
# =============================================================================

run_test "Unicode characters (UTF-8)" \
  "Hello 世界 مرحبا 🌍" \
  "Hello 世界 مرحبا 🌍"

run_test "Unicode with HTML entities" \
  "<div>Hello 世界 & 'test'</div>" \
  "&lt;div&gt;Hello 世界 &amp; &#39;test&#39;&lt;/div&gt;"

run_test "Emoji and special symbols" \
  "✅ PASS ❌ FAIL 🔒 Security" \
  "✅ PASS ❌ FAIL 🔒 Security"

run_test "Mixed RTL/LTR text with entities" \
  "English & العربية <tag>" \
  "English &amp; العربية &lt;tag&gt;"

run_test "Zero-width characters (stripped by v3)" \
  "$(printf 'test\xe2\x80\x8bzero\xe2\x80\x8cwidth\xe2\x80\x8bchars')" \
  "testzerowidthchars"

# =============================================================================
# Control Character and Injection Tests
# =============================================================================

run_test "Null bytes removed" \
  "$(printf 'test\x00null\x00byte')" \
  "testnullbyte"

run_test "Carriage return removed" \
  "$(printf 'line1\r\nline2')" \
  "line1 line2"

run_test "Tab normalized to space" \
  "$(printf 'line1\tTab\nLine2')" \
  "line1Tab Line2"

run_test "ANSI escape sequences (CSI)" \
  "$(printf '\033[31mRed\033[0m Text')" \
  "Red Text"

run_test "Bell and other control chars" \
  "$(printf 'test\007bell\010backspace')" \
  "testbellbackspace"

# =============================================================================
# Homograph and Lookalike Attack Tests
# =============================================================================

run_test "Cyrillic lookalikes" \
  "Аdmin (Cyrillic A)" \
  "Аdmin (Cyrillic A)"

run_test "Mathematical bold/italic (preserved)" \
  "𝐇𝐞𝐥𝐥𝐨 𝑾𝒐𝒓𝒍𝒅" \
  "𝐇𝐞𝐥𝐥𝐨 𝑾𝒐𝒓𝒍𝒅"

# =============================================================================
# Truncation and Length Tests
# =============================================================================

# Test truncation with very long input
long_input=$(printf 'A%.0s' {1..2000})
echo "Test $((pass + fail + 1)): Long input truncation (2000 chars)"
result=$(sanitize_line "$long_input")
result_len=${#result}
echo "  Input length: 2000"
echo "  Result length: $result_len"
echo "  Max allowed: $SANITIZE_LINE_MAXLEN"
if [ "$result_len" -le "$SANITIZE_LINE_MAXLEN" ]; then
  echo "  ✅ PASS (truncated correctly)"
  pass=$((pass + 1))
else
  echo "  ❌ FAIL (not truncated)"
  fail=$((fail + 1))
fi
echo ""

# =============================================================================
# XSS and Injection Vector Tests
# =============================================================================

run_test "HTML event handler injection" \
  "<img src=x onerror='alert(1)'>" \
  "&lt;img src=x onerror=&#39;alert(1)&#39;&gt;"

run_test "JavaScript protocol" \
  "javascript:alert('xss')" \
  "javascript:alert(&#39;xss&#39;)"

run_test "Data URI with base64" \
  "data:text/html;base64,PHNjcmlwdD5hbGVydCgxKTwvc2NyaXB0Pg==" \
  "data:text/html;base64,PHNjcmlwdD5hbGVydCgxKTwvc2NyaXB0Pg=="

run_test "SVG with script" \
  "<svg onload=alert(1)>" \
  "&lt;svg onload=alert(1)&gt;"

run_test "Markdown injection attempt" \
  "[Click me](javascript:alert('xss'))" \
  "[Click me](javascript:alert(&#39;xss&#39;))"

run_test "HTML comment injection" \
  "<!-- <script>alert(1)</script> -->" \
  "&lt;!-- &lt;script&gt;alert(1)&lt;/script&gt; --&gt;"

run_test "CSS expression injection" \
  "style='expression(alert(1))'" \
  "style=&#39;expression(alert(1))&#39;"

run_test "XML entity expansion attempt" \
  "<!ENTITY xxe SYSTEM \"file:///etc/passwd\">" \
  "&lt;!ENTITY xxe SYSTEM &quot;file:///etc/passwd&quot;&gt;"

run_test "CDATA section" \
  "<![CDATA[<script>alert(1)</script>]]>" \
  "&lt;![CDATA[&lt;script&gt;alert(1)&lt;/script&gt;]]&gt;"

run_test "Server-side template injection" \
  '{{7*7}} ${7*7} <%= 7*7 %>' \
  '{{7*7}} ${7*7} &lt;%= 7*7 %&gt;'

run_test "Path traversal in filename" \
  "../../../etc/shadow" \
  ".._.._.._etc_shadow" \
  "sanitize_filename"

run_test "Windows path traversal" \
  "..\\..\\windows\\system32" \
  "..-..-windows-system32" \
  "sanitize_filename"

run_test "Command injection attempt" \
  "test; rm -rf / #" \
  "test; rm -rf / #"

run_test "SQL injection pattern" \
  "' OR '1'='1" \
  "&#39; OR &#39;1&#39;=&#39;1"

run_test "LDAP injection pattern" \
  "*()|&" \
  "*()|&amp;"

# =============================================================================
# Edge Cases
# =============================================================================

run_test "Multiple consecutive entities" \
  "&&&&<<<<>>>>" \
  "&amp;&amp;&amp;&amp;&lt;&lt;&lt;&lt;&gt;&gt;&gt;&gt;"

run_test "Already encoded entities (double encoding)" \
  "&lt;script&gt;" \
  "&amp;lt;script&amp;gt;"

run_test "Mixed quotes" \
  "test \"'\" nested" \
  "test &quot;&#39;&quot; nested"

run_test "Backslash and special chars" \
  "C:\\Windows\\System32 & 'cmd'" \
  "C:\\Windows\\System32 &amp; &#39;cmd&#39;"

# =============================================================================
# sanitize_print Multi-line Tests
# =============================================================================

echo "Test $((pass + fail + 1)): Multi-line with sanitize_print"
multiline_input="Line 1: <error> & 'quote'
Line 2: \"test\"
Line 3: Normal"
result=$(sanitize_print "$multiline_input")
if echo "$result" | grep -q "&lt;error&gt;" && \
   echo "$result" | grep -q "&amp;" && \
   echo "$result" | grep -q "&#39;"; then
  echo "  ✅ PASS (contains expected entities)"
  pass=$((pass + 1))
else
  echo "  ❌ FAIL"
  fail=$((fail + 1))
fi
echo ""

# =============================================================================
# sanitize_ref and sanitize_filename Tests
# =============================================================================

run_test "Ref sanitization: branch name" \
  "feature/test-123" \
  "feature/test-123" \
  "sanitize_ref"

run_test "Ref sanitization: dangerous chars replaced" \
  "feature\$test<>|branch" \
  "feature-test-branch" \
  "sanitize_ref"

run_test "Filename sanitization: slashes to underscores" \
  "../../etc/passwd" \
  ".._.._etc_passwd" \
  "sanitize_filename"

# =============================================================================
# Unicode Attack Pattern Tests (v3 — Trojan Source / Supply Chain)
# =============================================================================

# Pattern 1: Bidi override (Trojan Source attack)
run_test "Bidi override stripped (U+202E RLO)" \
  "$(printf 'safe\xe2\x80\xaeevil\xe2\x80\xactext')" \
  "safeeviltext"

run_test "Bidi embedding stripped (U+202A LRE)" \
  "$(printf 'normal\xe2\x80\xaaembedded\xe2\x80\xactext')" \
  "normalembeddedtext"

run_test "Bidi isolate stripped (U+2066 LRI)" \
  "$(printf 'left\xe2\x81\xa6isolated\xe2\x81\xa9right')" \
  "leftisolatedright"

# Pattern 2: Zero-width / invisible characters
run_test "Zero-width space stripped (U+200B)" \
  "$(printf 'is\xe2\x80\x8bAdmin')" \
  "isAdmin"

run_test "Zero-width joiner stripped (U+200D)" \
  "$(printf 'run\xe2\x80\x8dcommand')" \
  "runcommand"

run_test "Zero-width non-joiner stripped (U+200C)" \
  "$(printf 'pass\xe2\x80\x8cword')" \
  "password"

run_test "BOM stripped (U+FEFF)" \
  "$(printf '\xef\xbb\xbfmalicious')" \
  "malicious"

run_test "Word joiner stripped (U+2060)" \
  "$(printf 'test\xe2\x81\xa0value')" \
  "testvalue"

# Pattern 2 with sanitize_ref (ASCII allowlist)
run_test "Ref: bidi override replaced with dash" \
  "$(printf 'safe\xe2\x80\xaeevil\xe2\x80\xactext')" \
  "safe-evil-text" \
  "sanitize_ref"

run_test "Ref: ZWS replaced with dash" \
  "$(printf 'is\xe2\x80\x8bAdmin')" \
  "is-Admin" \
  "sanitize_ref"

run_test "Ref: BOM stripped from ref" \
  "$(printf '\xef\xbb\xbfmalicious')" \
  "malicious" \
  "sanitize_ref"

# Pattern 3: Homoglyph (cannot auto-strip — valid Unicode letters)
# sanitize_ref replaces non-ASCII with dash; sanitize_line preserves them
run_test "Ref: Cyrillic homoglyph replaced" \
  "$(printf 'micr\xd0\xbesoft')" \
  "micr-soft" \
  "sanitize_ref"

# Interlinear annotation attack
run_test "Interlinear annotation stripped (U+FFF9-FFFB)" \
  "$(printf 'test\xef\xbf\xb9hidden\xef\xbf\xbbvisible')" \
  "testhiddenvisible"

# Combined: bidi + XSS
run_test "Bidi + XSS combined attack" \
  "$(printf '\xe2\x80\xae<script>alert(1)</script>\xe2\x80\xac')" \
  "&lt;script&gt;alert(1)&lt;/script&gt;"

# ANSI escape stripping
run_test "ANSI CSI color codes stripped" \
  "$(printf '\x1b[31mERROR\x1b[0m: test')" \
  "ERROR: test"

run_test "ANSI OSC sequences stripped" \
  "$(printf '\x1b]0;evil title\x07real output')" \
  "real output"

# Sanitizer version
run_test "Sanitizer version is v3" \
  "$(sanitizer_version)" \
  "iccDEV-sanitizer-v3"

# =============================================================================
# Results Summary
# =============================================================================

echo "=========================================="
echo "Results: $pass passed, $fail failed"
echo "=========================================="

if [ $fail -eq 0 ]; then
  echo "✅ All tests PASSED"
  exit 0
else
  echo "❌ Some tests FAILED"
  exit 1
fi

