###############################################################
#
# Copyright (c) 2026 International Color Consortium.
#                 All rights reserved.
#                 https://color.org
#
# Intent: Test the canonical PowerShell sanitizer helper.
#
###############################################################

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent (Split-Path -Parent $scriptDir)
$sanitizeScript = Join-Path $repoRoot '.github/scripts/sanitize.ps1'

if (-not (Test-Path $sanitizeScript)) {
    Write-Error "ERROR: Cannot find sanitize.ps1 at $sanitizeScript"
    exit 1
}

. $sanitizeScript

$script:pass = 0
$script:fail = 0

function Assert-Eq {
    param(
        [string]$Name,
        [string]$Expected,
        [string]$Actual
    )

    if ($Actual -eq $Expected) {
        Write-Host "  [PASS] $Name"
        $script:pass++
    } else {
        Write-Host "  [FAIL] $Name"
        Write-Host "    Expected: [$Expected]"
        Write-Host "    Actual:   [$Actual]"
        $script:fail++
    }
}

Write-Host "=== PowerShell sanitizer helper tests ==="

Assert-Eq "HTML escaping" `
    "&lt;script&gt;alert(&#39;xss&#39;)&lt;/script&gt;" `
    (Sanitize-Line -InputString "<script>alert('xss')</script>")

Assert-Eq "ANSI CSI stripped" `
    "Red Text" `
    (Sanitize-Line -InputString "$([char]0x1B)[31mRed$([char]0x1B)[0m Text")

Assert-Eq "ANSI OSC stripped" `
    "safe" `
    (Sanitize-Line -InputString "safe$([char]0x1B)]0;spoof$([char]0x07)")

Assert-Eq "Zero-width stripped" `
    "zerowidth" `
    (Sanitize-Line -InputString "zero$([char]0x200B)width")

Assert-Eq "Bidi stripped" `
    "safetext" `
    (Sanitize-Line -InputString "safe$([char]0x202E)text")

Assert-Eq "Word joiner stripped" `
    "testvalue" `
    (Sanitize-Line -InputString "test$([char]0x2060)value")

Assert-Eq "Line separator stripped" `
    "linebreak" `
    (Sanitize-Line -InputString "line$([char]0x2028)break")

Assert-Eq "Tag character stripped" `
    "tagchar" `
    (Sanitize-Line -InputString "tag$([char]::ConvertFromUtf32(0xE0061))char")

Assert-Eq "Tab stripped from line" `
    "col1col2 Line2" `
    (Sanitize-Line -InputString "col1`tcol2`nLine2")

Assert-Eq "Ref hidden chars stripped and metacharacters neutralized" `
    "feature/-refs-test" `
    (Sanitize-Ref -InputString "feature/$([char]0x202E)`$(refs)test")

Assert-Eq "Filename path separators neutralized" `
    ".._.._etc_passwd" `
    (Sanitize-Filename -InputString "../../etc/passwd")

# -----------------------------------------------------------------------------
# C1 controls (U+0080-U+009F) -- Bash/PowerShell parity anchors
#
# Strip-CtrlRemoveNewlines keeps 0x20..0x7E plus >= 0xA0, so this block is already
# dropped here; U+009B is CSI, the same introducer the ANSI rule above strips in
# its ESC-bracket form.  The Bash side could not see it at all until now: its C0
# strip is tr, which works on BYTES, and UTF-8 spells this block C2 80..C2 9F.
# These cases pin the behaviour sanitize-sed.sh was just corrected to match, so a
# future edit to either script cannot silently drift from the other.
# -----------------------------------------------------------------------------

Assert-Eq "C1 CSI U+009B stripped" `
    "log2K1Gsp" `
    (Sanitize-Line -InputString "log$([char]0x9B)2K$([char]0x9B)1Gsp")

Assert-Eq "C1 low bound U+0080 stripped" `
    "ab" `
    (Sanitize-Line -InputString "a$([char]0x80)b")

Assert-Eq "C1 high bound U+009F stripped" `
    "ab" `
    (Sanitize-Line -InputString "a$([char]0x9F)b")

Assert-Eq "U+00A0 just above C1 preserved" `
    "a$([char]0xA0)b" `
    (Sanitize-Line -InputString "a$([char]0xA0)b")

Assert-Eq "Latin-1 letter above C1 preserved" `
    "a$([char]0xE9)b" `
    (Sanitize-Line -InputString "a$([char]0xE9)b")

Assert-Eq "C1 CSI stripped by Sanitize-Print" `
    "log2K" `
    (Sanitize-Print -InputString "log$([char]0x9B)2K")

Assert-Eq "Version marker" `
    "iccDEV-sanitizer-v4" `
    (Sanitizer-Version)

Write-Host "PowerShell sanitizer helper tests: $pass passed, $fail failed"

if ($fail -ne 0) {
    exit 1
}
