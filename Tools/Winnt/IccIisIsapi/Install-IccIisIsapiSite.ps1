<#
@file
File:       Install-IccIisIsapiSite.ps1

Contains:   IIS site installer and verifier for the iccIisIsapi sample.

Version:    V1

Copyright:  (c) see Software License
#>

<#
Copyright (c) International Color Consortium.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in
   the documentation and/or other materials provided with the
   distribution.

3. In the absence of prior written permission, the names "ICC" and "The
   International Color Consortium" must not be used to imply that the
   ICC organization endorses or promotes products derived from this
   software.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESSED OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE INTERNATIONAL COLOR CONSORTIUM OR
ITS CONTRIBUTING MEMBERS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
====================================================================

This software consists of voluntary contributions made by many
individuals on behalf of the International Color Consortium.

Membership in the ICC is encouraged when this software is used for
commercial purposes.

For more information on The International Color Consortium, please
see http://www.color.org/.
#>

[CmdletBinding()]
param(
  [string]$SiteName = 'iccDLL Server',
  [string]$PoolName,
  [int]$Port = 18081,
  [string]$HostHeader = '',
  [ValidateSet('Install', 'Build')]
  [string]$Layout = 'Install',
  [ValidateSet('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel')]
  [string]$Configuration = 'Release',
  [string]$SiteRoot,
  [string]$SampleIccPath
)

$ErrorActionPreference = 'Stop'
$PSDefaultParameterValues['*:ErrorAction'] = 'Stop'

if (-not $PoolName) {
  $PoolName = "$SiteName-Pool"
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $scriptRoot '..\..\..')).Path
$appcmd = Join-Path $env:SystemRoot 'System32\inetsrv\appcmd.exe'

if (-not (Test-Path $appcmd)) {
  throw "appcmd.exe was not found at $appcmd. Install IIS management tools first."
}

$currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($currentIdentity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
  throw 'This script must be run from an elevated PowerShell session.'
}

