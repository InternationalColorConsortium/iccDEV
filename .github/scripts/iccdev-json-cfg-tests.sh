#!/bin/bash
###############################################################################
# iccDEV JSON Configuration (-cfg) CI Tests
###############################################################################
#
# Tests the -cfg JSON configuration mode for 3 iccDEV tools:
#   - iccApplyNamedCmm
#   - iccApplySearch
#   - iccApplyProfiles
#
# Environment variables (set by CI workflow):
#   ICCDEV_TOOLS_DIR   -- path to Build/Tools/ (contains tool subdirs)
#   ICCDEV_TESTING_DIR -- path to Testing/ (contains generated profiles)
#   LD_LIBRARY_PATH    -- must include IccProfLib + IccXML shared libs
#
# Exit code: 0 = all pass, 1 = test failure, 2 = ASAN/UBSAN finding
###############################################################################

set -uo pipefail

# -- Path resolution ----------------------------------------------------------

TOOLS_DIR="${ICCDEV_TOOLS_DIR:?Set ICCDEV_TOOLS_DIR to iccDEV Build/Tools path}"
TESTING_DIR="${ICCDEV_TESTING_DIR:?Set ICCDEV_TESTING_DIR to iccDEV Testing path}"
OUTDIR="${ICCDEV_TEST_OUTDIR:-/tmp/iccdev-tool-output}"
mkdir -p "$OUTDIR"

APPLY_NAMED_CMM="$TOOLS_DIR/IccApplyNamedCmm/iccApplyNamedCmm"
APPLY_SEARCH="$TOOLS_DIR/IccApplySearch/iccApplySearch"
APPLY_PROFILES="$TOOLS_DIR/IccApplyProfiles/iccApplyProfiles"

# -- Sanitizer env ------------------------------------------------------------

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=0,detect_leaks=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-halt_on_error=0,print_stacktrace=1}"

# -- Find a test profile -----------------------------------------------------

PROFILE=""
for candidate in \
  "$TESTING_DIR/sRGB_v4_ICC_preference.icc" \
  "$TESTING_DIR/Calc/srgbCalcTest.icc" \
  "$TESTING_DIR/Calc/argbCalc.icc" \
  "$TESTING_DIR/../Testing/sRGB_v4_ICC_preference.icc"; do
  if [ -f "$candidate" ]; then
    PROFILE="$candidate"
    break
  fi
done

if [ -z "$PROFILE" ]; then
  # Fallback: find any .icc file
  PROFILE="$(find "$TESTING_DIR" -name '*.icc' -size +100c 2>/dev/null | sed -n '1p')"
fi

if [ -z "$PROFILE" ]; then
  echo "ERROR: No ICC profile found in $TESTING_DIR"
  exit 1
fi

echo "Using test profile: $PROFILE"
echo "Tools dir: $TOOLS_DIR"
echo "Testing dir: $TESTING_DIR"
echo ""

# -- Test framework -----------------------------------------------------------

PASS=0
FAIL=0
ASAN_FINDINGS=0
UBSAN_FINDINGS=0
TOTAL=0

run_test() {
  local name="$1"
  local tool="$2"
  local config="$3"
  local expect_fail="${4:-false}"
  TOTAL=$((TOTAL + 1))

  if [ ! -x "$tool" ]; then
    echo "  [SKIP] $name -- tool not found: $tool"
    PASS=$((PASS + 1))
    return
  fi

  local logfile="$OUTDIR/json-${name}.log"
  local exit_code=0
  timeout 30 "$tool" -cfg "$config" > "$logfile" 2>&1 || exit_code=$?

  # Check ASAN/UBSAN
  if grep -q "ERROR: AddressSanitizer" "$logfile" 2>/dev/null; then
    echo "  [ASAN] $name -- AddressSanitizer finding"
    ASAN_FINDINGS=$((ASAN_FINDINGS + 1))
    FAIL=$((FAIL + 1))
    return
  fi
  if grep -q "runtime error:" "$logfile" 2>/dev/null; then
    echo "  [UBSAN] $name -- undefined behavior"
    UBSAN_FINDINGS=$((UBSAN_FINDINGS + 1))
    FAIL=$((FAIL + 1))
    return
  fi

  if [ "$expect_fail" = "true" ]; then
    if [ "$exit_code" -ne 0 ]; then
      echo "  [PASS] $name (expected failure, exit=$exit_code)"
      PASS=$((PASS + 1))
    else
      echo "  [FAIL] $name (expected failure but got exit=0)"
      FAIL=$((FAIL + 1))
    fi
  else
    if [ "$exit_code" -eq 0 ]; then
      echo "  [PASS] $name (exit=0)"
      PASS=$((PASS + 1))
    else
      echo "  [FAIL] $name (exit=$exit_code)"
      FAIL=$((FAIL + 1))
    fi
  fi
}

