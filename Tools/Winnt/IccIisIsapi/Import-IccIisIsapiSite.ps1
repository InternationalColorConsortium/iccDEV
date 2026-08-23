<#
@file
File:       Import-IccIisIsapiSite.ps1

Contains:   Deploy an iccIisIsapi package to IIS without build tools.

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
#>

<#
.SYNOPSIS
  Deploy a pre-built iccIisIsapi package to IIS.

.DESCRIPTION
  Extracts a ZIP package created by Export-IccIisIsapiSite.ps1 to a target
  directory and configures an IIS site with app pool, handler mapping, and
  ISAPI restriction. No build tools, CMake, or vcpkg required.

.PARAMETER PackageZip
  Path to the ZIP file created by Export-IccIisIsapiSite.ps1.

.PARAMETER DestinationRoot
  Directory to extract the package into. Defaults to C:\iccDLL-Server.

.PARAMETER SiteName
  IIS site name. Defaults to 'iccDLL Server'.

.PARAMETER Port
  HTTP port to bind. Defaults to 18081.

.PARAMETER BindAddress
  IP address to bind. Defaults to loopback-only 127.0.0.1.

.PARAMETER PoolName
  IIS application pool name. Defaults to '<SiteName>-Pool'.

.PARAMETER SkipIISSetup
  Extract files only -- do not configure IIS.

.EXAMPLE
  .\Import-IccIisIsapiSite.ps1 -PackageZip iccDLL-Server-package.zip
  .\Import-IccIisIsapiSite.ps1 -PackageZip pkg.zip -SiteName "Test" -Port 8080
  .\Import-IccIisIsapiSite.ps1 -PackageZip pkg.zip -SkipIISSetup
#>

[CmdletBinding()]
param(
  [Parameter(Mandatory)]
  [string]$PackageZip,

  [string]$DestinationRoot = "C:\iccDLL-Server",

  [string]$SiteName = "iccDLL Server",

  [int]$Port = 18081,

  [string]$BindAddress = "127.0.0.1",

  [string]$PoolName = "",

  [switch]$SkipIISSetup
)

$ErrorActionPreference = 'Stop'

if (-not $PoolName) { $PoolName = "$SiteName-Pool" }

# -- Validate package ---------------------------------------------------------
if (-not (Test-Path $PackageZip)) {
  throw "Package not found: $PackageZip"
}

Write-Host "=== Import-IccIisIsapiSite ===" -ForegroundColor Cyan
Write-Host "Package:     $PackageZip"
Write-Host "Destination: $DestinationRoot"
Write-Host "Site:        $SiteName ($BindAddress`:$Port)"

# -- Validate and extract -----------------------------------------------------
$stagingRoot = Join-Path ([IO.Path]::GetTempPath()) ("icc-iis-import-" + [guid]::NewGuid().ToString("N"))
try {
  New-Item -ItemType Directory -Path $stagingRoot -Force | Out-Null
  Expand-Archive -Path $PackageZip -DestinationPath $stagingRoot -Force

  $packageDllPath = Join-Path $stagingRoot "iccIisIsapi.dll"
  if (-not (Test-Path $packageDllPath -PathType Leaf)) {
    throw "CRITICAL: iccIisIsapi.dll not found in package."
  }
  foreach ($requiredWebFile in @('index.html', 'index.js', 'endpoints.html', 'error.html', 'site.js', 'sanitize.js', 'web.config')) {
    if (-not (Test-Path (Join-Path $stagingRoot $requiredWebFile) -PathType Leaf)) {
      throw "CRITICAL: Required IIS web artifact missing from package: $requiredWebFile"
    }
  }

  $requiredExes = @(
    "iccToXml.exe",
    "iccFromXml.exe",
    "iccDumpProfile.exe",
    "iccPawgReport.exe",
    "iccRoundTrip.exe"
  )
  foreach ($exe in $requiredExes) {
    if (-not (Test-Path (Join-Path $stagingRoot $exe) -PathType Leaf)) {
      throw "CRITICAL: Required tool executable missing from package: $exe"
    }
  }

  if (Test-Path $DestinationRoot) {
    Write-Host "Destination exists -- updating files in place" -ForegroundColor Yellow
  } else {
    New-Item -ItemType Directory -Path $DestinationRoot -Force | Out-Null
  }

  Get-ChildItem $stagingRoot -Force | ForEach-Object {
    Copy-Item $_.FullName $DestinationRoot -Recurse -Force
  }
  $fileCount = (Get-ChildItem $stagingRoot -Recurse -File).Count
  Write-Host "Extracted $fileCount package files to $DestinationRoot"
} finally {
  if (Test-Path $stagingRoot) {
    Remove-Item $stagingRoot -Recurse -Force
  }
}
$dllPath = Join-Path $DestinationRoot "iccIisIsapi.dll"

