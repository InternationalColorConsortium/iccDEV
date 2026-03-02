###############################################################
#
## Copyright (©) 2024-2025 David H Hoyt. All rights reserved.
##                 https://srd.cx
##
## Last Updated:  16-DEC-2025-2025 1400Z by David Hoyt
#
## Intent:test_sanitization.ps1
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


$ErrorActionPreference = "Stop"

# Source the canonical sanitizer
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$sanitizeScript = Join-Path $scriptDir "sanitize.ps1"

if (Test-Path $sanitizeScript) {
    . $sanitizeScript
} else {
    Write-Error "ERROR: Cannot find sanitize.ps1 in $scriptDir"
    exit 1
}

Write-Host "=========================================="
Write-Host "Testing Sanitization Functions"
Write-Host "=========================================="
Write-Host ""

$script:pass = 0
$script:fail = 0

function Run-Test {
    param(
        [string]$TestName,
        [string]$Input,
        [string]$Expected,
        [string]$Function = "Sanitize-Line"
    )
    
    $testNum = $script:pass + $script:fail + 1
    Write-Host "Test ${testNum}: $TestName"
    $inputLine = "  Input:    " + $Input
    $expectedLine = "  Expected: " + $Expected
    Write-Host $inputLine
    Write-Host $expectedLine
    
    $result = switch ($Function) {
        "Sanitize-Line" { Sanitize-Line -InputString $Input }
        "Sanitize-Print" { Sanitize-Print -InputString $Input }
        "Sanitize-Ref" { Sanitize-Ref -InputString $Input }
        "Sanitize-Filename" { Sanitize-Filename -InputString $Input }
        "Escape-Html" { Escape-Html -InputString $Input }
        default { Sanitize-Line -InputString $Input }
    }
    
    $resultLine = "  Result:   " + $result
    Write-Host $resultLine
    
    if ($result -eq $Expected) {
        Write-Host "  ✅ PASS" -ForegroundColor Green
        $script:pass++
    } else {
        Write-Host "  ❌ FAIL" -ForegroundColor Red
        $script:fail++
    }
    Write-Host ""
}

# =============================================================================
# HTML Entity Escaping Tests
# =============================================================================

Run-Test -TestName "Basic XSS payload" `
    -Input "<script>alert('xss')</script>" `
    -Expected "&lt;script&gt;alert(&#39;xss&#39;)&lt;/script&gt;"

Run-Test -TestName "All HTML special chars" `
    -Input "A&B <tag> `"quoted`" 'single'" `
    -Expected "A&amp;B &lt;tag&gt; &quot;quoted&quot; &#39;single&#39;"

Run-Test -TestName "Realistic cppcheck output" `
    -Input "[IccTagBasic.cpp:42]: (warning) Member variable 'IccTag::m_nReserved' is not initialized." `
    -Expected "[IccTagBasic.cpp:42]: (warning) Member variable &#39;IccTag::m_nReserved&#39; is not initialized."

Run-Test -TestName "Empty string" `
    -Input "" `
    -Expected ""

Run-Test -TestName "Normal text (no special chars)" `
    -Input "Normal text with numbers 123 and letters abc" `
    -Expected "Normal text with numbers 123 and letters abc"

# =============================================================================
# Unicode and Charset Tests
# =============================================================================

Run-Test -TestName "Unicode characters (UTF-8)" `
    -Input "Hello 世界 مرحبا 🌍" `
    -Expected "Hello 世界 مرحبا 🌍"

Run-Test -TestName "Unicode with HTML entities" `
    -Input "<div>Hello 世界 & 'test'</div>" `
    -Expected "&lt;div&gt;Hello 世界 &amp; &#39;test&#39;&lt;/div&gt;"

Run-Test -TestName "Emoji and special symbols" `
    -Input "✅ PASS ❌ FAIL 🔒 Security" `
    -Expected "✅ PASS ❌ FAIL 🔒 Security"

Run-Test -TestName "Mixed RTL/LTR text with entities" `
    -Input "English & العربية <tag>" `
    -Expected "English &amp; العربية &lt;tag&gt;"

# =============================================================================
# Control Character and Injection Tests
# =============================================================================

Run-Test -TestName "Null bytes removed" `
    -Input "test`0null`0byte" `
    -Expected "testnullbyte"