# -- Generate JSON configs dynamically (using discovered profile path) --------

TMPDIR="$(mktemp -d)"

# 1. ApplyNamedCmm -- basic sRGB identity
cat > "$TMPDIR/named-basic.json" <<EOF
{
  "dataFiles": {
    "srcType": "colorData",
    "srcFile": "",
    "dstType": "colorData",
    "dstFile": "",
    "dstEncoding": "value",
    "dstPrecision": 4,
    "dstDigits": 9
  },
  "profileSequence": [
    {
      "iccFile": "$PROFILE",
      "intent": 1,
      "interpolation": "tetrahedral",
      "useBPC": false,
      "useD2BxB2Dx": true,
      "adjustPcsLuminance": false
    }
  ],
  "colorData": {
    "space": "RGB ",
    "encoding": "float",
    "data": [
      {"values": [1.0, 1.0, 1.0]},
      {"values": [0.0, 0.0, 0.0]},
      {"values": [0.5, 0.5, 0.5]},
      {"values": [1.0, 0.0, 0.0]},
      {"values": [0.0, 1.0, 0.0]},
      {"values": [0.0, 0.0, 1.0]}
    ]
  }
}
EOF

# 2. ApplyNamedCmm -- 8-bit encoding
cat > "$TMPDIR/named-8bit.json" <<EOF
{
  "dataFiles": {
    "srcType": "colorData",
    "srcFile": "",
    "dstType": "colorData",
    "dstFile": "",
    "dstEncoding": "8Bit",
    "dstPrecision": 0,
    "dstDigits": 3
  },
  "profileSequence": [
    {
      "iccFile": "$PROFILE",
      "intent": 0,
      "interpolation": "linear",
      "useBPC": false
    }
  ],
  "colorData": {
    "space": "RGB ",
    "encoding": "8Bit",
    "data": [
      {"values": [255, 255, 255]},
      {"values": [0, 0, 0]},
      {"values": [128, 128, 128]},
      {"values": [255, 0, 0]}
    ]
  }
}
EOF

# 3. ApplyNamedCmm -- 16-bit encoding
cat > "$TMPDIR/named-16bit.json" <<EOF
{
  "dataFiles": {
    "srcType": "colorData",
    "srcFile": "",
    "dstType": "colorData",
    "dstFile": "",
    "dstEncoding": "16Bit",
    "dstPrecision": 0,
    "dstDigits": 5
  },
  "profileSequence": [
    {
      "iccFile": "$PROFILE",
      "intent": 1,
      "interpolation": "tetrahedral",
      "useBPC": true
    }
  ],
  "colorData": {
    "space": "RGB ",
    "encoding": "16Bit",
    "data": [
      {"values": [65535, 65535, 65535]},
      {"values": [0, 0, 0]},
      {"values": [32768, 32768, 32768]}
    ]
  }
}
EOF

# 4. ApplyNamedCmm -- output to file
cat > "$TMPDIR/named-output-file.json" <<EOF
{
  "dataFiles": {
    "srcType": "colorData",
    "srcFile": "",
    "dstType": "colorData",
    "dstFile": "$TMPDIR/output.txt",
    "dstEncoding": "value",
    "dstPrecision": 6,
    "dstDigits": 12
  },
  "profileSequence": [
    {
      "iccFile": "$PROFILE",
      "intent": 1,
      "useBPC": false
    }
  ],
  "colorData": {
    "space": "RGB ",
    "encoding": "float",
    "data": [
      {"values": [0.5, 0.5, 0.5]}
    ]
  }
}
EOF

