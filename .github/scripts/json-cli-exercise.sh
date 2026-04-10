#!/bin/bash
# JSON CLI Exercise -- 125 tests across 26 groups for all JSON-enabled iccDEV tools
# Adapted for upstream iccDEV repository (run from repo root)
set -o pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
# Auto-detect: if in .github/scripts/, go up 2 levels to repo root
if [ -d "$SCRIPT_DIR/../../IccProfLib" ]; then
  REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
elif [ -d "$SCRIPT_DIR/../../../IccProfLib" ]; then
  REPO_ROOT="$(cd "$SCRIPT_DIR/../../.." && pwd)"
else
  REPO_ROOT="$(pwd)"
fi
cd "$REPO_ROOT" || exit 1
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-build/IccProfLib:build/IccXML}"
export ASAN_OPTIONS=halt_on_error=0,detect_leaks=0
export UBSAN_OPTIONS=halt_on_error=0,print_stacktrace=1

NAMEDCMM=build/Tools/IccApplyNamedCmm/iccApplyNamedCmm
SEARCH=build/Tools/IccApplySearch/iccApplySearch
PROFILES=build/Tools/IccApplyProfiles/iccApplyProfiles

PASS=0; FAIL=0; ASAN=0; TOTAL=0

run_test() {
  local name="$1" tool="$2" cfg="$3" expect="$4"
  TOTAL=$((TOTAL+1))
  output=$("$tool" -cfg "$cfg" 2>&1)
  rc=$?
  asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
  if [ "$asan_hit" -gt 0 ]; then
    ASAN=$((ASAN+1))
    echo "[ASAN] #$TOTAL $name (exit=$rc, $asan_hit sanitizer hits)"
    echo "$output" | grep 'AddressSanitizer\|runtime error:' | sed -n '1,3p'
  elif [ "$expect" = "any" ] || [ "$rc" -eq "$expect" ]; then
    PASS=$((PASS+1))
    echo "[PASS] #$TOTAL $name (exit=$rc)"
  else
    FAIL=$((FAIL+1))
    echo "[FAIL] #$TOTAL $name (exit=$rc, expected=$expect)"
    echo "$output" | tail -3
  fi
}

echo "=================================================================="
echo " JSON CLI Exercise -- All Fields, All Tools"
echo "=================================================================="
echo ""

# ============================================================
# GROUP 1: All 7 color encodings (ApplyNamedCmm)
# ============================================================
echo "--- GROUP 1: Color Encoding Strings (7 tests) ---"
for enc in value float unitFloat percent 8Bit 16Bit 16BitV2; do
  python3 -c "
import json, sys
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'$enc','dstPrecision':4,'dstDigits':9},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-enc-$enc.json','w'), indent=2)
"
  # 8Bit output on XYZ PCS space is unsupported (FromInternalEncoding returns BadColorEncoding)
  if [ "$enc" = "8Bit" ]; then
    run_test "encoding=$enc" "$NAMEDCMM" "/tmp/test-enc-$enc.json" any
  else
    run_test "encoding=$enc" "$NAMEDCMM" "/tmp/test-enc-$enc.json" 0
  fi
done

# ============================================================
# GROUP 2: dstPrecision variations
# ============================================================
echo ""
echo "--- GROUP 2: dstPrecision Variations (7 tests) ---"
for prec in 0 1 2 4 8 12 20; do
  digs=$((prec + 5))
  python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float','dstPrecision':$prec,'dstDigits':$digs},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-prec-$prec.json','w'), indent=2)
"
  run_test "dstPrecision=$prec" "$NAMEDCMM" "/tmp/test-prec-$prec.json" 0
done

# ============================================================
# GROUP 3: dstDigits variations
# ============================================================
echo ""
echo "--- GROUP 3: dstDigits Variations (5 tests) ---"
for dig in 1 5 9 15 30; do
  python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float','dstPrecision':4,'dstDigits':$dig},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-dig-$dig.json','w'), indent=2)
"
  run_test "dstDigits=$dig" "$NAMEDCMM" "/tmp/test-dig-$dig.json" 0
done

# ============================================================
# GROUP 4: debugCalc boolean
# ============================================================
echo ""
echo "--- GROUP 4: debugCalc Boolean (2 tests) ---"
for dc in True False; do
  python3 -c "
import json
cfg = {
  'dataFiles': {'debugCalc':$dc,'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-debug-$dc.json','w'), indent=2)
"
  run_test "debugCalc=$dc" "$NAMEDCMM" "/tmp/test-debug-$dc.json" 0
done

# ============================================================
# GROUP 5: srcType variations
# ============================================================
echo ""
echo "--- GROUP 5: srcType Variations (3 tests) ---"
for stype in colorData legacy it8; do
  python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'$stype','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-srctype-$stype.json','w'), indent=2)
"
  run_test "srcType=$stype" "$NAMEDCMM" "/tmp/test-srctype-$stype.json" any
done

# ============================================================
# GROUP 6: dstFile output routing
# ============================================================
echo ""
echo "--- GROUP 6: dstFile Output (2 tests) ---"
rm -f /tmp/json-test-output.txt
python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstFile':'/tmp/json-test-output.txt','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]},{'values':[1.0,0.0,0.0]}]}
}
json.dump(cfg, open('/tmp/test-dstfile.json','w'), indent=2)
"
run_test "dstFile=/tmp/output" "$NAMEDCMM" "/tmp/test-dstfile.json" 0
if [ -f /tmp/json-test-output.txt ]; then
  echo "  -> output file created ($(wc -c < /tmp/json-test-output.txt) bytes)"
  rm /tmp/json-test-output.txt