function Get-DefaultSiteRoot {
  param(
    [string]$LayoutName,
    [string]$ConfigName
  )

  if ($LayoutName -eq 'Build') {
    return Join-Path $repoRoot ("out\iis-shared-vcpkg-toolchain\Tools\IccIisIsapi\" + $ConfigName)
  }

  return Join-Path $repoRoot 'out\iis-shared-install\bin'
}

if (-not $SiteRoot) {
  $SiteRoot = Get-DefaultSiteRoot -LayoutName $Layout -ConfigName $Configuration
}
$SiteRoot = (Resolve-Path $SiteRoot).Path

$dllPath = Join-Path $SiteRoot 'iccIisIsapi.dll'
$smokePath = Join-Path $SiteRoot 'iccIisIsapiSmoke.exe'
$indexPath = Join-Path $SiteRoot 'index.html'
$webConfigPath = Join-Path $SiteRoot 'web.config'

foreach ($requiredPath in @($dllPath, $smokePath, $indexPath, $webConfigPath)) {
  if (-not (Test-Path $requiredPath)) {
    throw "Required IIS sample artifact missing: $requiredPath"
  }
}

if (-not $SampleIccPath) {
  $candidateSample = Join-Path $repoRoot 'Testing\sRGB_v4_ICC_preference.icc'
  if (Test-Path $candidateSample) {
    $SampleIccPath = $candidateSample
  }
}
if ($SampleIccPath) {
  $SampleIccPath = (Resolve-Path $SampleIccPath).Path
}

function Invoke-AppCmd {
  param(
    [string[]]$Arguments,
    [switch]$IgnoreExitCode
  )

  $output = & $appcmd @Arguments 2>&1
  $exitCode = $LASTEXITCODE
  if (-not $IgnoreExitCode -and $exitCode -ne 0) {
    $text = ($output | Out-String).Trim()
    throw "appcmd failed ($exitCode): $($Arguments -join ' ')`n$text"
  }

  return @{
    Output = $output
    ExitCode = $exitCode
  }
}

function Test-AppCmdExists {
  param(
    [string[]]$Arguments,
    [string]$MatchText
  )

  $result = Invoke-AppCmd -Arguments $Arguments -IgnoreExitCode
  if ($result.ExitCode -ne 0) {
    return $false
  }

  $text = ($result.Output | Out-String)
  return $text -match [Regex]::Escape($MatchText)
}

$bindingInfo = "*:${Port}:$HostHeader"
$baseUrlHost = if ($HostHeader) { $HostHeader } else { 'localhost' }
$baseUrl = "http://${baseUrlHost}:$Port/"

if (-not (Test-AppCmdExists -Arguments @('list', 'apppool', "/name:$PoolName") -MatchText $PoolName)) {
  Invoke-AppCmd -Arguments @('add', 'apppool', "/name:$PoolName") | Out-Null
}
Invoke-AppCmd -Arguments @('set', 'apppool', $PoolName, '/managedRuntimeVersion:', '/managedPipelineMode:Integrated') | Out-Null
Invoke-AppCmd -Arguments @('set', 'apppool', $PoolName, '/enable32BitAppOnWin64:false') | Out-Null

if (-not (Test-AppCmdExists -Arguments @('list', 'site', "/name:$SiteName") -MatchText $SiteName)) {
  Invoke-AppCmd -Arguments @('add', 'site', "/name:$SiteName", "/bindings:http/$bindingInfo", "/physicalPath:$SiteRoot") | Out-Null
}
else {
  $siteInfo = Invoke-AppCmd -Arguments @('list', 'site', "/name:$SiteName", '/text:*')
  $siteText = ($siteInfo.Output | Out-String)
  if ($siteText -notmatch [Regex]::Escape("bindingInformation:`"$bindingInfo`"") -and
      $siteText -notmatch [Regex]::Escape("bindings:`"http/$bindingInfo`"")) {
    throw "Existing IIS site '$SiteName' does not use binding $bindingInfo. Use a different site name or update the binding manually."
  }
  Invoke-AppCmd -Arguments @('set', 'vdir', "/vdir.name:$SiteName/", "/physicalPath:$SiteRoot") | Out-Null
}

Invoke-AppCmd -Arguments @('set', 'app', "$SiteName/", "/applicationPool:$PoolName") | Out-Null
Invoke-AppCmd -Arguments @('set', 'config', $SiteName, '-section:system.webServer/serverRuntime', '/appConcurrentRequestLimit:5000', '/commit:apphost') | Out-Null
Invoke-AppCmd -Arguments @('set', 'config', $SiteName, '-section:system.webServer/handlers', '/accessPolicy:Read,Script,Execute', '/commit:apphost') | Out-Null

$handlerList = Invoke-AppCmd -Arguments @('list', 'config', $SiteName, '-section:system.webServer/handlers')
$handlerText = ($handlerList.Output | Out-String)
if ($handlerText -match 'name="iccIisIsapi"' -or $handlerText -match 'name=''iccIisIsapi''') {
  Invoke-AppCmd -Arguments @('set', 'config', $SiteName, '-section:system.webServer/handlers', "/-[name='iccIisIsapi']", '/commit:apphost') | Out-Null
}
Invoke-AppCmd -Arguments @(
  'set', 'config', $SiteName, '-section:system.webServer/handlers',
  "/+[name='iccIisIsapi',path='iccIisIsapi.dll',verb='GET,HEAD,POST',modules='IsapiModule',scriptProcessor='$dllPath',resourceType='File',requireAccess='Execute',allowPathInfo='True']",
  '/commit:apphost'
) | Out-Null

$restrictionList = Invoke-AppCmd -Arguments @('list', 'config', '/section:isapiCgiRestriction')
$restrictionText = ($restrictionList.Output | Out-String)
if ($restrictionText -match [Regex]::Escape("path=`"$dllPath`"") -or
    $restrictionText -match [Regex]::Escape("path='$dllPath'")) {
  Invoke-AppCmd -Arguments @('set', 'config', '/section:isapiCgiRestriction', "/-[path='$dllPath']", '/commit:apphost') | Out-Null
}
Invoke-AppCmd -Arguments @(
  'set', 'config', '/section:isapiCgiRestriction',
  "/+[path='$dllPath',allowed='True',description='iccDEV IIS ISAPI sample']",
  '/commit:apphost'
) | Out-Null

Invoke-AppCmd -Arguments @('start', 'apppool', $PoolName) -IgnoreExitCode | Out-Null
Invoke-AppCmd -Arguments @('start', 'site', $SiteName) -IgnoreExitCode | Out-Null

$rootResponse = Invoke-WebRequest -Uri $baseUrl -UseBasicParsing -TimeoutSec 30
if ($rootResponse.StatusCode -ne 200 -or $rootResponse.Content -notmatch 'iccDEV IIS') {
  throw "Root verification failed for $baseUrl"
}

$healthResponse = Invoke-WebRequest -Uri ($baseUrl + 'iccIisIsapi.dll?mode=health') -UseBasicParsing -TimeoutSec 30
if ($healthResponse.StatusCode -ne 200 -or $healthResponse.Content.Trim() -ne 'ok') {
  throw "Health verification failed for $baseUrl"
}

$summaryResponse = Invoke-WebRequest -Uri ($baseUrl + 'iccIisIsapi.dll') -UseBasicParsing -TimeoutSec 30
if ($summaryResponse.StatusCode -ne 200 -or $summaryResponse.Content -notmatch 'IccProfLib version:') {
  throw "Summary verification failed for $baseUrl"
}

$verification = [ordered]@{
  site_name = $SiteName
  pool_name = $PoolName
  site_root = $SiteRoot
  dll_path = $dllPath
  base_url = $baseUrl
  root_ok = $true
  health_ok = $true
  summary_ok = $true
}

if ($SampleIccPath) {
  $uploadUrl = $baseUrl + 'iccIisIsapi.dll?mode=tools&input=icc&filename=sample.icc'
  $headers = @{ Accept = 'application/json' }
  $uploadResponse = Invoke-WebRequest -Uri $uploadUrl -Method Post -Headers $headers -InFile $SampleIccPath -ContentType 'application/octet-stream' -UseBasicParsing -TimeoutSec 120
  if ($uploadResponse.StatusCode -ne 200) {
    throw "Upload verification failed for $uploadUrl"
  }

  $uploadJson = $uploadResponse.Content | ConvertFrom-Json
  if ($uploadJson.mode -ne 'tools') {
    throw "Unexpected upload response mode from $uploadUrl"
  }

  $verification.upload_ok = $true
  $verification.upload_sample = $SampleIccPath
  $verification.workspace_url = $uploadJson.workspace_url
}

$verification | ConvertTo-Json -Depth 5