# -- Read manifest ------------------------------------------------------------
$manifestPath = Join-Path $DestinationRoot "manifest.json"
if (Test-Path $manifestPath) {
  $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
  Write-Host "Package built on $($manifest.hostname) at $($manifest.exported)"
}

if ($SkipIISSetup) {
  Write-Host "`nFiles extracted. IIS setup skipped (-SkipIISSetup)." -ForegroundColor Yellow
  Write-Host "Configure IIS manually or run Install-IccIisIsapiSite.ps1."
  exit 0
}

# -- Require admin -------------------------------------------------------------
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
  [Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
  throw "IIS configuration requires administrator privileges. Re-run as admin."
}

# -- Configure IIS -------------------------------------------------------------
$appcmd = "$env:SystemRoot\system32\inetsrv\appcmd.exe"
if (-not (Test-Path $appcmd)) {
  throw "appcmd.exe not found. Is IIS installed?"
}

Write-Host "`nConfiguring IIS..." -ForegroundColor Cyan

# App pool
$poolExists = & $appcmd list apppool /name:"$PoolName" 2>$null
if (-not $poolExists) {
  & $appcmd add apppool /name:"$PoolName" /managedRuntimeVersion:"" /enable32BitAppOnWin64:false
  Write-Host "  Created app pool: $PoolName"
} else {
  Write-Host "  App pool exists: $PoolName"
}
& $appcmd set apppool /apppool.name:"$PoolName" /processModel.identityType:ApplicationPoolIdentity 2>$null

# Site
$siteExists = & $appcmd list site /name:"$SiteName" 2>$null
if (-not $siteExists) {
  & $appcmd add site /name:"$SiteName" /physicalPath:"$DestinationRoot" `
    /bindings:"http/${BindAddress}:${Port}:" 2>$null
  Write-Host "  Created site: $SiteName on port $Port"
} else {
  & $appcmd set site /site.name:"$SiteName" /physicalPath:"$DestinationRoot" 2>$null
  Write-Host "  Updated site root: $DestinationRoot"
}
& $appcmd set site /site.name:"$SiteName" /[path='/'].applicationPool:"$PoolName" 2>$null
& $appcmd set config "$SiteName" -section:system.webServer/serverRuntime `
  /appConcurrentRequestLimit:64 /commit:apphost 2>$null

# Handler mapping
& $appcmd set config "$SiteName" /section:handlers `
  /+"[name='iccIisIsapi',path='iccIisIsapi.dll',verb='*',modules='IsapiModule',scriptProcessor='$dllPath',resourceType='File',requireAccess='Execute',allowPathInfo='True']" `
  /commit:apphost 2>$null
Write-Host "  Handler mapped: iccIisIsapi.dll"

# ISAPI restriction
& $appcmd set config /section:isapiCgiRestriction `
  /+"[path='$dllPath',allowed='true',description='iccDEV ISAPI']" `
  /commit:apphost 2>$null
Write-Host "  ISAPI restriction: allowed"

# Start
& $appcmd start apppool /apppool.name:"$PoolName" 2>$null
& $appcmd start site /site.name:"$SiteName" 2>$null
Write-Host "  Site started" -ForegroundColor Green

# -- Verify -------------------------------------------------------------------
Write-Host "`nVerifying..." -ForegroundColor Cyan
Start-Sleep -Seconds 2

$endpoints = @(
  @{ Path = "/iccIisIsapi.dll?mode=health"; Expect = 200; Label = "Health" },
  @{ Path = "/";                             Expect = 200; Label = "Landing page" },
  @{ Path = "/endpoints.html";               Expect = 200; Label = "Tool console" }
)

$allOk = $true
foreach ($ep in $endpoints) {
  try {
    $r = Invoke-WebRequest -Uri "http://${BindAddress}:$Port$($ep.Path)" -UseBasicParsing -TimeoutSec 10
    $mark = if ($r.StatusCode -eq $ep.Expect) { "PASS" } else { "FAIL"; $allOk = $false }
    Write-Host "  [$mark] $($ep.Label): $($r.StatusCode)"
  } catch {
    Write-Host "  [FAIL] $($ep.Label): $($_.Exception.Message)" -ForegroundColor Red
    $allOk = $false
  }
}

if ($allOk) {
  Write-Host "`nDeployment successful!" -ForegroundColor Green
  Write-Host "  Site URL:      http://${BindAddress}:$Port/"
  Write-Host "  Tool console:  http://${BindAddress}:$Port/endpoints.html"
  Write-Host "  Health check:  http://${BindAddress}:$Port/iccIisIsapi.dll?mode=health"
} else {
  Write-Host "`nDeployment completed with warnings. Check IIS event logs." -ForegroundColor Yellow
}