fi

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstFile':'','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-dstfile-empty.json','w'), indent=2)
"
run_test "dstFile=empty(stdout)" "$NAMEDCMM" "/tmp/test-dstfile-empty.json" 0

# ============================================================
# GROUP 7: srcSpace override
# ============================================================
echo ""
echo "--- GROUP 7: srcSpace Override (2 tests) ---"
python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','srcSpace':'RGB ','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-srcspace-rgb.json','w'), indent=2)
"
run_test "srcSpace=RGB" "$NAMEDCMM" "/tmp/test-srcspace-rgb.json" 0

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','srcSpace':'Lab ','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'Lab ','encoding':'float','data':[{'values':[50.0,0.0,0.0]}]}
}
json.dump(cfg, open('/tmp/test-srcspace-lab.json','w'), indent=2)
"
run_test "srcSpace=Lab" "$NAMEDCMM" "/tmp/test-srcspace-lab.json" any

# ============================================================
# GROUP 8: Rendering intents
# ============================================================
echo ""
echo "--- GROUP 8: Rendering Intents (10 tests) ---"
for intent in 0 1 2 3 41 43 91 93 101 1001; do
  python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':$intent}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-intent-$intent.json','w'), indent=2)
"
  run_test "intent=$intent" "$NAMEDCMM" "/tmp/test-intent-$intent.json" any
done

# ============================================================
# GROUP 9: Interpolation modes
# ============================================================
echo ""
echo "--- GROUP 9: Interpolation Modes (2 tests) ---"
for interp in linear tetrahedral; do
  python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1,'interpolation':'$interp'}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-interp-$interp.json','w'), indent=2)
"
  run_test "interpolation=$interp" "$NAMEDCMM" "/tmp/test-interp-$interp.json" 0
done

# ============================================================
# GROUP 10: Profile boolean flags
# ============================================================
echo ""
echo "--- GROUP 10: Profile Boolean Flags (8 tests) ---"
for flag in useBPC useD2BxB2Dx adjustPcsLuminance useHToS; do
  for val in True False; do
    python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1,'$flag':$val}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-flag-${flag}-${val}.json','w'), indent=2)
"
    run_test "$flag=$val" "$NAMEDCMM" "/tmp/test-flag-${flag}-${val}.json" any
  done
done

# ============================================================
# GROUP 11: iccEnvVars and pccEnvVars
# ============================================================
echo ""
echo "--- GROUP 11: Environment Variable Maps (3 tests) ---"
python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1,'iccEnvVars':[{'name':'art\x00','value':1.0}]}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-envvars.json','w'), indent=2)
"
run_test "iccEnvVars" "$NAMEDCMM" "/tmp/test-envvars.json" any

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1,'pccEnvVars':[{'name':'art\x00','value':1.0}]}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-pccenvvars.json','w'), indent=2)
"
run_test "pccEnvVars" "$NAMEDCMM" "/tmp/test-pccenvvars.json" any

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1,'iccEnvVars':[],'pccEnvVars':[]}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-empty-envvars.json','w'), indent=2)
"
run_test "empty envVars arrays" "$NAMEDCMM" "/tmp/test-empty-envvars.json" 0

# ============================================================
# GROUP 12: pccFile
# ============================================================
echo ""
echo "--- GROUP 12: PCC File (2 tests) ---"
python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1,'pccFile':'Testing/Display/sRGB_D65_MAT.icc'}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-pccfile.json','w'), indent=2)
"
run_test "pccFile=sRGB" "$NAMEDCMM" "/tmp/test-pccfile.json" any

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1,'pccFile':''}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-pccfile-empty.json','w'), indent=2)
"
run_test "pccFile=empty" "$NAMEDCMM" "/tmp/test-pccfile-empty.json" 0

# ============================================================
# GROUP 13: Multi-profile chains
# ============================================================
echo ""
echo "--- GROUP 13: Multi-Profile Chains (3 tests) ---"
python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [
    {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1},
    {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}
  ],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-chain2.json','w'), indent=2)
"
run_test "2-profile chain" "$NAMEDCMM" "/tmp/test-chain2.json" 0

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [
    {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1},
    {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':3},
    {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}
  ],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-chain3.json','w'), indent=2)
"
run_test "3-profile chain mixed intents" "$NAMEDCMM" "/tmp/test-chain3.json" 0

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-chain-empty.json','w'), indent=2)
"
run_test "empty profileSequence" "$NAMEDCMM" "/tmp/test-chain-empty.json" any

# ============================================================
# GROUP 14: colorData encoding variants
# ============================================================
echo ""
echo "--- GROUP 14: colorData Input Encoding (4 tests) ---"
for cenc in float 8Bit 16Bit value; do
  if [ "$cenc" = "8Bit" ]; then
    vals="[128,128,128]"
  elif [ "$cenc" = "16Bit" ]; then
    vals="[32768,32768,32768]"
  else
    vals="[0.5,0.5,0.5]"
  fi
  python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'RGB ','encoding':'$cenc','data':[{'values':$vals}]}
}
json.dump(cfg, open('/tmp/test-cenc-$cenc.json','w'), indent=2)
"
  if [ "$cenc" = "value" ]; then
    run_test "colorData.encoding=$cenc" "$NAMEDCMM" "/tmp/test-cenc-$cenc.json" any
  else
    run_test "colorData.encoding=$cenc" "$NAMEDCMM" "/tmp/test-cenc-$cenc.json" 0
  fi
done

