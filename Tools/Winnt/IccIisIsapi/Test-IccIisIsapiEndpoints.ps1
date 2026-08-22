<#
@file
File:       Test-IccIisIsapiEndpoints.ps1

Contains:   Live HTTP QA for the iccIisIsapi IIS sample.

Copyright:  (c) see Software License
#>

[CmdletBinding()]
param(
  [string]$BaseUrl = 'http://localhost:18081',
  [Parameter(Mandatory = $true)]
  [string]$SampleIccPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$PSDefaultParameterValues['*:ErrorAction'] = 'Stop'
$ProgressPreference = 'SilentlyContinue'
$BaseUrl = $BaseUrl.TrimEnd('/')
$SampleIccPath = (Resolve-Path $SampleIccPath).Path

function Assert-True {
  param(
    [bool]$Condition,
    [string]$Message
  )

  if (-not $Condition) {
    throw $Message
  }
}

function Invoke-QARequest {
  param(
    [string]$Path,
    [string]$Method = 'Get',
    [string]$InFile,
    [string]$ContentType
  )

  $args = @{
    Uri = $BaseUrl + $Path
    Method = $Method
    UseBasicParsing = $true
    SkipHttpErrorCheck = $true
    TimeoutSec = 120
  }
  if ($InFile) {
    $args.InFile = $InFile
  }
  if ($ContentType) {
    $args.ContentType = $ContentType
  }
  if ($Method -eq 'Post') {
    $args.Headers = @{
      'X-ICCDEV-Request' = '1'
      Accept = 'application/json'
    }
  }

  Invoke-WebRequest @args
}

$results = [ordered]@{}

$root = Invoke-QARequest -Path '/'
Assert-True ($root.StatusCode -eq 200) 'Landing page did not return HTTP 200.'
Assert-True ([string]$root.Content -match 'iccDEV IIS') 'Landing page content is unexpected.'
Assert-True ([string]$root.Headers['X-Frame-Options'] -match 'DENY') 'Landing page omitted clickjacking protection.'
Assert-True ([string]$root.Headers['X-Content-Type-Options'] -match 'nosniff') 'Landing page omitted MIME sniffing protection.'
$contentSecurityPolicy = [string]$root.Headers['Content-Security-Policy']
Assert-True ($contentSecurityPolicy -match "frame-ancestors 'none'") 'Landing page omitted frame-ancestors protection.'
Assert-True ($contentSecurityPolicy -notmatch "'unsafe-inline'") 'Landing page CSP permits arbitrary inline style execution.'
Assert-True ($contentSecurityPolicy -match "style-src-attr 'none'") 'Landing page CSP does not block style attributes.'
Assert-True (([regex]::Matches($contentSecurityPolicy, "'sha256-[A-Za-z0-9+/=]+'")).Count -eq 3) 'Landing page CSP omitted a trusted inline stylesheet hash.'
$results.landing_page = 'ok'

$console = Invoke-QARequest -Path '/endpoints.html'
Assert-True ($console.StatusCode -eq 200) 'Endpoint console did not return HTTP 200.'
Assert-True ([string]$console.Content -match 'Analyze An ICC Profile') 'Endpoint console does not expose the profile analysis UI.'
$results.endpoint_console = 'ok'

$health = Invoke-QARequest -Path '/iccIisIsapi.dll?mode=health'
Assert-True ($health.StatusCode -eq 200) 'Health endpoint did not return HTTP 200.'
Assert-True ($health.Content.Trim() -eq 'ok') 'Health endpoint did not return ok.'
Assert-True (@($health.Headers['Content-Security-Policy']).Count -eq 1) 'Health endpoint returned duplicate CSP headers.'
Assert-True (@($health.Headers['Cache-Control']).Count -eq 1) 'Health endpoint returned duplicate Cache-Control headers.'
$results.health = 'ok'

$summary = Invoke-QARequest -Path '/iccIisIsapi.dll'
Assert-True ($summary.StatusCode -eq 200) 'Summary endpoint did not return HTTP 200.'
Assert-True ([string]$summary.Content -match 'iccDEV IIS service') 'Summary endpoint content is unexpected.'
Assert-True ([string]$summary.Content -notmatch '\+\b[0-9a-f]{7,40}\b|Git Commit|version:') 'Summary endpoint disclosed software version or commit metadata.'
$results.summary = 'ok'

$unknownMode = Invoke-QARequest -Path '/iccIisIsapi.dll?mode=admin'
Assert-True ($unknownMode.StatusCode -eq 400) 'Unknown mode did not return HTTP 400.'
$unknownFormat = Invoke-QARequest -Path '/iccIisIsapi.dll?format=csv'
Assert-True ($unknownFormat.StatusCode -eq 400) 'Unknown format did not return HTTP 400.'
$encodedNull = Invoke-QARequest -Path '/iccIisIsapi.dll?format=xml%00json'
Assert-True ($encodedNull.StatusCode -eq 400) 'Encoded null query value was not rejected.'
$results.query_allowlist = 'ok'

$xml = Invoke-QARequest -Path '/iccIisIsapi.dll?format=xml'
Assert-True ($xml.StatusCode -eq 200) 'XML endpoint did not return HTTP 200.'
Assert-True ([string]$xml.Headers['Content-Type'] -match '^application/xml') 'XML endpoint returned the wrong content type.'
Assert-True ([string]$xml.Content -match '^<\?xml') 'XML endpoint did not return an XML declaration.'
$results.xml = 'ok'

$unsupportedMethods = @('Put', 'Delete', 'Options', 'Trace')
foreach ($method in $unsupportedMethods) {
  $unsupportedMethod = Invoke-WebRequest `
    -Uri ($BaseUrl + '/iccIisIsapi.dll?mode=health') `
    -Method $method `
    -UseBasicParsing `
    -SkipHttpErrorCheck
  Assert-True ($unsupportedMethod.StatusCode -eq 405) "Unsupported method $method did not return HTTP 405."
  Assert-True ([string]$unsupportedMethod.Headers['Server'] -notmatch 'Microsoft-HTTPAPI') "Unsupported method $method leaked the HTTP.sys server product."
}
$results.unsupported_method = 'ok'

$invalid = Invoke-QARequest `
  -Path '/iccIisIsapi.dll?mode=tools&input=invalid' `
  -Method Post `
  -InFile $SampleIccPath `
  -ContentType 'application/octet-stream'
Assert-True ($invalid.StatusCode -eq 400) 'Invalid input kind was not rejected with HTTP 400.'
Assert-True ([string]$invalid.Content -notmatch "'icc'|'xml'|input query parameter") 'Invalid input response disclosed accepted parameter values.'
$results.invalid_input_rejected = 'ok'

$missingHeader = Invoke-WebRequest `
  -Uri ($BaseUrl + '/iccIisIsapi.dll?mode=tools&input=icc') `
  -Method Post `
  -InFile $SampleIccPath `
  -ContentType 'application/octet-stream' `
  -UseBasicParsing `
  -SkipHttpErrorCheck
Assert-True ($missingHeader.StatusCode -eq 403) 'Tool request without X-ICCDEV-Request was not rejected.'
Assert-True ([string]$missingHeader.Content -notmatch 'X-ICCDEV-Request') 'Missing-header response disclosed the request marker name.'
$results.request_header_required = 'ok'

$wrongType = Invoke-WebRequest `
  -Uri ($BaseUrl + '/iccIisIsapi.dll?mode=tools&input=icc') `
  -Method Post `
  -Headers @{ 'X-ICCDEV-Request' = '1' } `
  -InFile $SampleIccPath `
  -ContentType 'text/plain' `
  -UseBasicParsing `
  -SkipHttpErrorCheck
Assert-True ($wrongType.StatusCode -eq 415) 'Unexpected ICC content type was not rejected.'
Assert-True ([string]$wrongType.Content -notmatch 'application/octet-stream|application/xml|text/xml') 'Media-type error disclosed accepted content types.'
$results.content_type_enforced = 'ok'

$crossSite = Invoke-WebRequest `
  -Uri ($BaseUrl + '/iccIisIsapi.dll?mode=tools&input=icc') `
  -Method Post `
  -Headers @{
    'X-ICCDEV-Request' = '1'
    'Sec-Fetch-Site' = 'cross-site'
  } `
  -InFile $SampleIccPath `
  -ContentType 'application/octet-stream' `
  -UseBasicParsing `
  -SkipHttpErrorCheck
Assert-True ($crossSite.StatusCode -eq 403) 'Cross-site tool request was not rejected.'
$results.cross_site_rejected = 'ok'

$uploadPath = '/iccIisIsapi.dll?mode=tools&input=icc&filename=qa-proof.html'
$upload = Invoke-QARequest `
  -Path $uploadPath `
  -Method Post `
  -InFile $SampleIccPath `
  -ContentType 'application/octet-stream'
Assert-True ($upload.StatusCode -eq 200) 'ICC upload did not return HTTP 200.'

$payload = $upload.Content | ConvertFrom-Json
Assert-True ($payload.mode -eq 'tools') 'ICC upload response mode is not tools.'
Assert-True ($payload.input.filename -eq 'qa-proof.icc') 'ICC upload did not force a safe .icc extension.'
Assert-True ([string]$upload.Content -notmatch '[A-Za-z]:\\\\') 'ICC upload response disclosed an absolute Windows path.'
Assert-True ([string]$upload.Content -notmatch '"command"\s*:') 'ICC upload response disclosed child-process command lines.'
$workspaceName = ([string]$payload.workspace_url).Trim('/').Split('/')[-1]
Assert-True ($workspaceName -match '^[0-9a-f]{32}$') 'Workspace URL did not use a 128-bit lowercase capability identifier.'
$uploadedInputPath = ([string]$payload.input.url) -replace '^\./', '/'
$uploadedInput = Invoke-QARequest -Path $uploadedInputPath
Assert-True ($uploadedInput.StatusCode -eq 200) 'Persisted ICC input did not return HTTP 200.'
Assert-True ([string]$uploadedInput.Headers['Content-Type'] -match '^application/octet-stream') 'Persisted ICC input was not served as binary content.'
$pawg = $payload.tools | Where-Object { $_.name -eq 'iccPawgReport' } | Select-Object -First 1
Assert-True ($null -ne $pawg) 'ICC upload response omitted iccPawgReport.'
Assert-True ($pawg.ok -eq $true) 'iccPawgReport did not complete.'
Assert-True ([string]::IsNullOrWhiteSpace($pawg.artifact_url) -eq $false) 'iccPawgReport did not publish a report artifact.'

$reportPath = ([string]$pawg.artifact_url) -replace '^\./', '/'
$report = Invoke-QARequest -Path $reportPath
Assert-True ($report.StatusCode -eq 200) 'PAWG report artifact did not return HTTP 200.'
Assert-True ([string]$report.Content -match 'ICC PROFILE ASSESSMENT REPORT') 'Downloaded PAWG report content is unexpected.'
$results.pawg_report = 'ok'

$workspacePath = ([string]$payload.workspace_url) -replace '^\./', '/'
$workspace = Invoke-QARequest -Path $workspacePath
Assert-True ($workspace.StatusCode -eq 200) 'Workspace page did not return HTTP 200.'
Assert-True ([string]$workspace.Content -match 'iccPawgReport') 'Workspace page omitted the PAWG report result.'
Assert-True ([string]$workspace.Content -notmatch '/_tool-work/[^/]+/_tool-work/') 'Workspace page contains duplicated artifact paths.'
Assert-True ([string]$workspace.Headers['Content-Security-Policy'] -notmatch "'unsafe-inline'") 'Workspace CSP permits arbitrary inline style execution.'
$results.workspace = 'ok'

$workspaceRoot = Invoke-WebRequest `
  -Uri ($BaseUrl + '/_tool-work/') `
  -UseBasicParsing `
  -SkipHttpErrorCheck
Assert-True ($workspaceRoot.StatusCode -in @(403, 404)) 'Workspace root disclosed a navigation or directory page.'
Assert-True ([string]$workspaceRoot.Content -match 'Request unavailable') 'Workspace root did not return the generic error page.'
Assert-True ([string]$workspaceRoot.Content -notmatch 'Detailed Error|Physical Path|[A-Za-z]:\\') 'Workspace root disclosed IIS diagnostics or a physical path.'
$results.workspace_root_denied = 'ok'

$missingResource = Invoke-WebRequest `
  -Uri ($BaseUrl + '/missing-resource-for-qa') `
  -UseBasicParsing `
  -SkipHttpErrorCheck
Assert-True ($missingResource.StatusCode -eq 404) 'Missing resource did not return HTTP 404.'
Assert-True ([string]$missingResource.Content -match 'Request unavailable') 'Missing resource did not return the generic error page.'
Assert-True ([string]$missingResource.Content -notmatch 'Detailed Error|Physical Path|[A-Za-z]:\\') 'Missing resource disclosed IIS diagnostics or a physical path.'
$results.generic_errors = 'ok'

$reflectionToken = 'ps2q0llv51'
$reflectionProbe = Invoke-WebRequest `
  -Uri ($BaseUrl + '/' + $reflectionToken + '%5c%5cl7rk87flyy/') `
  -UseBasicParsing `
  -SkipHttpErrorCheck
Assert-True ($reflectionProbe.StatusCode -in @(400, 404)) 'Encoded-backslash reflection probe returned an unexpected status.'
Assert-True ([string]$reflectionProbe.Content -match 'Request unavailable') 'Encoded-backslash reflection probe did not return the generic error page.'
Assert-True ([string]$reflectionProbe.Content -notmatch [regex]::Escape($reflectionToken)) 'Generic error page reflected the attacker-controlled path.'
Assert-True ([string]$reflectionProbe.Content -notmatch '%5c|\\\\l7rk87flyy') 'Generic error page reflected the encoded or decoded path.'
$results.error_path_not_reflected = 'ok'

foreach ($operationalPath in @('/integration.html', '/index2.html', '/Readme.md')) {
  $operationalResponse = Invoke-WebRequest `
    -Uri ($BaseUrl + $operationalPath) `
    -UseBasicParsing `
    -SkipHttpErrorCheck
  Assert-True ($operationalResponse.StatusCode -in @(403, 404)) "Operational artifact '$operationalPath' remained publicly accessible."
  Assert-True ([string]$operationalResponse.Content -match 'Request unavailable') "Operational artifact '$operationalPath' did not use the generic error page."
}
$results.operational_docs_hidden = 'ok'

