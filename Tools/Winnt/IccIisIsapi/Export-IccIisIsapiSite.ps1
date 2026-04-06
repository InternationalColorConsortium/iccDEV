<#
@file
File:       Export-IccIisIsapiSite.ps1

Contains:   Package an iccIisIsapi site deployment into a portable ZIP.

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
  Package an iccIisIsapi site deployment into a portable ZIP archive.

.DESCRIPTION
  Collects all DLLs, EXEs, HTML/JS/CSS, assets, and configuration files from
  a build output or install directory into a self-contained ZIP that can be
  deployed on any Windows machine with IIS installed — no build tools required.

  Use Import-IccIisIsapiSite.ps1 to deploy the resulting package.

.PARAMETER SourceRoot
  Path to the built site root directory containing iccIisIsapi.dll and tool EXEs.
  Defaults to the most recent build output if found.

.PARAMETER OutputZip
  Path for the output ZIP file. Defaults to iccDLL-Server-package.zip in the
  current directory.

.PARAMETER IncludeToolWork
  If set, includes existing _tool-work/ contents in the package.
  Default: $false (empty _tool-work/index.html only).

.EXAMPLE
  .\Export-IccIisIsapiSite.ps1 -SourceRoot out\debug\Tools\IccIisIsapi\Debug
  .\Export-IccIisIsapiSite.ps1 -SourceRoot out\iis-shared-install\bin -OutputZip release.zip
#>

[CmdletBinding()]
param(
  [Parameter(Mandatory)]
  [string]$SourceRoot,

  [string]$OutputZip = "iccDLL-Server-package.zip",

  [switch]$IncludeToolWork
)

$ErrorActionPreference = 'Stop'

# ── Validate source ──────────────────────────────────────────────────────────
$SourceRoot = (Resolve-Path $SourceRoot).Path
$dll = Join-Path $SourceRoot "iccIisIsapi.dll"
if (-not (Test-Path $dll)) {
  throw "iccIisIsapi.dll not found in '$SourceRoot'. Build first or specify the correct -SourceRoot."
}

Write-Host "=== Export-IccIisIsapiSite ===" -ForegroundColor Cyan
Write-Host "Source: $SourceRoot"

# ── Collect files ────────────────────────────────────────────────────────────
$staging = Join-Path ([System.IO.Path]::GetTempPath()) "iccDLL-export-$([guid]::NewGuid().ToString('N').Substring(0,8))"
New-Item -ItemType Directory -Path $staging -Force | Out-Null

# DLLs
$dllPatterns = @("*.dll")
foreach ($p in $dllPatterns) {
  Get-ChildItem $SourceRoot -Filter $p -File | ForEach-Object {
    Copy-Item $_.FullName $staging -Force
    Write-Host "  DLL: $($_.Name)"
  }
}

# EXEs (tool executables)
Get-ChildItem $SourceRoot -Filter "*.exe" -File | ForEach-Object {
  Copy-Item $_.FullName $staging -Force
  Write-Host "  EXE: $($_.Name)"
}

# Web files
$webFiles = @("*.html", "*.css", "*.js", "*.config", "*.yaml", "*.md")
foreach ($p in $webFiles) {
  Get-ChildItem $SourceRoot -Filter $p -File -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item $_.FullName $staging -Force
    Write-Host "  Web: $($_.Name)"
  }
}

# Assets directory
$assetsDir = Join-Path $SourceRoot "assets"
if (Test-Path $assetsDir) {
  $destAssets = Join-Path $staging "assets"
  Copy-Item $assetsDir $destAssets -Recurse -Force
  $assetCount = (Get-ChildItem $destAssets -Recurse -File).Count
  Write-Host "  Assets: $assetCount files"
}

# _tool-work/index.html (always include the landing page)
$twSrc = Join-Path $SourceRoot "_tool-work"
$twDest = Join-Path $staging "_tool-work"
New-Item -ItemType Directory -Path $twDest -Force | Out-Null
$twIndex = Join-Path $twSrc "index.html"
if (Test-Path $twIndex) {
  Copy-Item $twIndex (Join-Path $twDest "index.html") -Force
  Write-Host "  _tool-work/index.html"
}

if ($IncludeToolWork -and (Test-Path $twSrc)) {
  Copy-Item "$twSrc\*" $twDest -Recurse -Force -ErrorAction SilentlyContinue
  Write-Host "  _tool-work/ contents included"
}

# PowerShell lifecycle scripts (from source tree)
$scriptDir = $PSScriptRoot
foreach ($ps1 in @("Install-IccIisIsapiSite.ps1", "Uninstall-IccIisIsapiSite.ps1",
                    "Import-IccIisIsapiSite.ps1", "Stress-IccIisIsapi.ps1")) {
  $ps1Path = Join-Path $scriptDir $ps1
  if (Test-Path $ps1Path) {
    Copy-Item $ps1Path $staging -Force
    Write-Host "  Script: $ps1"
  }
}

# ── Create manifest ──────────────────────────────────────────────────────────
$manifest = @{
  exported   = (Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ")
  source     = $SourceRoot
  hostname   = $env:COMPUTERNAME
  dll_size   = (Get-Item $dll).Length
  file_count = (Get-ChildItem $staging -Recurse -File).Count
}
$manifest | ConvertTo-Json -Depth 2 | Set-Content (Join-Path $staging "manifest.json") -Encoding UTF8
Write-Host "  Manifest: manifest.json"

# ── Compress ─────────────────────────────────────────────────────────────────
if (Test-Path $OutputZip) { Remove-Item $OutputZip -Force }
Compress-Archive -Path "$staging\*" -DestinationPath $OutputZip -CompressionLevel Optimal
$zipSize = (Get-Item $OutputZip).Length
$zipMB = [Math]::Round($zipSize / 1MB, 1)

Write-Host ""
Write-Host "Package created: $OutputZip ($zipMB MB, $($manifest.file_count) files)" -ForegroundColor Green

# ── Cleanup staging ──────────────────────────────────────────────────────────
Remove-Item $staging -Recurse -Force -ErrorAction SilentlyContinue

Write-Host @"

Deploy on target machine:
  .\Import-IccIisIsapiSite.ps1 -PackageZip $OutputZip -SiteName "iccDLL Server" -Port 18081
"@
