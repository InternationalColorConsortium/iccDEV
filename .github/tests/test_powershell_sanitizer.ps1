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

# -----------------------------------------------------------------------------
# Embedded NUL (U+0000) -- the coverage the Bash suite cannot express
#
# test_sanitization.sh carries three "NUL-stripped payload after Bash argv
# conversion" cases whose input and expected value are the SAME NUL-free string,
# with a comment saying why: Bash cannot hold a NUL in a variable or a command
# substitution, so those cases record the limitation instead of exercising the
# strip.  A .NET string is length-prefixed and holds U+0000 fine, so this side is
# the only one that can reach it, and until now nothing did.
#
# Two different mechanisms drop it, so the cases below pin two different things.
# Sanitize-Line drops NUL through the 0x20..0x7E / >= 0xA0 keep-set in
# Strip-CtrlRemoveNewlines, the same filter the tab and C1 cases above cover.
# Sanitize-Ref and Sanitize-Filename drop it through the explicit NUL strip in
# Sanitize-Ref, which nothing else here reaches: Strip-UnicodeControl runs
# first but leaves NUL alone, matching \p{Cf} where U+0000 is category Cc.
#
# Those two discriminate against that strip's removal rather than merely passing
# -- a surviving NUL falls through to the [^A-Za-z0-9._/-] rule and becomes a
# dash, giving "branch-hidden" and "evil.php-.jpg" instead of the values below.
# -----------------------------------------------------------------------------

Assert-Eq "NUL stripped from line" `
    "testnullbyte" `
    (Sanitize-Line -InputString "test$([char]0)null$([char]0)byte")

Assert-Eq "NUL stripped from ref, not replaced with a dash" `
    "branchhidden" `
    (Sanitize-Ref -InputString "branch$([char]0)hidden")

Assert-Eq "NUL stripped from filename, not replaced with a dash" `
    "evil.php.jpg" `
    (Sanitize-Filename -InputString "evil.php$([char]0).jpg")

# -----------------------------------------------------------------------------
# Coverage retained from the retired test_sanitization.ps1
#
# That suite was orphaned and pinned to v3, so it went; but five of the things
# it touched are exercised nowhere else ON THIS SIDE.  test_sanitization.sh
# covers them all against the Bash sanitizer, which says nothing about this
# one -- implementation parity is the whole reason this file exists, so
# "covered in the twin" is not coverage here.
#
# 1. Escape-Html's & and " branches.  The "HTML escaping" case at the top of
#    this file feeds <script>alert('xss')</script>, which carries neither, so
#    either Replace could be deleted with this job still green while raw & and "
#    reach GITHUB_STEP_SUMMARY.  Same input and expectation as
#    test_sanitization.sh:88 so the two stay comparable.
# 2. Truncate-String and SANITIZE_LINE_MAXLEN.  Derived from the live value
#    rather than hardcoded, because both MAXLENs are env-overridable.  Asserts
#    the exact result, which also pins that the ellipsis is inside the limit
#    rather than appended past it.
# 3. Sanitize-Print's (LF){4,} collapse to three, AND its Escape-Html call.
#    The one Sanitize-Print case above covers C1 stripping and carries no HTML
#    special, so deleting the Escape-Html line from Sanitize-Print left this
#    job green while a raw <script> reached GITHUB_STEP_SUMMARY -- item 1's
#    hole again, on the block sink.  One payload now covers both rules.
# 4. Astral characters surviving -- once through EACH keep-set, which are
#    separate functions.  Every preservation case here stopped at the BMP
#    (U+00A0, U+00E9) and the one astral case asserts a tag character is
#    REMOVED, so excluding 0xD800..0xDFFF -- a plausible "drop unpaired
#    surrogates" hardening -- was green on both paths.
#      - Sanitize-Print / Strip-CtrlKeepNewlines is the GITHUB_STEP_SUMMARY
#        sink (ci-pr-win.yml:892, :1042, :1185).  Its whole ">= 0xA0" keep
#        branch had NO case at all: the two Sanitize-Print assertions in this
#        file are "log<U+009B>2K" and the HTML payload above, and 0x9B is
#        below 0xA0, so both are pure ASCII on that path.  Deleting that
#        clause outright stripped every emoji, accented letter and CJK
#        character from a step summary with every assertion still passing.
#        The emoji case covers it.
#      - Sanitize-Line / Strip-CtrlRemoveNewlines has the BMP cases above but
#        no astral one.  The mathematical-bold case covers it.
#    Built with ConvertFromUtf32 to keep this file ASCII, as the
#    tag-character case above does.
# 5. Backslash is NOT a path separator to Sanitize-Filename.  Only "/" is
#    mapped to "_" (sanitize.ps1:406); "\" falls in the [^A-Za-z0-9._/-]
#    class and becomes "-".  The surviving filename case at the top feeds
#    ../../etc/passwd, which has no backslash, so a later "map \ to _ as
#    well" change would go unnoticed on the side this file exists to pin.
#    Retired suite's exact input and expectation.
# -----------------------------------------------------------------------------

Assert-Eq "All HTML special chars" `
    "A&amp;B &lt;tag&gt; &quot;quoted&quot; &#39;single&#39;" `
    (Sanitize-Line -InputString "A&B <tag> `"quoted`" 'single'")

$maxLen = $script:SANITIZE_LINE_MAXLEN
Assert-Eq "Over-length line truncated to MAXLEN with an ellipsis" `
    (("A" * ($maxLen - 3)) + "...") `
    (Sanitize-Line -InputString ("A" * ($maxLen * 2)))

Assert-Eq "Line exactly at MAXLEN not truncated" `
    ("A" * $maxLen) `
    (Sanitize-Line -InputString ("A" * $maxLen))

Assert-Eq "Sanitize-Print collapses newline runs to three and escapes HTML" `
    "&lt;a&gt;`n`n`n&amp;b &#39;c&#39;" `
    (Sanitize-Print -InputString "<a>`n`n`n`n`n`n&b 'c'")

Assert-Eq "Astral emoji survives Sanitize-Print" `
    ("lock" + [char]::ConvertFromUtf32(0x1F512)) `
    (Sanitize-Print -InputString ("lock" + [char]::ConvertFromUtf32(0x1F512)))

Assert-Eq "Mathematical bold survives Sanitize-Line" `
    ([char]::ConvertFromUtf32(0x1D407) + "ello") `
    (Sanitize-Line -InputString ([char]::ConvertFromUtf32(0x1D407) + "ello"))

Assert-Eq "Backslash neutralized, not treated as a path separator" `
    "..-..-windows-system32" `
    (Sanitize-Filename -InputString "..\..\\windows\system32")

Assert-Eq "Version marker" `
    "iccDEV-sanitizer-v4" `
    (Sanitizer-Version)

Write-Host "PowerShell sanitizer helper tests: $pass passed, $fail failed"

if ($fail -ne 0) {
    exit 1
}