$toXml = $payload.tools |
  Where-Object { $_.name -eq 'iccToXml' } |
  Select-Object -First 1
Assert-True ($null -ne $toXml) 'ICC upload response omitted iccToXml.'
Assert-True ([string]::IsNullOrWhiteSpace($toXml.artifact_url) -eq $false) 'iccToXml did not publish an XML artifact.'

$xmlInputPath = Join-Path ([System.IO.Path]::GetTempPath()) (
  'icc-isapi-qa-' + [guid]::NewGuid().ToString('N') + '.xml'
)
try {
  $xmlArtifactPath = ([string]$toXml.artifact_url) -replace '^\./', '/'
  $xmlArtifact = Invoke-QARequest -Path $xmlArtifactPath
  Assert-True ($xmlArtifact.StatusCode -eq 200) 'Generated XML artifact did not return HTTP 200.'
  [System.IO.File]::WriteAllText(
    $xmlInputPath,
    [string]$xmlArtifact.Content,
    [System.Text.UTF8Encoding]::new($false)
  )

  $xmlUpload = Invoke-QARequest `
    -Path '/iccIisIsapi.dll?mode=tools&input=xml&filename=qa-profile.xml' `
    -Method Post `
    -InFile $xmlInputPath `
    -ContentType 'application/xml'
  Assert-True ($xmlUpload.StatusCode -eq 200) 'XML upload did not return HTTP 200.'

  $xmlPayload = $xmlUpload.Content | ConvertFrom-Json
  Assert-True ($xmlPayload.input.kind -eq 'xml') 'XML upload response input kind is not xml.'
  $xmlPawg = $xmlPayload.tools |
    Where-Object { $_.name -eq 'iccPawgReport' } |
    Select-Object -First 1
  Assert-True ($null -ne $xmlPawg) 'XML upload response omitted iccPawgReport.'
  Assert-True ($xmlPawg.ok -eq $true) 'XML-derived PAWG assessment did not complete.'
  Assert-True ([string]::IsNullOrWhiteSpace($xmlPawg.artifact_url) -eq $false) 'XML-derived PAWG assessment did not publish a report.'

  $xmlReportPath = ([string]$xmlPawg.artifact_url) -replace '^\./', '/'
  $xmlReport = Invoke-QARequest -Path $xmlReportPath
  Assert-True ($xmlReport.StatusCode -eq 200) 'XML-derived PAWG report did not return HTTP 200.'
  Assert-True ([string]$xmlReport.Content -match 'ICC PROFILE ASSESSMENT REPORT') 'XML-derived PAWG report content is unexpected.'
  $results.xml_pawg_report = 'ok'
}
finally {
  Remove-Item -LiteralPath $xmlInputPath -Force -ErrorAction SilentlyContinue
}