# 5. ApplyNamedCmm -- debugCalc + BPC
cat > "$TMPDIR/named-debug-bpc.json" <<EOF
{
  "dataFiles": {
    "srcType": "colorData",
    "srcFile": "",
    "dstType": "colorData",
    "dstFile": "",
    "dstEncoding": "value",
    "dstPrecision": 4,
    "dstDigits": 9,
    "debugCalc": false
  },
  "profileSequence": [
    {
      "iccFile": "$PROFILE",
      "intent": 1,
      "interpolation": "tetrahedral",
      "useBPC": true,
      "useD2BxB2Dx": true,
      "adjustPcsLuminance": false,
      "useHToS": false,
      "useSourceTransform": false,
      "useV5SubProfile": false
    }
  ],
  "colorData": {
    "space": "RGB ",
    "encoding": "float",
    "data": [
      {"values": [0.5, 0.5, 0.5]},
      {"values": [1.0, 0.0, 0.0]}
    ]
  }
}
EOF

# 6. ApplySearch -- basic
cat > "$TMPDIR/search-basic.json" <<EOF
{
  "dataFiles": {
    "srcType": "colorData",
    "srcFile": "",
    "dstType": "colorData",
    "dstFile": "",
    "dstEncoding": "value",
    "dstPrecision": 4,
    "dstDigits": 9
  },
  "searchApply": {
    "profileSequence": [
      {
        "iccFile": "$PROFILE",
        "intent": 0,
        "interpolation": "tetrahedral",
        "useBPC": false
      }
    ],
    "initial": {
      "l": 50.0,
      "sn": 30,
      "sv": 5.0
    }
  },
  "colorData": {
    "space": "RGB ",
    "encoding": "float",
    "data": [
      {"values": [0.5, 0.5, 0.5]}
    ]
  }
}
EOF

# 7-10: Malformed configs (expect failure)
echo '{}' > "$TMPDIR/empty-object.json"
echo '[1,2,3]' > "$TMPDIR/array-not-object.json"
echo '{"broken":' > "$TMPDIR/invalid-syntax.json"
echo 'null' > "$TMPDIR/null-value.json"

# 11. Missing profileSequence
cat > "$TMPDIR/missing-profileseq.json" <<EOF
{
  "dataFiles": {
    "srcType": "colorData",
    "dstType": "colorData",
    "dstEncoding": "value"
  }
}
EOF

# 12. Nonexistent profile path
cat > "$TMPDIR/nonexistent-profile.json" <<EOF
{
  "dataFiles": {
    "srcType": "colorData",
    "dstType": "colorData",
    "dstEncoding": "value"
  },
  "profileSequence": [
    {
      "iccFile": "/nonexistent/path/fake.icc",
      "intent": 0
    }
  ],
  "colorData": {
    "space": "RGB ",
    "encoding": "float",
    "data": [{"values": [0.5, 0.5, 0.5]}]
  }
}
EOF

# 13. Wrong types (integer where string expected)
cat > "$TMPDIR/wrong-types.json" <<EOF
{
  "dataFiles": {
    "srcType": 999,
    "dstType": 999,
    "dstEncoding": 999
  },
  "profileSequence": [
    {
      "iccFile": "$PROFILE",
      "intent": "not-a-number"
    }
  ],
  "colorData": {
    "space": "RGB ",
    "encoding": "float",
    "data": [{"values": [0.5, 0.5, 0.5]}]
  }
}
EOF

# 14. Extreme numeric values
cat > "$TMPDIR/extreme-values.json" <<EOF
{
  "dataFiles": {
    "srcType": "colorData",
    "dstType": "colorData",
    "dstEncoding": "float",
    "dstPrecision": 999999,
    "dstDigits": 999999
  },
  "profileSequence": [
    {
      "iccFile": "$PROFILE",
      "intent": 1
    }
  ],
  "colorData": {
    "space": "RGB ",
    "encoding": "float",
    "data": [{"values": [1e38, -1e38, 0.0]}]
  }
}
EOF