Run-Test -TestName "Carriage return removed" `
    -Input "line1`r`nline2" `
    -Expected "line1 line2"

Run-Test -TestName "Tab preserved then converted to space" `
    -Input "line1`tTab`nLine2" `
    -Expected "line1`tTab Line2"

Run-Test -TestName "Bell and other control chars" `
    -Input "test$([char]0x07)bell$([char]0x08)backspace" `
    -Expected "testbellbackspace"

# =============================================================================
# Homograph and Lookalike Attack Tests
# =============================================================================

Run-Test -TestName "Cyrillic lookalikes" `
    -Input "Аdmin (Cyrillic A)" `
    -Expected "Аdmin (Cyrillic A)"

Run-Test -TestName "Mathematical bold/italic (preserved)" `
    -Input "𝐇𝐞𝐥𝐥𝐨 𝑾𝒐𝒓𝒍𝒅" `
    -Expected "𝐇𝐞𝐥𝐥𝐨 𝑾𝒐𝒓𝒍𝒅"

# =============================================================================
# Truncation and Length Tests
# =============================================================================

Write-Host "Test $($script:pass + $script:fail + 1): Long input truncation (2000 chars)"
$longInput = "A" * 2000
$result = Sanitize-Line -InputString $longInput
$resultLen = $result.Length
Write-Host "  Input length: 2000"
Write-Host "  Result length: $resultLen"
Write-Host "  Max allowed: $script:SANITIZE_LINE_MAXLEN"
if ($resultLen -le $script:SANITIZE_LINE_MAXLEN) {
    Write-Host "  ✅ PASS (truncated correctly)" -ForegroundColor Green
    $script:pass++
} else {
    Write-Host "  ❌ FAIL (not truncated)" -ForegroundColor Red
    $script:fail++
}
Write-Host ""

# =============================================================================
# XSS and Injection Vector Tests
# =============================================================================

Run-Test -TestName "HTML event handler injection" `
    -Input "<img src=x onerror='alert(1)'>" `
    -Expected "&lt;img src=x onerror=&#39;alert(1)&#39;&gt;"

Run-Test -TestName "JavaScript protocol" `
    -Input "javascript:alert('xss')" `
    -Expected "javascript:alert(&#39;xss&#39;)"

Run-Test -TestName "Data URI with base64" `
    -Input "data:text/html;base64,PHNjcmlwdD5hbGVydCgxKTwvc2NyaXB0Pg==" `
    -Expected "data:text/html;base64,PHNjcmlwdD5hbGVydCgxKTwvc2NyaXB0Pg=="

Run-Test -TestName "SVG with script" `
    -Input "<svg onload=alert(1)>" `
    -Expected "&lt;svg onload=alert(1)&gt;"

Run-Test -TestName "Markdown injection attempt" `
    -Input "[Click me](javascript:alert('xss'))" `
    -Expected "[Click me](javascript:alert(&#39;xss&#39;))"

Run-Test -TestName "HTML comment injection" `
    -Input "<!-- <script>alert(1)</script> -->" `
    -Expected "&lt;!-- &lt;script&gt;alert(1)&lt;/script&gt; --&gt;"

Run-Test -TestName "CSS expression injection" `
    -Input "style='expression(alert(1))'" `
    -Expected "style=&#39;expression(alert(1))&#39;"

Run-Test -TestName "XML entity expansion attempt" `
    -Input "<!ENTITY xxe SYSTEM `"file:///etc/passwd`">" `
    -Expected "&lt;!ENTITY xxe SYSTEM &quot;file:///etc/passwd&quot;&gt;"

Run-Test -TestName "CDATA section" `
    -Input "<![CDATA[<script>alert(1)</script>]]>" `
    -Expected "&lt;![CDATA[&lt;script&gt;alert(1)&lt;/script&gt;]]&gt;"

Run-Test -TestName "Server-side template injection" `
    -Input '{{7*7}} ${7*7} <%= 7*7 %>' `
    -Expected '{{7*7}} ${7*7} &lt;%= 7*7 %&gt;'

Run-Test -TestName "Path traversal in filename" `
    -Input "../../../etc/shadow" `
    -Expected ".._.._.._etc_shadow" `
    -Function "Sanitize-Filename"

Run-Test -TestName "Windows path traversal" `
    -Input "..\..\\windows\system32" `
    -Expected "..-..-windows-system32" `
    -Function "Sanitize-Filename"

