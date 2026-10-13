<#
.SYNOPSIS
  Concurrent load & stress test for iccIisIsapi.dll on IIS.

.DESCRIPTION
  Launches multiple parallel sessions hitting every endpoint with normal and
  malicious payloads. Requires PowerShell 7 or later because the concurrent
  phases use ForEach-Object -Parallel. Tests:
    1. Health endpoint (GET)           - high concurrency, rapid fire
    2. Summary endpoint (GET)          - concurrent reads
    3. XML mode endpoint (GET)         - concurrent reads
    4. 405 enforcement (wrong method)  - POST to GET-only, GET to POST-only
    5. File upload (POST /tools)       - concurrent uploads with fuzz vectors
    6. Query string injection          - XSS / SQLi / path traversal in params
    7. Oversized requests              - body > 16MB
    8. Malformed Content-Type          - unusual MIME types
    9. Rapid reconnect                 - connection churn
   10. URI scheme injection            - javascript:/data: in query params

.PARAMETER BaseUrl
  The IIS site URL (default: http://127.0.0.1:18081)

.PARAMETER FuzzDir
  Path to the xsscx/fuzz corpus (optional, enables fuzz payload injection)

.PARAMETER Concurrency
  Number of parallel sessions per test phase (default: 20)

.PARAMETER Duration
  How many seconds to sustain each phase (default: 10)

.PARAMETER ProfilePath
  Path to a valid .icc file for upload tests

.EXAMPLE
  pwsh -File .\Stress-IccIisIsapi.ps1 -BaseUrl http://localhost:18081 -FuzzDir E:\xss\fuzz -Concurrency 50
#>

[CmdletBinding()]
param(
  [string]$BaseUrl     = 'http://127.0.0.1:18081',
  [string]$FuzzDir     = '',
  [ValidateRange(1, 256)]
  [int]   $Concurrency = 20,
  [ValidateRange(1, 600)]
  [int]   $Duration    = 10,
  [string]$ProfilePath = ''
)

if ($PSVersionTable.PSVersion.Major -lt 7) {
  throw "Stress-IccIisIsapi.ps1 requires PowerShell 7 or later. Run it with pwsh."
}

$ErrorActionPreference = 'Stop'
$dll = "$BaseUrl/iccIisIsapi.dll"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

function Write-Phase([string]$Name) {
  Write-Host "`n=== Phase: $Name ===" -ForegroundColor Cyan
}

function Test-Endpoint {
  param(
    [string]$Url,
    [string]$Method = 'GET',
    [object]$Body = $null,
    [string]$ContentType = $null,
    [hashtable]$Headers = @{},
    [int]$TimeoutSec = 10
  )
  try {
    if ($Method -eq 'POST' -and -not $Headers.ContainsKey('X-ICCDEV-Request')) {
      $Headers['X-ICCDEV-Request'] = '1'
    }
    $params = @{
      Uri              = $Url
      Method           = $Method
      UseBasicParsing  = $true
      TimeoutSec       = $TimeoutSec
      SkipHttpErrorCheck = $true
    }
    if ($null -ne $Body) {
      $params.Body = if ($Body -is [string]) { [System.Text.Encoding]::UTF8.GetBytes($Body) } else { $Body }
    }
    if ($ContentType) { $params.ContentType = $ContentType }
    if ($Headers.Count -gt 0) { $params.Headers = $Headers }

    $r = Invoke-WebRequest @params
    return @{
      Status  = $r.StatusCode
      Length  = $r.Content.Length
      Headers = $r.Headers
      Content = [string]$r.Content
      Ok      = $true
    }
  } catch {
    $code = 0
    if ($_.Exception.Response) {
      $code = [int]$_.Exception.Response.StatusCode
    }
    return @{
      Status = $code
      Error  = $_.Exception.Message
      Ok     = $false
    }
  }
}

function Run-ConcurrentBatch {
  param(
    [scriptblock]$Action,
    [int]$Count = $Concurrency,
    [string]$Label = 'batch'
  )
  $jobs = 1..$Count | ForEach-Object {
    Start-Job -ScriptBlock $Action -ArgumentList $dll, $_, $Duration
  }
  $results = $jobs | Wait-Job -Timeout ($Duration + 30) | Receive-Job
  $jobs | Remove-Job -Force -ErrorAction SilentlyContinue
  return $results
}

# ---------------------------------------------------------------------------
# Load fuzz payloads
# ---------------------------------------------------------------------------

$xssPayloads = @(
  '<script->alert(1)</script>',
  '<img src=x onerror=alert(1)>',
  "'""><img src=x onerror=alert(1)>",
  '<svg/onload=alert(1)>',
  'javascript:alert(document.cookie)',
  'data:text/html,<script->alert(1)</script>',
  '"><iframe src="javascript:alert(1)">',
  '<a href="javascript:alert(1)">click</a>',
  "' OR 1=1 --",
  '../../etc/passwd',
  '..\..\windows\system32\config\sam',
  '%00<script->alert(1)</script>',
  '%0A%0DSet-Cookie: evil=true',
  'file:///etc/passwd',
  '<![CDATA[<script->alert(1)</script>]]>'
)

$uriPayloads = @(
  'javascript:alert(1)',
  'JAVASCRIPT:alert(1)',
  'data:text/html;base64,PHNjcmlwdD5hbGVydCgxKTwvc2NyaXB0Pg==',
  'vbscript:MsgBox(1)',
  'blob:http://evil.com/uuid',
  'mhtml:http://evil.com/file.mht!1.html',
  "java`tscript:alert(1)",
  "java`nscript:alert(1)"
)

if ($FuzzDir -and (Test-Path "$FuzzDir\javascript")) {
  Write-Host "Loading fuzz payloads from $FuzzDir..."
  $xssFile = "$FuzzDir\no-experience-required-xss-signatures-only-fools-dont-use.txt"
  if (Test-Path $xssFile) {
    $extraXss = Get-Content $xssFile -TotalCount 200 | Where-Object { $_.Trim().Length -gt 0 }
    $xssPayloads += $extraXss
  }
  $uriFiles = Get-ChildItem "$FuzzDir\uri" -File -Filter '*.txt' -ErrorAction SilentlyContinue
  foreach ($f in $uriFiles) {
    $lines = Get-Content $f.FullName -TotalCount 50 | Where-Object { $_.Trim().Length -gt 0 }
    $uriPayloads += $lines
  }
  Write-Host "  XSS payloads: $($xssPayloads.Count), URI payloads: $($uriPayloads.Count)"
}

# ---------------------------------------------------------------------------
# Connectivity check
# ---------------------------------------------------------------------------

Write-Host "`n=== IIS ISAPI Concurrent Stress Test ===" -ForegroundColor Yellow
Write-Host "Target: $dll"
Write-Host "Concurrency: $Concurrency, Duration: ${Duration}s per phase"

$healthCheck = Test-Endpoint -Url "$dll`?mode=health"
if (-not $healthCheck.Ok -or $healthCheck.Status -ne 200 -or $healthCheck.Content.Trim() -ne 'ok') {
  Write-Host "FATAL: Cannot reach $dll - is IIS running?" -ForegroundColor Red
  Write-Host "Run: .\Install-IccIisIsapiSite.ps1 -Layout Build  (or -Layout Install)" -ForegroundColor Yellow
  exit 1
}
Write-Host "Health check: $($healthCheck.Status) OK" -ForegroundColor Green

$stats = @{ Total = 0; Pass = 0; Fail = 0; Skipped = 0; Errors = @() }

function Assert-Check([bool]$Condition, [string]$Label) {
  $stats.Total++
  if ($Condition) {
    $stats.Pass++
  }
  else {
    $stats.Fail++
    $stats.Errors += $Label
    Write-Host "  [FAIL] $Label" -ForegroundColor Red
  }
}

function Record-Result([hashtable]$r, [string]$Label, [int]$ExpectedStatus = 200) {
  $stats.Total++
  if ($r.Ok -and $r.Status -eq $ExpectedStatus) {
    $stats.Pass++
  } elseif (-not $r.Ok -and $ExpectedStatus -ge 400 -and $r.Status -eq $ExpectedStatus) {
    $stats.Pass++
  } else {
    $stats.Fail++
    $stats.Errors += "$Label : got $($r.Status), expected $ExpectedStatus"
  }
}

# ---------------------------------------------------------------------------
# Phase 1: Health endpoint - high concurrency rapid fire
# ---------------------------------------------------------------------------

Write-Phase "1/10 - Health endpoint rapid fire (${Concurrency} concurrent)"

$healthResults = 1..$Concurrency | ForEach-Object -Parallel {
  $dll = $using:dll
  $count = 0; $ok = 0; $fail = 0
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  while ($sw.Elapsed.TotalSeconds -lt $using:Duration) {
    try {
      $r = Invoke-WebRequest -Uri "$dll`?mode=health" -UseBasicParsing -TimeoutSec 5
      if ($r.StatusCode -eq 200) { $ok++ } else { $fail++ }
    } catch { $fail++ }
    $count++
  }
  [PSCustomObject]@{ Worker = $_; Requests = $count; Ok = $ok; Fail = $fail }
} -ThrottleLimit $Concurrency

$totalReqs = ($healthResults | Measure-Object -Property Requests -Sum).Sum
$totalOk   = ($healthResults | Measure-Object -Property Ok -Sum).Sum
$totalFail = ($healthResults | Measure-Object -Property Fail -Sum).Sum
Write-Host "  Requests: $totalReqs, OK: $totalOk, Fail: $totalFail, RPS: $([math]::Round($totalReqs / $Duration, 1))"
if ($totalFail -gt 0) { Write-Host "  WARNING: $totalFail failures under load" -ForegroundColor Yellow }

# ---------------------------------------------------------------------------
# Phase 2: Summary + XML - concurrent reads
# ---------------------------------------------------------------------------

Write-Phase "2/10 - Summary and XML concurrent reads"

$readResults = 1..($Concurrency * 2) | ForEach-Object -Parallel {
  $dll = $using:dll
  $endpoint = if ($_ % 2 -eq 0) { "$dll" } else { "$dll`?format=xml" }
  try {
    $r = Invoke-WebRequest -Uri $endpoint -UseBasicParsing -TimeoutSec 10
    [PSCustomObject]@{ Status = $r.StatusCode; Len = $r.Content.Length; Endpoint = $endpoint }
  } catch {
    [PSCustomObject]@{ Status = 0; Len = 0; Endpoint = $endpoint }
  }
} -ThrottleLimit $Concurrency

$readOk = ($readResults | Where-Object { $_.Status -eq 200 }).Count
Write-Host "  Total: $($readResults.Count), OK: $readOk, Fail: $($readResults.Count - $readOk)"
Assert-Check ($readResults.Count -eq ($Concurrency * 2) -and $readOk -eq $readResults.Count) 'Summary/XML reads failed or returned incomplete worker results.'

# ---------------------------------------------------------------------------
# Phase 3: 405 enforcement
# ---------------------------------------------------------------------------

Write-Phase "3/10 - HTTP method enforcement"

$methodTests = @(
  @{ Url = "$dll`?mode=health"; Method = 'POST'; Expect = 405; Label = 'POST->health' },
  @{ Url = "$dll`?mode=health"; Method = 'PUT'; Expect = 405; Label = 'PUT->health' },
  @{ Url = "$dll`?mode=health"; Method = 'DELETE'; Expect = 405; Label = 'DELETE->health' },
  @{ Url = "$dll"; Method = 'POST'; Expect = 405; Label = 'POST->summary' },
  @{ Url = "$dll`?mode=tools"; Method = 'GET'; Expect = 405; Label = 'GET->tools' }
)

foreach ($t in $methodTests) {
  $r = Test-Endpoint -Url $t.Url -Method $t.Method
  $got = if ($r.Ok) { $r.Status } else { $r.Status }
  $pass = ($got -eq $t.Expect)
  $icon = if ($pass) { "OK" } else { "FAIL" }
  Write-Host "  [$icon] $($t.Label): $got (expected $($t.Expect))"
  $stats.Total++; if ($pass) { $stats.Pass++ } else { $stats.Fail++; $stats.Errors += $t.Label }
}

# ---------------------------------------------------------------------------
# Phase 4: Security header verification under load
# ---------------------------------------------------------------------------

Write-Phase "4/10 - Security headers present on every response"

$requiredHeaders = @(
  'X-Content-Type-Options',
  'X-Frame-Options',
  'Content-Security-Policy',
  'Cache-Control',
  'Referrer-Policy'
)

$headerResults = 1..($Concurrency) | ForEach-Object -Parallel {
  $dll = $using:dll
  $headers = $using:requiredHeaders
  try {
    $r = Invoke-WebRequest -Uri "$dll`?mode=health" -UseBasicParsing -TimeoutSec 5
    $missing = $headers | Where-Object { -not $r.Headers.ContainsKey($_) }
    [PSCustomObject]@{ Ok = ($missing.Count -eq 0); Missing = ($missing -join ',') }
  } catch {
    [PSCustomObject]@{ Ok = $false; Missing = 'request_failed' }
  }
} -ThrottleLimit $Concurrency

$headerOk = ($headerResults | Where-Object { $_.Ok }).Count
Write-Host "  Checked $($headerResults.Count) responses: $headerOk/$($headerResults.Count) have all 5 security headers"
$headerFails = $headerResults | Where-Object { -not $_.Ok }
Assert-Check ($headerResults.Count -eq $Concurrency -and $headerOk -eq $headerResults.Count) 'Response security headers missing or worker results incomplete.'
if ($headerFails) {
  $headerFails | ForEach-Object { Write-Host "  Missing: $($_.Missing)" -ForegroundColor Yellow }
}

# ---------------------------------------------------------------------------
# Phase 5: XSS payload injection in query string
# ---------------------------------------------------------------------------

Write-Phase "5/10 - XSS payloads in query parameters"

$xssResults = $xssPayloads | ForEach-Object -Parallel {
  $dll = $using:dll
  $payload = $_
  $encoded = [System.Uri]::EscapeDataString($payload)
  try {
    $r = Invoke-WebRequest -Uri "$dll`?mode=$encoded" -UseBasicParsing -SkipHttpErrorCheck -TimeoutSec 5
    $body = $r.Content
    $reflected = $body.Contains($payload)
    [PSCustomObject]@{
      Payload   = $payload.Substring(0, [Math]::Min(60, $payload.Length))
      Status    = $r.StatusCode
      Reflected = $reflected
    }
  } catch {
    $code = 0
    if ($_.Exception.Response) { $code = [int]$_.Exception.Response.StatusCode }
    [PSCustomObject]@{ Payload = $payload.Substring(0, [Math]::Min(60, $payload.Length)); Status = $code; Reflected = $false }
  }
} -ThrottleLimit $Concurrency

$reflectedCount = ($xssResults | Where-Object { $_.Reflected }).Count
Assert-Check ($xssResults.Count -eq $xssPayloads.Count -and @($xssResults | Where-Object { $_.Status -ne 400 -or $_.Reflected }).Count -eq 0) 'Unknown-mode queries were not rejected safely with HTTP 400.'
Write-Host "  Tested: $($xssResults.Count) payloads"
Write-Host "  Reflected raw (DANGER): $reflectedCount" -ForegroundColor $(if ($reflectedCount -gt 0) { 'Red' } else { 'Green' })
if ($reflectedCount -gt 0) {
  $xssResults | Where-Object { $_.Reflected } | Select-Object -First 10 | ForEach-Object {
    Write-Host "    REFLECTED: $($_.Payload)" -ForegroundColor Red
  }
}

# ---------------------------------------------------------------------------
# Phase 6: URI scheme injection via filename param
# ---------------------------------------------------------------------------

Write-Phase "6/10 - URI scheme injection in filename parameter"

$uriResults = $uriPayloads | ForEach-Object -Parallel {
  $dll = $using:dll
  $payload = $_
  $encoded = [System.Uri]::EscapeDataString($payload)
  try {
    $r = Invoke-WebRequest -Uri "$dll`?mode=tools&input=icc&filename=$encoded" -Method POST -Headers @{ 'X-ICCDEV-Request' = '1' } -UseBasicParsing -TimeoutSec 5 -Body ([byte[]](0x00, 0x00, 0x00, 0x00)) -ContentType 'application/octet-stream'
    $body = $r.Content
    [PSCustomObject]@{ Payload = $payload; Status = $r.StatusCode; HasScheme = ($body -match 'javascript:|data:|vbscript:') }
  } catch {
    [PSCustomObject]@{ Payload = $payload; Status = 0; HasScheme = $false }
  }
} -ThrottleLimit $Concurrency

$schemeLeaks = ($uriResults | Where-Object { $_.HasScheme }).Count
Assert-Check ($uriResults.Count -eq $uriPayloads.Count -and @($uriResults | Where-Object { $_.Status -ne 200 -or $_.HasScheme }).Count -eq 0) 'URI filename probes failed to complete safely.'
Write-Host "  Tested: $($uriResults.Count) URI payloads"
Write-Host "  Scheme leak in response: $schemeLeaks" -ForegroundColor $(if ($schemeLeaks -gt 0) { 'Red' } else { 'Green' })

# ---------------------------------------------------------------------------
# Phase 7: Oversized request body (DoS resistance)
# ---------------------------------------------------------------------------

Write-Phase "7/10 - Oversized request body (17 MB)"

$bigBody = [byte[]]::new(17 * 1024 * 1024)
[System.Random]::new(42).NextBytes($bigBody)
$oversizeResult = Test-Endpoint -Url "$dll`?mode=tools&input=icc&filename=huge.icc" -Method POST -Body $bigBody -ContentType 'application/octet-stream' -TimeoutSec 15
Write-Host "  Status: $($oversizeResult.Status) (expect HTTP 400 or 413)"
Assert-Check ($oversizeResult.Status -in @(400, 413)) "Oversized upload returned $($oversizeResult.Status), expected 400 or 413."
$bigBody = $null  # free memory

# ---------------------------------------------------------------------------
# Phase 8: Malformed Content-Type headers
# ---------------------------------------------------------------------------

Write-Phase "8/10 - Malformed Content-Type headers"

$badTypes = @(
  'text/html; charset=utf-8',
  'application/x-www-form-urlencoded',
  'multipart/form-data; boundary=----evil',
  '../../../etc/passwd',
  '<script->alert(1)</script>',
  'application/octet-stream; name="../../etc/passwd"',
  ''
)

foreach ($ct in $badTypes) {
  if ($ct -in @('../../../etc/passwd', '<script->alert(1)</script>')) {
    $stats.Skipped++
    Write-Host "  [SKIP] '$ct' cannot be sent through the cmdlet's Content-Type parser; server behavior was not tested."
    continue
  }
  $r = Test-Endpoint -Url "$dll`?mode=tools&input=icc&filename=test.icc" -Method POST -Body 'AAAA' -ContentType $ct
  $label = if ($ct.Length -gt 40) { $ct.Substring(0, 40) + '...' } else { $ct }
  Write-Host "  Content-Type='$label' -> $($r.Status)"
  $expected = if ($ct -eq 'application/octet-stream; name="../../etc/passwd"') { 200 } else { 415 }
  Record-Result $r "Content-Type '$label'" $expected
}

# ---------------------------------------------------------------------------
# Phase 9: Rapid reconnect churn
# ---------------------------------------------------------------------------

Write-Phase "9/10 - Rapid reconnect churn (${Concurrency}x$Duration sec)"

$churnResults = 1..$Concurrency | ForEach-Object -Parallel {
  $dll = $using:dll
  $count = 0; $ok = 0
  $sw = [System.Diagnostics.Stopwatch]::StartNew()
  while ($sw.Elapsed.TotalSeconds -lt $using:Duration) {
    try {
      $r = Invoke-WebRequest `
        -Uri "$dll`?mode=health" `
        -UseBasicParsing `
        -DisableKeepAlive `
        -TimeoutSec 5
      if ($r.StatusCode -eq 200) {
        $ok++
      }
    } catch { }
    $count++
  }
  [PSCustomObject]@{ Requests = $count; Ok = $ok }
} -ThrottleLimit $Concurrency

$churnTotal = ($churnResults | Measure-Object -Property Requests -Sum).Sum
$churnOk    = ($churnResults | Measure-Object -Property Ok -Sum).Sum
Write-Host "  Connections: $churnTotal, OK: $churnOk, Fail: $($churnTotal - $churnOk)"

# ---------------------------------------------------------------------------
# Phase 10: Path traversal in filename
# ---------------------------------------------------------------------------

Write-Phase "10/10 - Path traversal payloads in filename"

$pathPayloads = @(
  '../../../etc/passwd',
  '..\..\windows\system32\config\sam',
  '....//....//etc/passwd',
  'test%00.xml',
  '.hidden',
  'CON',
  'NUL',
  'COM1',
  'AUX',
  "test`ninjected-header: true",
  'file.icc;rm -rf /',
  'file.icc|calc.exe',
  'file.icc$(calc.exe)'
)

foreach ($fn in $pathPayloads) {
  $encoded = [System.Uri]::EscapeDataString($fn)
  $r = Test-Endpoint -Url "$dll`?mode=tools&input=icc&filename=$encoded" -Method POST -Body 'AAAA' -ContentType 'application/octet-stream'
  Write-Host "  filename='$fn' -> status $($r.Status)"
  Record-Result $r "Filename probe '$fn'" 200
  if ($r.Status -eq 200) {
    try {
      $response = $r.Content | ConvertFrom-Json -ErrorAction Stop
      $safe = [string]$response.input.filename
      Assert-Check ($safe -match '^[^/\\:<>|]+\.icc$' -and $safe -notmatch '[\x00-\x1f]' -and
        $response.input.url -match '^/_tool-work/[0-9a-f]{32}/[^/]+\.icc$') "Unsafe persisted filename or artifact URL for '$fn'."
    }
    catch {
      Assert-Check $false "Filename probe returned invalid tool JSON: $($_.Exception.Message)"
    }
  }
}

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------

Write-Host "`n=== STRESS TEST SUMMARY ===" -ForegroundColor Yellow
Write-Host "Health rapid-fire: $totalReqs requests, $totalFail failures, ~$([math]::Round($totalReqs / $Duration, 1)) RPS"
Write-Host "XSS reflected: $reflectedCount / $($xssPayloads.Count)"
Write-Host "URI scheme leak: $schemeLeaks / $($uriPayloads.Count)"
Write-Host "Reconnect churn: $churnTotal connections, $($churnTotal - $churnOk) failures"
Write-Host "Assertions: $($stats.Pass)/$($stats.Total) passed; $($stats.Skipped) client-header probes skipped"

if ($totalFail -gt 0 -or
    $healthResults.Count -ne $Concurrency -or
    $totalReqs -eq 0 -or
    $stats.Fail -gt 0 -or
    $reflectedCount -gt 0 -or
    $schemeLeaks -gt 0 -or
    $churnResults.Count -ne $Concurrency -or
    $churnTotal -eq 0 -or
    $churnOk -ne $churnTotal) {
  $stats.Errors | ForEach-Object { Write-Host "  [FAIL] $_" -ForegroundColor Red }
  Write-Host "`n*** QA CHECKS FAILED ***" -ForegroundColor Red
  exit 1
}

Write-Host "`nAll executed QA checks passed; skipped probes are not server evidence." -ForegroundColor Green
exit 0