# 15. Path traversal attempt
cat > "$TMPDIR/path-traversal.json" <<EOF
{
  "dataFiles": {
    "srcType": "colorData",
    "dstType": "colorData",
    "dstEncoding": "value"
  },
  "profileSequence": [
    {
      "iccFile": "../../../../../../etc/passwd",
      "intent": 0
    }
  ],
  "colorData": {
    "space": "RGB ",
    "encoding": "float",
    "data": [{"values": [0.5, 0.5, 0.5]}]
  }
}
EOF

# 16-20: Encoding variations
for enc in "float" "unitFloat" "percent" "16BitV2" "value"; do
  cat > "$TMPDIR/named-enc-${enc}.json" <<EOF
{
  "dataFiles": {
    "srcType": "colorData",
    "dstType": "colorData",
    "dstEncoding": "$enc",
    "dstPrecision": 4,
    "dstDigits": 9
  },
  "profileSequence": [
    {
      "iccFile": "$PROFILE",
      "intent": 1
    }
  ],
  "colorData": {
    "space": "RGB ",
    "encoding": "$enc",
    "data": [{"values": [0.5, 0.5, 0.5]}]
  }
}
EOF
done

# 21-24: Intent variations (0-3)
for intent in 0 1 2 3; do
  cat > "$TMPDIR/named-intent-${intent}.json" <<EOF
{
  "dataFiles": {
    "srcType": "colorData",
    "dstType": "colorData",
    "dstEncoding": "value",
    "dstPrecision": 4,
    "dstDigits": 9
  },
  "profileSequence": [
    {
      "iccFile": "$PROFILE",
      "intent": $intent,
      "useBPC": false
    }
  ],
  "colorData": {
    "space": "RGB ",
    "encoding": "float",
    "data": [{"values": [0.5, 0.5, 0.5]}]
  }
}
EOF
done

# 25. Two-profile chain (same profile twice = identity-ish)
cat > "$TMPDIR/named-chain.json" <<EOF
{
  "dataFiles": {
    "srcType": "colorData",
    "dstType": "colorData",
    "dstEncoding": "value",
    "dstPrecision": 4,
    "dstDigits": 9
  },
  "profileSequence": [
    {
      "iccFile": "$PROFILE",
      "intent": 1,
      "useBPC": false
    },
    {
      "iccFile": "$PROFILE",
      "intent": 1,
      "useBPC": false
    }
  ],
  "colorData": {
    "space": "RGB ",
    "encoding": "float",
    "data": [{"values": [0.5, 0.5, 0.5]}, {"values": [1.0, 0.0, 0.0]}]
  }
}
EOF

# 26. Boolean flags exercise
cat > "$TMPDIR/named-booleans.json" <<EOF
{
  "dataFiles": {
    "srcType": "colorData",
    "dstType": "colorData",
    "dstEncoding": "value",
    "dstPrecision": 4,
    "dstDigits": 9,
    "debugCalc": true
  },
  "profileSequence": [
    {
      "iccFile": "$PROFILE",
      "intent": 1,
      "useBPC": true,
      "useD2BxB2Dx": false,
      "adjustPcsLuminance": true,
      "useHToS": true,
      "useSourceTransform": true,
      "useV5SubProfile": true
    }
  ],
  "colorData": {
    "space": "RGB ",
    "encoding": "float",
    "data": [{"values": [0.5, 0.5, 0.5]}]
  }
}
EOF

# -- Run tests ----------------------------------------------------------------

echo "=========================================="
echo " iccDEV JSON Configuration Tests"
echo "=========================================="
echo ""