$doctypePath = Join-Path ([System.IO.Path]::GetTempPath()) (
  'icc-isapi-doctype-' + [guid]::NewGuid().ToString('N') + '.xml'
)
try {
  [System.IO.File]::WriteAllText(
    $doctypePath,
    '<!DOCTYPE IccProfile [<!ENTITY xxe SYSTEM "file:///c:/windows/win.ini">]><IccProfile>&xxe;</IccProfile>',
    [System.Text.Encoding]::ASCII
  )
  $doctype = Invoke-QARequest `
    -Path '/iccIisIsapi.dll?mode=tools&input=xml&filename=doctype.xml' `
    -Method Post `
    -InFile $doctypePath `
    -ContentType 'application/xml'
  Assert-True ($doctype.StatusCode -eq 400) 'XML DOCTYPE upload was not rejected.'
  Assert-True ([string]$doctype.Content -notmatch 'DOCTYPE|XXE') 'XML rejection disclosed the mitigation strategy.'
  $results.xml_doctype_rejected = 'ok'
}
finally {
  Remove-Item -LiteralPath $doctypePath -Force -ErrorAction SilentlyContinue
}

[pscustomobject]@{
  base_url = $BaseUrl
  sample = $SampleIccPath
  checks = $results
  workspace_url = $payload.workspace_url
  report_url = $pawg.artifact_url
  xml_report_url = $xmlPawg.artifact_url
} | ConvertTo-Json -Depth 5
