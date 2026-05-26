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

Assert-Eq "Version marker" `
    "iccDEV-sanitizer-v4" `
    (Sanitizer-Version)

Write-Host "PowerShell sanitizer helper tests: $pass passed, $fail failed"

if ($fail -ne 0) {
    exit 1
}