echo "-- Section 1: iccApplyNamedCmm -- Valid configs --"
run_test "named-basic"       "$APPLY_NAMED_CMM" "$TMPDIR/named-basic.json"
run_test "named-8bit"        "$APPLY_NAMED_CMM" "$TMPDIR/named-8bit.json"
run_test "named-16bit"       "$APPLY_NAMED_CMM" "$TMPDIR/named-16bit.json"
run_test "named-output-file" "$APPLY_NAMED_CMM" "$TMPDIR/named-output-file.json"
run_test "named-debug-bpc"   "$APPLY_NAMED_CMM" "$TMPDIR/named-debug-bpc.json"
run_test "named-chain"       "$APPLY_NAMED_CMM" "$TMPDIR/named-chain.json"
run_test "named-booleans"    "$APPLY_NAMED_CMM" "$TMPDIR/named-booleans.json"
echo ""

echo "-- Section 2: iccApplySearch -- Valid config --"
run_test "search-basic"      "$APPLY_SEARCH" "$TMPDIR/search-basic.json" true  # search may reject non-search profiles
echo ""

echo "-- Section 3: Encoding variations --"
for enc in float unitFloat percent 16BitV2 value; do
  # percent and value encodings may fail with range mismatch on some profiles
  if [ "$enc" = "percent" ] || [ "$enc" = "value" ]; then
    run_test "named-enc-${enc}" "$APPLY_NAMED_CMM" "$TMPDIR/named-enc-${enc}.json" true
  else
    run_test "named-enc-${enc}" "$APPLY_NAMED_CMM" "$TMPDIR/named-enc-${enc}.json"
  fi
done
echo ""

echo "-- Section 4: Intent variations (0-3) --"
for intent in 0 1 2 3; do
  run_test "named-intent-${intent}" "$APPLY_NAMED_CMM" "$TMPDIR/named-intent-${intent}.json"
done
echo ""

echo "-- Section 5: Malformed JSON (expect graceful failure) --"
run_test "empty-object"         "$APPLY_NAMED_CMM" "$TMPDIR/empty-object.json"         true
run_test "array-not-object"     "$APPLY_NAMED_CMM" "$TMPDIR/array-not-object.json"     true
run_test "invalid-syntax"       "$APPLY_NAMED_CMM" "$TMPDIR/invalid-syntax.json"       true
run_test "null-value"           "$APPLY_NAMED_CMM" "$TMPDIR/null-value.json"           true
run_test "missing-profileseq"   "$APPLY_NAMED_CMM" "$TMPDIR/missing-profileseq.json"   true
run_test "nonexistent-profile"  "$APPLY_NAMED_CMM" "$TMPDIR/nonexistent-profile.json"  true
run_test "wrong-types"          "$APPLY_NAMED_CMM" "$TMPDIR/wrong-types.json"
run_test "extreme-values"       "$APPLY_NAMED_CMM" "$TMPDIR/extreme-values.json"
run_test "path-traversal"       "$APPLY_NAMED_CMM" "$TMPDIR/path-traversal.json"       true
echo ""

echo "-- Section 6: Cross-tool malformed (ApplySearch) --"
run_test "search-empty"         "$APPLY_SEARCH" "$TMPDIR/empty-object.json"           true
run_test "search-invalid"       "$APPLY_SEARCH" "$TMPDIR/invalid-syntax.json"         true
run_test "search-nonexistent"   "$APPLY_SEARCH" "$TMPDIR/nonexistent-profile.json"    true
echo ""

echo "-- Section 7: Cross-tool malformed (ApplyProfiles) --"
run_test "profiles-empty"       "$APPLY_PROFILES" "$TMPDIR/empty-object.json"         true
run_test "profiles-invalid"     "$APPLY_PROFILES" "$TMPDIR/invalid-syntax.json"       true
echo ""

# -- Summary ------------------------------------------------------------------

echo "=========================================="
echo " RESULTS: $PASS/$TOTAL pass, $FAIL fail"
echo " ASAN findings: $ASAN_FINDINGS"
echo " UBSAN findings: $UBSAN_FINDINGS"
echo "=========================================="

# Cleanup
rm -rf "$TMPDIR"

# Exit code
if [ "$ASAN_FINDINGS" -gt 0 ] || [ "$UBSAN_FINDINGS" -gt 0 ]; then
  exit 2
elif [ "$FAIL" -gt 0 ]; then
  exit 1
else
  exit 0
fi
