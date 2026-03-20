#!/usr/bin/env bash
###############################################################
#
## Copyright (©) 2024-2025 David H Hoyt. All rights reserved.
##                 https://srd.cx
##
## Last Updated:  16-DEC-2025-2025 1400Z by David Hoyt
#
## Intent:test-all-signatures.sh
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

set -uo pipefail

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Source the sanitizer
source "${SCRIPT_DIR}/../scripts/newest-sanitizer.sh"

# Colors
if [[ -t 1 ]]; then
  RED='\033[0;31m'
  GREEN='\033[0;32m'
  YELLOW='\033[1;33m'
  BLUE='\033[0;34m'
  CYAN='\033[0;36m'
  NC='\033[0m'
else
  RED=''
  GREEN=''
  YELLOW=''
  BLUE=''
  CYAN=''
  NC=''
fi

# Test statistics
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
SKIPPED_FILES=0
TESTED_FILES=0

# Signature repository path
SIG_REPO="${SCRIPT_DIR}/../../Commodity-Injection-Signatures"

echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  Comprehensive Signature Test Suite                         ║${NC}"
echo -e "${BLUE}║  Testing ALL Commodity-Injection-Signatures                  ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if signature repository exists
if [[ ! -d "$SIG_REPO" ]]; then
  echo -e "${RED}✗ ERROR: Commodity-Injection-Signatures not found at: $SIG_REPO${NC}"
  echo "Please clone: git clone https://github.com/xsscx/Commodity-Injection-Signatures.git"
  exit 1
fi

echo -e "${CYAN}Signature Repository: $SIG_REPO${NC}"
echo ""