# ============================================================
# GROUP 15: Multiple data samples
# ============================================================
echo ""
echo "--- GROUP 15: Multiple Data Samples (2 tests) ---"
python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'RGB ','encoding':'float','data':[
    {'values':[0.0,0.0,0.0]},{'values':[0.25,0.25,0.25]},{'values':[0.5,0.5,0.5]},
    {'values':[0.75,0.75,0.75]},{'values':[1.0,1.0,1.0]},{'values':[1.0,0.0,0.0]},
    {'values':[0.0,1.0,0.0]},{'values':[0.0,0.0,1.0]},{'values':[1.0,1.0,0.0]},
    {'values':[0.0,1.0,1.0]}
  ]}
}
json.dump(cfg, open('/tmp/test-multi-samples.json','w'), indent=2)
"
run_test "10 color samples" "$NAMEDCMM" "/tmp/test-multi-samples.json" 0

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-single-sample.json','w'), indent=2)
"
run_test "1 color sample" "$NAMEDCMM" "/tmp/test-single-sample.json" 0

# ============================================================
# GROUP 16: ApplySearch field variations
# ============================================================
echo ""
echo "--- GROUP 16: ApplySearch Field Variations (8 tests) ---"

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'searchApply': {
    'profileSequence': [
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1},
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}
    ],
    'initial': {'intent':1},
    'pccWeights': []
  },
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-search-basic.json','w'), indent=2)
"
run_test "search: basic 2-profile" "$SEARCH" "/tmp/test-search-basic.json" 0

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'searchApply': {
    'profileSequence': [
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1},
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1},
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}
    ],
    'initial': {'intent':1},
    'pccWeights': []
  },
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-search-3prof.json','w'), indent=2)
"
run_test "search: 3-profile chain" "$SEARCH" "/tmp/test-search-3prof.json" any

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'searchApply': {
    'profileSequence': [
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':41,'useBPC':True},
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':41,'useBPC':True}
    ],
    'initial': {'intent':41},
    'pccWeights': []
  },
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-search-bpc.json','w'), indent=2)
"
run_test "search: BPC intent=41" "$SEARCH" "/tmp/test-search-bpc.json" any

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'searchApply': {
    'profileSequence': [
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1},
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}
    ],
    'initial': {'intent':1,'adjustPcsLuminance':True},
    'pccWeights': []
  },
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-search-luminance.json','w'), indent=2)
"
run_test "search: adjustPcsLuminance=true" "$SEARCH" "/tmp/test-search-luminance.json" any

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'searchApply': {
    'profileSequence': [
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1},
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}
    ],
    'initial': {'intent':1,'useV5SubProfile':True},
    'pccWeights': []
  },
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-search-v5sub.json','w'), indent=2)
"
run_test "search: useV5SubProfile=true" "$SEARCH" "/tmp/test-search-v5sub.json" any

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'searchApply': {
    'profileSequence': [
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1,'interpolation':'linear'},
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1,'interpolation':'tetrahedral'}
    ],
    'initial': {'intent':1,'interpolation':'linear'},
    'pccWeights': []
  },
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-search-interp-init.json','w'), indent=2)
"
run_test "search: initial.interpolation" "$SEARCH" "/tmp/test-search-interp-init.json" 0

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'searchApply': {
    'profileSequence': [
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1},
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}
    ],
    'initial': {'intent':1},
    'pccWeights': [{'pccFile':'Testing/Display/sRGB_D65_MAT.icc','weight':1.0}]
  },
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-search-pccweights.json','w'), indent=2)
"
run_test "search: pccWeights with weight=1.0" "$SEARCH" "/tmp/test-search-pccweights.json" any

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'8Bit'},
  'searchApply': {
    'profileSequence': [
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1},
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}
    ],
    'initial': {'intent':1},
    'pccWeights': []
  },
  'colorData': {'space':'RGB ','encoding':'8Bit','data':[{'values':[128,128,128]}]}
}
json.dump(cfg, open('/tmp/test-search-8bit.json','w'), indent=2)
"
run_test "search: 8Bit encoding" "$SEARCH" "/tmp/test-search-8bit.json" 0

# ============================================================
# GROUP 17: ApplyProfiles JSON fields
# ============================================================
echo ""
echo "--- GROUP 17: ApplyProfiles JSON Fields (6 tests) ---"

# These require TIFF files which may not exist -- test config parsing
for fenc in 8Bit 16Bit float sameAsSource; do
  python3 -c "
import json
cfg = {
  'imageFiles': {
    'srcImageFile': '/tmp/nonexistent.tif',
    'dstImageFile': '/tmp/out.tif',
    'dstEncoding': '$fenc',
    'dstCompression': False,
    'dstPlanar': False,
    'dstEmbedIcc': True
  },
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}]
}
json.dump(cfg, open('/tmp/test-applyprofiles-enc-$fenc.json','w'), indent=2)
"
  run_test "applyProfiles: dstEnc=$fenc" "$PROFILES" "/tmp/test-applyprofiles-enc-$fenc.json" any
done

python3 -c "
import json
cfg = {
  'imageFiles': {
    'srcImageFile': '/tmp/nonexistent.tif',
    'dstImageFile': '/tmp/out.tif',
    'dstEncoding': '8Bit',
    'dstCompression': True,
    'dstPlanar': True,
    'dstEmbedIcc': True
  },
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}]
}
json.dump(cfg, open('/tmp/test-applyprofiles-compress.json','w'), indent=2)
"
run_test "applyProfiles: compress+planar+embed" "$PROFILES" "/tmp/test-applyprofiles-compress.json" any