Run-Test -TestName "Command injection attempt" `
    -Input "test; rm -rf / #" `
    -Expected "test; rm -rf / #"

Run-Test -TestName "SQL injection pattern" `
    -Input "' OR '1'='1" `
    -Expected "&#39; OR &#39;1&#39;=&#39;1"

Run-Test -TestName "LDAP injection pattern" `
    -Input "*()|&" `
    -Expected "*()|&amp;"

# =============================================================================
# Edge Cases
# =============================================================================

Run-Test -TestName "Multiple consecutive entities" `
    -Input "&&&&<<<<>>>>" `
    -Expected "&amp;&amp;&amp;&amp;&lt;&lt;&lt;&lt;&gt;&gt;&gt;&gt;"

Run-Test -TestName "Already encoded entities (double encoding)" `
    -Input "&lt;script&gt;" `
    -Expected "&amp;lt;script&amp;gt;"

Run-Test -TestName "Mixed quotes" `
    -Input "test `"'`" nested" `
    -Expected "test &quot;&#39;&quot; nested"

Run-Test -TestName "Backslash and special chars" `
    -Input "C:\Windows\System32 & 'cmd'" `
    -Expected "C:\Windows\System32 &amp; &#39;cmd&#39;"

# =============================================================================
# sanitize_print Multi-line Tests
# =============================================================================

Write-Host "Test $($script:pass + $script:fail + 1): Multi-line with Sanitize-Print"
$multilineInput = @"
Line 1: <error> & 'quote'
Line 2: "test"
Line 3: Normal
"@
$result = Sanitize-Print -InputString $multilineInput
if ($result -match "&lt;error&gt;" -and $result -match "&amp;" -and $result -match "&#39;") {
    Write-Host "  ✅ PASS (contains expected entities)" -ForegroundColor Green
    $script:pass++
} else {
    Write-Host "  ❌ FAIL" -ForegroundColor Red
    $script:fail++
}
Write-Host ""

# =============================================================================
# sanitize_ref and sanitize_filename Tests
# =============================================================================

Run-Test -TestName "Ref sanitization: branch name" `
    -Input "feature/test-123" `
    -Expected "feature/test-123" `
    -Function "Sanitize-Ref"

Run-Test -TestName "Ref sanitization: dangerous chars replaced" `
    -Input "feature`$test<>|branch" `
    -Expected "feature-test-branch" `
    -Function "Sanitize-Ref"

Run-Test -TestName "Filename sanitization: slashes to underscores" `
    -Input "../../etc/passwd" `
    -Expected ".._.._etc_passwd" `
    -Function "Sanitize-Filename"

# =============================================================================
# Excessive newline collapse test
# =============================================================================

Write-Host "Test $($script:pass + $script:fail + 1): Excessive newlines collapsed to 3"
$excessiveNewlines = "Line1`n`n`n`n`n`n`nLine2"
$result = Sanitize-Print -InputString $excessiveNewlines
$newlineCount = ([regex]::Matches($result, "`n")).Count
if ($newlineCount -le 3) {
    Write-Host "  ✅ PASS (newlines collapsed to $newlineCount)" -ForegroundColor Green
    $script:pass++
} else {
    Write-Host "  ❌ FAIL (still has $newlineCount newlines)" -ForegroundColor Red
    $script:fail++
}
Write-Host ""

# =============================================================================
# Sanitizer version test
# =============================================================================

Write-Host "Test $($script:pass + $script:fail + 1): Sanitizer version"
$version = Sanitizer-Version
if ($version -eq "iccDEV-sanitizer-v3") {
    Write-Host "  ✅ PASS (version: $version)" -ForegroundColor Green
    $script:pass++
} else {
    Write-Host "  ❌ FAIL (unexpected version: $version)" -ForegroundColor Red
    $script:fail++
}
Write-Host ""

# =============================================================================
# Results Summary
# =============================================================================

Write-Host "=========================================="
Write-Host "Results: $script:pass passed, $script:fail failed"
Write-Host "=========================================="

if ($script:fail -eq 0) {
    Write-Host "✅ All tests PASSED" -ForegroundColor Green
    exit 0
} else {
    Write-Host "❌ Some tests FAILED" -ForegroundColor Red
    exit 1
}