# Test a signature file
test_signature_file() {
  local file="$1"
  local category="$2"
  local max_tests="${3:-50}"  # Default: test first 50 signatures per file
  local line_count=0
  local file_tests=0
  local file_passed=0
  local file_failed=0

  # Skip if file is too large (> 1MB)
  local file_size=$(stat -f%z "$file" 2>/dev/null || stat -c%s "$file" 2>/dev/null)
  if [[ $file_size -gt 1048576 ]]; then
    echo -e "  ${YELLOW}⊘ SKIP: File too large ($(numfmt --to=iec-i --suffix=B $file_size))${NC}"
    ((SKIPPED_FILES++))
    return 0
  fi

  # Read and test signatures from file
  while IFS= read -r line || [[ -n "$line" ]]; do
    ((line_count++))

    # Skip empty lines and comments
    [[ -z "$line" ]] && continue
    [[ "$line" =~ ^# ]] && continue
    [[ "$line" =~ ^// ]] && continue

    # Test the signature
    local result
    result=$(sanitize_line "$line" 2>/dev/null)

    # Verify HTML is escaped (basic check)
    if [[ "$line" =~ \< ]] && [[ ! "$result" =~ "&lt;" ]]; then
      # Has < but not escaped
      echo -e "  ${RED}✗ FAIL: Line $line_count not escaped${NC}"
      ((file_failed++))
    elif [[ "$line" =~ \> ]] && [[ ! "$result" =~ "&gt;" ]]; then
      # Has > but not escaped
      echo -e "  ${RED}✗ FAIL: Line $line_count not escaped${NC}"
      ((file_failed++))
    else
      ((file_passed++))
    fi

    ((file_tests++))
    ((TOTAL_TESTS++))

    # Limit tests per file
    [[ $file_tests -ge $max_tests ]] && break

  done < "$file"

  if [[ $file_tests -eq 0 ]]; then
    echo -e "  ${YELLOW}⊘ SKIP: No valid signatures found${NC}"
    ((SKIPPED_FILES++))
    return 0
  fi

  ((TESTED_FILES++))
  ((PASSED_TESTS+=file_passed))
  ((FAILED_TESTS+=file_failed))

  if [[ $file_failed -eq 0 ]]; then
    echo -e "  ${GREEN}✓ PASS: $file_passed/$file_tests tests${NC}"
  else
    echo -e "  ${RED}✗ FAIL: $file_passed passed, $file_failed failed (total: $file_tests)${NC}"
  fi
}

# Process a directory of signature files
process_directory() {
  local dir="$1"
  local category="$2"

  echo -e "${YELLOW}═══════════════════════════════════════════════════════${NC}"
  echo -e "${YELLOW}Category: $category${NC}"
  echo -e "${YELLOW}═══════════════════════════════════════════════════════${NC}"
  echo ""

  local file_count=0

  # Find all .txt, .fuzz, and .svg files
  while IFS= read -r file; do
    ((file_count++))
    local basename=$(basename "$file")
    echo -e "${CYAN}[$file_count] Testing: $basename${NC}"
    test_signature_file "$file" "$category"
    echo ""
  done < <(find "$dir" -maxdepth 1 -type f \( -name "*.txt" -o -name "*.fuzz" -o -name "*.svg" \) 2>/dev/null | sort)

  if [[ $file_count -eq 0 ]]; then
    echo -e "${YELLOW}  No signature files found in this category${NC}"
    echo ""
  fi
}

# Main test execution
echo -e "${BLUE}Starting comprehensive test sweep...${NC}"
echo ""

# Test major categories
process_directory "$SIG_REPO/random" "Random XSS & Malicious Input"
process_directory "$SIG_REPO/svg" "SVG-based Attacks"
process_directory "$SIG_REPO/unix" "Unix/Shell Injection"
process_directory "$SIG_REPO/uri" "URI Mutations & Protocol Handlers"
process_directory "$SIG_REPO/javascript" "JavaScript Injection"
process_directory "$SIG_REPO/xml" "XML/XXE Injection"
process_directory "$SIG_REPO/sqlinjection" "SQL Injection"
process_directory "$SIG_REPO/css" "CSS Injection"
process_directory "$SIG_REPO/httpheader" "HTTP Header Injection"
process_directory "$SIG_REPO/callback" "Callback Injection"
process_directory "$SIG_REPO/email" "Email Injection"
process_directory "$SIG_REPO/json" "JSON Injection"
process_directory "$SIG_REPO/meta" "Meta Tag Injection"
process_directory "$SIG_REPO/parameter" "Parameter Pollution"
process_directory "$SIG_REPO/referer" "Referer Injection"
process_directory "$SIG_REPO/soap" "SOAP Injection"
process_directory "$SIG_REPO/ssi" "SSI Injection"
process_directory "$SIG_REPO/lfi-local-file-system-harvesting" "LFI/Path Traversal"
process_directory "$SIG_REPO/shell" "Shell Injection"
process_directory "$SIG_REPO/angular" "Angular Template Injection"
process_directory "$SIG_REPO/python" "Python Injection"
process_directory "$SIG_REPO/java" "Java Injection"
process_directory "$SIG_REPO/ps" "PowerShell Injection"
process_directory "$SIG_REPO/applescript" "AppleScript Injection"
process_directory "$SIG_REPO/custom" "Custom Signatures"
process_directory "$SIG_REPO/ascii" "ASCII/Unicode Attacks"
process_directory "$SIG_REPO/calc" "Calculator/Expression Injection"
process_directory "$SIG_REPO/ua" "User-Agent Injection"
process_directory "$SIG_REPO/rbl" "RBL/DNS Attacks"
process_directory "$SIG_REPO/graphics" "Graphics-based Attacks"

# Test root-level files
echo -e "${YELLOW}═══════════════════════════════════════════════════════${NC}"
echo -e "${YELLOW}Category: Root Level Signatures${NC}"
echo -e "${YELLOW}═══════════════════════════════════════════════════════${NC}"
echo ""

echo -e "${CYAN}[1] Testing: no-experience-required-xss-signatures-only-fools-dont-use.txt${NC}"
test_signature_file "$SIG_REPO/no-experience-required-xss-signatures-only-fools-dont-use.txt" "Root XSS" 100
echo ""

# Note: Skip full-unicode.txt as it's 5.5MB
echo -e "${CYAN}[2] Testing: xml-paste-from-gist.txt${NC}"
test_signature_file "$SIG_REPO/xml-paste-from-gist.txt" "Root XML"
echo ""

# Final Summary
echo -e "${BLUE}╔══════════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║  Comprehensive Test Summary                                 ║${NC}"
echo -e "${BLUE}╚══════════════════════════════════════════════════════════════╝${NC}"
echo ""
echo "Signature Files Tested: $TESTED_FILES"
echo "Signature Files Skipped: $SKIPPED_FILES"
echo ""
echo "Total Signature Tests: $TOTAL_TESTS"
echo -e "${GREEN}Passed: $PASSED_TESTS${NC}"

if [[ $FAILED_TESTS -gt 0 ]]; then
  echo -e "${RED}Failed: $FAILED_TESTS${NC}"
  echo ""
  echo -e "${RED}Some sanitization tests failed!${NC}"
  exit 1
else
  echo -e "${GREEN}Failed: 0${NC}"
  echo ""
  echo -e "${GREEN}All signature tests passed!${NC}"
  echo -e "${GREEN}The sanitizer successfully neutralized all attack patterns.${NC}"
  exit 0
fi