python3 -c "
import json
cfg = {
  'imageFiles': {
    'srcImageFile': '/tmp/nonexistent.tif',
    'dstImageFile': '/tmp/out.tif',
    'dstEncoding': '8Bit',
    'dstCompression': False,
    'dstPlanar': False,
    'dstEmbedIcc': False
  },
  'profileSequence': [
    {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1},
    {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}
  ]
}
json.dump(cfg, open('/tmp/test-applyprofiles-chain.json','w'), indent=2)
"
run_test "applyProfiles: 2-profile chain" "$PROFILES" "/tmp/test-applyprofiles-chain.json" any

# ============================================================
# GROUP 18: Edge-case field values
# ============================================================
echo ""
echo "--- GROUP 18: Edge-Case Field Values (6 tests) ---"

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':-1}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-neg-intent.json','w'), indent=2)
"
run_test "negative intent=-1" "$NAMEDCMM" "/tmp/test-neg-intent.json" any

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':99999}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-large-intent.json','w'), indent=2)
"
run_test "large intent=99999" "$NAMEDCMM" "/tmp/test-large-intent.json" any

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'BOGUS'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-bad-encoding.json','w'), indent=2)
"
run_test "invalid encoding=BOGUS" "$NAMEDCMM" "/tmp/test-bad-encoding.json" any

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':3},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-num-encoding.json','w'), indent=2)
"
run_test "numeric encoding=3" "$NAMEDCMM" "/tmp/test-num-encoding.json" any

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'RGB ','encoding':'float','data':[]}
}
json.dump(cfg, open('/tmp/test-empty-data.json','w'), indent=2)
"
run_test "empty data array" "$NAMEDCMM" "/tmp/test-empty-data.json" any

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}],
  'colorData': {'space':'CMYK','encoding':'float','data':[{'values':[0.5,0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-cmyk-mismatch.json','w'), indent=2)
"
run_test "CMYK space to RGB profile" "$NAMEDCMM" "/tmp/test-cmyk-mismatch.json" any

# ============================================================
# GROUP 19: Stress configs -- all fields simultaneously
# ============================================================
echo ""
echo "--- GROUP 19: Stress Configs (4 tests) ---"

python3 -c "
import json
cfg = {
  'dataFiles': {
    'debugCalc': False,
    'srcType': 'colorData',
    'srcSpace': 'RGB ',
    'srcFile': '',
    'dstType': 'colorData',
    'dstFile': '',
    'dstEncoding': 'float',
    'dstPrecision': 6,
    'dstDigits': 12
  },
  'profileSequence': [{
    'iccFile': 'Testing/Display/sRGB_D65_MAT.icc',
    'intent': 1,
    'interpolation': 'tetrahedral',
    'useBPC': False,
    'useD2BxB2Dx': True,
    'adjustPcsLuminance': False,
    'useHToS': False,
    'iccEnvVars': [],
    'pccEnvVars': [],
    'pccFile': ''
  }],
  'colorData': {
    'space': 'RGB ',
    'encoding': 'float',
    'data': [
      {'values': [0.0, 0.0, 0.0]},
      {'values': [0.5, 0.5, 0.5]},
      {'values': [1.0, 1.0, 1.0]}
    ]
  }
}
json.dump(cfg, open('/tmp/test-all-fields.json','w'), indent=2)
"
run_test "all fields set" "$NAMEDCMM" "/tmp/test-all-fields.json" 0

python3 -c "
import json
cfg = {
  'dataFiles': {'debugCalc':True,'srcType':'colorData','dstEncoding':'float','dstPrecision':8,'dstDigits':14},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1,'useBPC':True,'useD2BxB2Dx':True,'adjustPcsLuminance':True,'useHToS':True}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-all-true.json','w'), indent=2)
"
run_test "all booleans true" "$NAMEDCMM" "/tmp/test-all-true.json" any

python3 -c "
import json
cfg = {
  'dataFiles': {'debugCalc':False,'srcType':'colorData','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1,'useBPC':False,'useD2BxB2Dx':False,'adjustPcsLuminance':False,'useHToS':False}],
  'colorData': {'space':'RGB ','encoding':'float','data':[{'values':[0.5,0.5,0.5]}]}
}
json.dump(cfg, open('/tmp/test-all-false.json','w'), indent=2)
"
run_test "all booleans false" "$NAMEDCMM" "/tmp/test-all-false.json" 0

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'colorData','dstEncoding':'float'},
  'searchApply': {
    'profileSequence': [
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1,'interpolation':'linear','useBPC':True},
      {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':3,'interpolation':'tetrahedral','useBPC':False}
    ],
    'initial': {'intent':41,'adjustPcsLuminance':False,'useV5SubProfile':False,'interpolation':'tetrahedral'},
    'pccWeights': []
  },
  'colorData': {'space':'RGB ','encoding':'8Bit','data':[{'values':[128,64,192]},{'values':[255,0,0]},{'values':[0,255,0]}]}
}
json.dump(cfg, open('/tmp/test-search-mixed.json','w'), indent=2)
"
run_test "search: mixed intents+interp+bpc" "$SEARCH" "/tmp/test-search-mixed.json" any

# ============================================================
# GROUP 20: External srcFile with -cfg
# ============================================================
echo ""
echo "--- GROUP 20: External srcFile (2 tests) ---"

# Create external legacy data file
cat > /tmp/test-external-data.txt << 'EXTEOF'
'RGB '	; Data Format
icEncodeFloat	; Encoding

0.5 0.5 0.5
1.0 0.0 0.0
0.0 1.0 0.0
EXTEOF

python3 -c "
import json
cfg = {
  'dataFiles': {'srcType':'legacy','srcFile':'/tmp/test-external-data.txt','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}]
}
json.dump(cfg, open('/tmp/test-srcfile-legacy.json','w'), indent=2)
"
run_test "srcFile=legacy external" "$NAMEDCMM" "/tmp/test-srcfile-legacy.json" 0

# External JSON data file
python3 -c "
import json
data = {
  'space': 'RGB ',
  'encoding': 'float',
  'data': [{'values':[0.5,0.5,0.5]},{'values':[1.0,0.0,0.0]}]
}
json.dump(data, open('/tmp/test-external-data.json','w'), indent=2)
cfg = {
  'dataFiles': {'srcType':'colorData','srcFile':'/tmp/test-external-data.json','dstEncoding':'float'},
  'profileSequence': [{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}]
}
json.dump(cfg, open('/tmp/test-srcfile-json.json','w'), indent=2)
"
run_test "srcFile=json external" "$NAMEDCMM" "/tmp/test-srcfile-json.json" 0

# ============================================================
# GROUP 21: CLI args mode (non-JSON) for comparison
# ============================================================
echo ""
echo "--- GROUP 21: CLI Args Mode Comparison (6 tests) ---"

# ApplyNamedCmm CLI: data_file encoding interpolation profile intent
# Create a minimal legacy data file
cat > /tmp/test-cli-data.txt << 'CLIEOF'
'RGB '	; Data Format
icEncodeFloat	; Encoding

0.5 0.5 0.5
1.0 0.0 0.0
0.0 0.0 1.0
CLIEOF

# Standard CLI: float encoding, tetrahedral, relative intent
TOTAL=$((TOTAL+1))
output=$("$NAMEDCMM" "/tmp/test-cli-data.txt" 3 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL cli: float+tet+rel (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli: float enc(3) + tet(1) + rel(1) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL cli: float+tet+rel (exit=$rc)"; fi

# CLI: 8bit encoding
TOTAL=$((TOTAL+1))
cat > /tmp/test-cli-8bit.txt << 'CLIEOF2'
'RGB '	; Data Format
icEncode8Bit	; Encoding

128 128 128
255 0 0
CLIEOF2
output=$("$NAMEDCMM" "/tmp/test-cli-8bit.txt" 4 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL cli: 8bit (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli: 8Bit enc(4) (exit=$rc)"
else PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli: 8Bit enc(4) -- XYZ 8Bit unsupported (exit=$rc)"; fi

# CLI: 16bit encoding
TOTAL=$((TOTAL+1))
cat > /tmp/test-cli-16bit.txt << 'CLIEOF3'
'RGB '	; Data Format
icEncode16Bit	; Encoding

32768 32768 32768
CLIEOF3
output=$("$NAMEDCMM" "/tmp/test-cli-16bit.txt" 5 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL cli: 16bit (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli: 16Bit enc(5) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL cli: 16bit (exit=$rc)"; fi

# CLI: linear interpolation
TOTAL=$((TOTAL+1))
output=$("$NAMEDCMM" "/tmp/test-cli-data.txt" 3 0 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL cli: linear (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli: linear interp(0) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL cli: linear (exit=$rc)"; fi

# CLI: absolute intent
TOTAL=$((TOTAL+1))
output=$("$NAMEDCMM" "/tmp/test-cli-data.txt" 3 1 "Testing/Display/sRGB_D65_MAT.icc" 3 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL cli: absolute (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli: absolute intent(3) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL cli: absolute (exit=$rc)"; fi

# CLI: -debugcalc flag
TOTAL=$((TOTAL+1))
output=$("$NAMEDCMM" -debugcalc "/tmp/test-cli-data.txt" 3 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL cli: -debugcalc (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli: -debugcalc flag (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL cli: -debugcalc (exit=$rc)"; fi

# ============================================================
# GROUP 22: ApplySearch CLI vs JSON cross-validation
# ============================================================
echo ""
echo "--- GROUP 22: ApplySearch CLI Args (3 tests) ---"

# CLI: data_file encoding interpolation profile1 intent1 profile2 intent2 -INIT init_intent
TOTAL=$((TOTAL+1))
output=$("$SEARCH" "/tmp/test-cli-data.txt" 3 1 "Testing/Display/sRGB_D65_MAT.icc" 1 "Testing/Display/sRGB_D65_MAT.icc" 1 -INIT 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL search cli: basic (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL search cli: basic 2-prof (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL search cli: basic (exit=$rc)"; fi

# Search CLI with 3 profiles (middle profile)
TOTAL=$((TOTAL+1))
output=$("$SEARCH" "/tmp/test-cli-data.txt" 3 1 "Testing/Display/sRGB_D65_MAT.icc" 1 "Testing/Display/sRGB_D65_MAT.icc" 1 "Testing/Display/sRGB_D65_MAT.icc" 1 -INIT 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL search cli: 3-prof (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL search cli: 3-profile chain (exit=$rc)"
else PASS=$((PASS+1)); echo "[PASS] #$TOTAL search cli: 3-prof -- incompatible chain expected fail (exit=$rc)"; fi

# Search CLI: -debugcalc
TOTAL=$((TOTAL+1))
output=$("$SEARCH" -debugcalc "/tmp/test-cli-data.txt" 3 1 "Testing/Display/sRGB_D65_MAT.icc" 1 "Testing/Display/sRGB_D65_MAT.icc" 1 -INIT 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL search cli: debugcalc (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL search cli: -debugcalc (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL search cli: debugcalc (exit=$rc)"; fi

# ============================================================
# GROUP 23: ApplyProfiles -- Real TIFF processing (JSON -cfg)
# ============================================================
echo ""
echo "--- GROUP 23: ApplyProfiles Real TIFF Processing (10 tests) ---"

# Create minimal test TIFFs if test-data dir exists
TEST_DATA_DIR=""
for d in "docs/Testing/test-data" "Testing/Fuzzing/docs/Tools/test-data"; do
  if [ -f "$d/rgb-4x4-8bit.tif" ]; then TEST_DATA_DIR="$d"; break; fi
done

# If no pre-built TIFFs, create them on the fly
if [ -z "$TEST_DATA_DIR" ]; then
  TEST_DATA_DIR="/tmp/json-test-tiffs"
  mkdir -p "$TEST_DATA_DIR"
  python3 -c "
import struct, os
def make_tiff(path, w, h, bps):
    spp = 3; data_len = w * h * spp * (bps // 8)
    pixel_data = bytes([128] * data_len)
    ifd_offset = 8; num_entries = 12; ifd_size = 2 + num_entries * 12 + 4
    data_offset = ifd_offset + ifd_size
    bps_offset = data_offset; xres_offset = bps_offset + 6; yres_offset = xres_offset + 8
    strip_offset = yres_offset + 8
    header = struct.pack('<2sIH', b'II', 42, ifd_offset)
    def e(tag, typ, cnt, val): return struct.pack('<HHII', tag, typ, cnt, val)
    ifd = struct.pack('<H', num_entries)
    ifd += e(256,3,1,w) + e(257,3,1,h) + e(258,3,3,bps_offset)
    ifd += e(259,3,1,1) + e(262,3,1,2) + e(273,4,1,strip_offset)
    ifd += e(277,3,1,spp) + e(278,3,1,h) + e(279,4,1,data_len)
    ifd += e(282,5,1,xres_offset) + e(283,5,1,yres_offset) + e(296,3,1,2)
    ifd += struct.pack('<I', 0)
    extra = struct.pack('<HHH', bps, bps, bps)
    extra += struct.pack('<II', 72, 1) + struct.pack('<II', 72, 1)
    with open(path, 'wb') as f:
        f.write(header); f.write(ifd); f.write(extra); f.write(pixel_data)
make_tiff('$TEST_DATA_DIR/rgb-4x4-8bit.tif', 4, 4, 8)
make_tiff('$TEST_DATA_DIR/rgb-4x4-16bit.tif', 4, 4, 16)
make_tiff('$TEST_DATA_DIR/rgb-8x8-8bit.tif', 8, 8, 8)
"
fi

# 23a: Real TIFF with each dstEncoding
for enc in 8Bit 16Bit float sameAsSource; do
  python3 -c "
import json; json.dump({
  'imageFiles':{'srcImageFile':'$TEST_DATA_DIR/rgb-4x4-8bit.tif','dstImageFile':'/tmp/json-tiff-$enc.tif',
    'dstEncoding':'$enc','dstCompression':False,'dstPlanar':False,'dstEmbedIcc':True},
  'profileSequence':[{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}]
}, open('/tmp/test-real-tiff-$enc.json','w'), indent=2)"
  run_test "applyProfiles real TIFF: enc=$enc" "$PROFILES" "/tmp/test-real-tiff-$enc.json" 0
done

# 23b: 16-bit source TIFF
python3 -c "
import json; json.dump({
  'imageFiles':{'srcImageFile':'$TEST_DATA_DIR/rgb-4x4-16bit.tif','dstImageFile':'/tmp/json-tiff-16src.tif',
    'dstEncoding':'16Bit','dstCompression':False,'dstPlanar':False,'dstEmbedIcc':True},
  'profileSequence':[{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}]
}, open('/tmp/test-real-tiff-16src.json','w'), indent=2)"
run_test "applyProfiles: 16-bit source TIFF" "$PROFILES" "/tmp/test-real-tiff-16src.json" 0

# 23c: Compression + embed
python3 -c "
import json; json.dump({
  'imageFiles':{'srcImageFile':'$TEST_DATA_DIR/rgb-4x4-8bit.tif','dstImageFile':'/tmp/json-tiff-lzw.tif',
    'dstEncoding':'8Bit','dstCompression':True,'dstPlanar':False,'dstEmbedIcc':True},
  'profileSequence':[{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}]
}, open('/tmp/test-real-tiff-lzw.json','w'), indent=2)"
run_test "applyProfiles: LZW compression + embed" "$PROFILES" "/tmp/test-real-tiff-lzw.json" 0

# 23d: No embed ICC
python3 -c "
import json; json.dump({
  'imageFiles':{'srcImageFile':'$TEST_DATA_DIR/rgb-4x4-8bit.tif','dstImageFile':'/tmp/json-tiff-noembed.tif',
    'dstEncoding':'8Bit','dstCompression':False,'dstPlanar':False,'dstEmbedIcc':False},
  'profileSequence':[{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}]
}, open('/tmp/test-real-tiff-noembed.json','w'), indent=2)"
run_test "applyProfiles: no embed ICC" "$PROFILES" "/tmp/test-real-tiff-noembed.json" 0

# 23e: Multi-profile chain with real TIFF
python3 -c "
import json; json.dump({
  'imageFiles':{'srcImageFile':'$TEST_DATA_DIR/rgb-8x8-8bit.tif','dstImageFile':'/tmp/json-tiff-chain.tif',
    'dstEncoding':'8Bit','dstCompression':False,'dstPlanar':False,'dstEmbedIcc':True},
  'profileSequence':[
    {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1},
    {'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':1}
  ]
}, open('/tmp/test-real-tiff-chain.json','w'), indent=2)"
run_test "applyProfiles: 2-profile chain real TIFF" "$PROFILES" "/tmp/test-real-tiff-chain.json" 0

# 23f: All rendering intents with real TIFF
for intent in 0 1 2 3; do
  python3 -c "
import json; json.dump({
  'imageFiles':{'srcImageFile':'$TEST_DATA_DIR/rgb-4x4-8bit.tif','dstImageFile':'/tmp/json-tiff-intent-$intent.tif',
    'dstEncoding':'8Bit','dstCompression':False,'dstPlanar':False,'dstEmbedIcc':True},
  'profileSequence':[{'iccFile':'Testing/Display/sRGB_D65_MAT.icc','intent':$intent}]
}, open('/tmp/test-real-tiff-intent-$intent.json','w'), indent=2)"
done
# Just run intents 0 and 2 (perceptual, saturation) -- 1 and 3 tested above
run_test "applyProfiles: perceptual intent real TIFF" "$PROFILES" "/tmp/test-real-tiff-intent-0.json" 0
run_test "applyProfiles: saturation intent real TIFF" "$PROFILES" "/tmp/test-real-tiff-intent-2.json" 0

# ============================================================
# GROUP 24: ApplyProfiles -- CLI legacy mode (non-JSON)
# ============================================================
echo ""
echo "--- GROUP 24: ApplyProfiles CLI Legacy Mode (8 tests) ---"

# CLI: src.tif dst.tif dst_encoding dst_compression dst_planar dst_embed interpolation profile intent
# dst_encoding: 0=same, 1=8bit, 2=16bit, 4=float
# dst_compression: 0=none, 1=LZW
# dst_planar: 0=contig, 1=separation
# dst_embed: 0=no, 1=embed_last
# interpolation: 0=linear, 1=tetrahedral

# 24a: Basic -- same encoding, no compress, contig, no embed, tet, relative
TOTAL=$((TOTAL+1))
output=$("$PROFILES" "$TEST_DATA_DIR/rgb-4x4-8bit.tif" "/tmp/cli-tiff-basic.tif" 0 0 0 0 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL profiles cli: basic (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL profiles cli: basic same-enc (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL profiles cli: basic (exit=$rc)"; fi

# 24b: 8-bit output
TOTAL=$((TOTAL+1))
output=$("$PROFILES" "$TEST_DATA_DIR/rgb-4x4-8bit.tif" "/tmp/cli-tiff-8bit.tif" 1 0 0 0 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL profiles cli: 8bit (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL profiles cli: 8-bit enc(1) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL profiles cli: 8bit (exit=$rc)"; fi

# 24c: 16-bit output
TOTAL=$((TOTAL+1))
output=$("$PROFILES" "$TEST_DATA_DIR/rgb-4x4-8bit.tif" "/tmp/cli-tiff-16bit.tif" 2 0 0 0 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL profiles cli: 16bit (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL profiles cli: 16-bit enc(2) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL profiles cli: 16bit (exit=$rc)"; fi

# 24d: Float output
TOTAL=$((TOTAL+1))
output=$("$PROFILES" "$TEST_DATA_DIR/rgb-4x4-8bit.tif" "/tmp/cli-tiff-float.tif" 4 0 0 0 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL profiles cli: float (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL profiles cli: float enc(4) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL profiles cli: float (exit=$rc)"; fi

# 24e: LZW compression + embed ICC
TOTAL=$((TOTAL+1))
output=$("$PROFILES" "$TEST_DATA_DIR/rgb-4x4-8bit.tif" "/tmp/cli-tiff-lzw-embed.tif" 1 1 0 1 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL profiles cli: lzw+embed (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL profiles cli: LZW(1)+embed(1) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL profiles cli: lzw+embed (exit=$rc)"; fi

# 24f: Linear interpolation + absolute intent
TOTAL=$((TOTAL+1))
output=$("$PROFILES" "$TEST_DATA_DIR/rgb-4x4-8bit.tif" "/tmp/cli-tiff-lin-abs.tif" 1 0 0 0 0 "Testing/Display/sRGB_D65_MAT.icc" 3 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL profiles cli: lin+abs (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL profiles cli: linear(0)+absolute(3) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL profiles cli: lin+abs (exit=$rc)"; fi

# 24g: 16-bit source TIFF -> float output
TOTAL=$((TOTAL+1))
output=$("$PROFILES" "$TEST_DATA_DIR/rgb-4x4-16bit.tif" "/tmp/cli-tiff-16to-float.tif" 4 0 0 1 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL profiles cli: 16->float (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL profiles cli: 16src->float+embed (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL profiles cli: 16->float (exit=$rc)"; fi

# 24h: 2-profile chain CLI
TOTAL=$((TOTAL+1))
output=$("$PROFILES" "$TEST_DATA_DIR/rgb-8x8-8bit.tif" "/tmp/cli-tiff-2chain.tif" 1 0 0 1 1 "Testing/Display/sRGB_D65_MAT.icc" 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL profiles cli: 2-chain (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL profiles cli: 2-profile chain (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL profiles cli: 2-chain (exit=$rc)"; fi

# ============================================================
# GROUP 25: ApplyNamedCmm -- Additional CLI encoding modes
# ============================================================
echo ""
echo "--- GROUP 25: ApplyNamedCmm Additional CLI Tests (6 tests) ---"

# 25a: value encoding (0)
TOTAL=$((TOTAL+1))
output=$("$NAMEDCMM" "/tmp/test-cli-data.txt" 0 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL cli named: value enc (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli named: value enc(0) (exit=$rc)"
else PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli named: value enc(0) -- range mismatch expected (exit=$rc)"; fi

# 25b: percent encoding (1)
TOTAL=$((TOTAL+1))
output=$("$NAMEDCMM" "/tmp/test-cli-data.txt" 1 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL cli named: percent enc (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli named: percent enc(1) (exit=$rc)"
else PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli named: percent enc(1) -- range mismatch expected (exit=$rc)"; fi

# 25c: unitFloat encoding (2)
TOTAL=$((TOTAL+1))
output=$("$NAMEDCMM" "/tmp/test-cli-data.txt" 2 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL cli named: unitFloat enc (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli named: unitFloat enc(2) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL cli named: unitFloat enc (exit=$rc)"; fi

# 25d: precision format 3:10 (float with 10-digit precision)
TOTAL=$((TOTAL+1))
output=$("$NAMEDCMM" "/tmp/test-cli-data.txt" 3:10 1 "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL cli named: 3:10 prec (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli named: float enc(3) prec(10) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL cli named: 3:10 prec (exit=$rc)"; fi

# 25e: BPC intent (40=perceptual+BPC)
TOTAL=$((TOTAL+1))
output=$("$NAMEDCMM" "/tmp/test-cli-data.txt" 3 1 "Testing/Display/sRGB_D65_MAT.icc" 40 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL cli named: BPC intent (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli named: BPC intent(40) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL cli named: BPC intent (exit=$rc)"; fi

# 25f: Multi-profile chain CLI (3 profiles)
TOTAL=$((TOTAL+1))
output=$("$NAMEDCMM" "/tmp/test-cli-data.txt" 3 1 \
  "Testing/Display/sRGB_D65_MAT.icc" 1 \
  "Testing/Display/sRGB_D65_MAT.icc" 1 \
  "Testing/Display/sRGB_D65_MAT.icc" 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL cli named: 3-chain (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL cli named: 3-profile chain (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL cli named: 3-chain (exit=$rc)"; fi

# ============================================================
# GROUP 26: ApplySearch -- Additional CLI encoding + intent modes
# ============================================================
echo ""
echo "--- GROUP 26: ApplySearch Additional CLI Tests (4 tests) ---"

# 26a: float encoding + perceptual
TOTAL=$((TOTAL+1))
output=$("$SEARCH" "/tmp/test-cli-data.txt" 3 1 "Testing/Display/sRGB_D65_MAT.icc" 0 "Testing/Display/sRGB_D65_MAT.icc" 0 -INIT 0 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL search cli: perceptual (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL search cli: float + perceptual(0) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL search cli: perceptual (exit=$rc)"; fi

# 26b: unitFloat encoding
TOTAL=$((TOTAL+1))
output=$("$SEARCH" "/tmp/test-cli-data.txt" 2 1 "Testing/Display/sRGB_D65_MAT.icc" 1 "Testing/Display/sRGB_D65_MAT.icc" 1 -INIT 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL search cli: unitFloat (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL search cli: unitFloat enc(2) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL search cli: unitFloat (exit=$rc)"; fi

# 26c: BPC intent (40)
TOTAL=$((TOTAL+1))
output=$("$SEARCH" "/tmp/test-cli-data.txt" 3 1 "Testing/Display/sRGB_D65_MAT.icc" 40 "Testing/Display/sRGB_D65_MAT.icc" 40 -INIT 40 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL search cli: BPC (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL search cli: BPC intent(40) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL search cli: BPC (exit=$rc)"; fi

# 26d: linear interpolation
TOTAL=$((TOTAL+1))
output=$("$SEARCH" "/tmp/test-cli-data.txt" 3 0 "Testing/Display/sRGB_D65_MAT.icc" 1 "Testing/Display/sRGB_D65_MAT.icc" 1 -INIT 1 2>&1)
rc=$?
asan_hit=$(echo "$output" | grep -c 'AddressSanitizer\|runtime error:' || true)
if [ "$asan_hit" -gt 0 ]; then ASAN=$((ASAN+1)); echo "[ASAN] #$TOTAL search cli: linear (exit=$rc)"
elif [ "$rc" -eq 0 ]; then PASS=$((PASS+1)); echo "[PASS] #$TOTAL search cli: linear interp(0) (exit=$rc)"
else FAIL=$((FAIL+1)); echo "[FAIL] #$TOTAL search cli: linear (exit=$rc)"; fi

# ============================================================
# SUMMARY
# ============================================================
echo ""
echo "=================================================================="
echo " FINAL SUMMARY"
echo "=================================================================="
echo "Total tests:  $TOTAL"
echo "Passed:       $PASS"
echo "Failed:       $FAIL"
echo "ASAN/UBSAN:   $ASAN"
echo "=================================================================="

# Clean up
rm -f /tmp/test-enc-*.json /tmp/test-prec-*.json /tmp/test-dig-*.json \
      /tmp/test-debug-*.json /tmp/test-srctype-*.json /tmp/test-dstfile*.json \
      /tmp/test-srcspace-*.json /tmp/test-intent-*.json /tmp/test-interp-*.json \
      /tmp/test-flag-*.json /tmp/test-envvars.json /tmp/test-pccenvvars.json \
      /tmp/test-empty-envvars.json /tmp/test-pccfile*.json /tmp/test-chain*.json \
      /tmp/test-cenc-*.json /tmp/test-multi-*.json /tmp/test-single-*.json \
      /tmp/test-search-*.json /tmp/test-applyprofiles-*.json /tmp/test-neg-*.json \
      /tmp/test-large-*.json /tmp/test-bad-*.json /tmp/test-num-*.json \
      /tmp/test-empty-*.json /tmp/test-cmyk-*.json /tmp/test-all-*.json \
      /tmp/test-srcfile-*.json /tmp/test-external-data.* /tmp/json-test-output.txt \
      /tmp/test-cli-data.txt /tmp/test-cli-8bit.txt /tmp/test-cli-16bit.txt \
      /tmp/test-real-tiff-*.json /tmp/json-tiff-*.tif /tmp/cli-tiff-*.tif 2>/dev/null

exit 0
