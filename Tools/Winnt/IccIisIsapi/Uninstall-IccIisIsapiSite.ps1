<#
@file
File:       Uninstall-IccIisIsapiSite.ps1

Contains:   IIS site uninstaller for the iccIisIsapi sample.

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
  [switch]$RemoveFiles
)

$ErrorActionPreference = 'Stop'
$PSDefaultParameterValues['*:ErrorAction'] = 'Stop'

if (-not $PoolName) {
  $PoolName = "$SiteName-Pool"
}

$appcmd = Join-Path $env:SystemRoot 'System32\inetsrv\appcmd.exe'

if (-not (Test-Path $appcmd)) {
  throw "appcmd.exe was not found at $appcmd. Install IIS management tools first."
}

$currentIdentity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($currentIdentity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
  throw 'This script must be run from an elevated PowerShell session.'
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

$removed = [ordered]@{
  site_name = $SiteName
  pool_name = $PoolName
  site_removed = $false
  pool_removed = $false
  isapi_restriction_removed = $false
  files_removed = $false
}

# Resolve site root before deletion (for optional -RemoveFiles)
$siteRoot = $null
if (Test-AppCmdExists -Arguments @('list', 'site', "/name:$SiteName") -MatchText $SiteName) {
  try {
    $vdirInfo = Invoke-AppCmd -Arguments @('list', 'vdir', "/vdir.name:$SiteName/", '/text:physicalPath') -IgnoreExitCode
    $siteRoot = ($vdirInfo.Output | Out-String).Trim()
  }
  catch { }

  $dllPath = if ($siteRoot) { Join-Path $siteRoot 'iccIisIsapi.dll' } else { $null }
}

# 1. Remove ISAPI CGI restriction (global scope)
if ($dllPath) {
  $restrictionList = Invoke-AppCmd -Arguments @('list', 'config', '/section:isapiCgiRestriction') -IgnoreExitCode
  $restrictionText = ($restrictionList.Output | Out-String)
  if ($restrictionText -match [Regex]::Escape("path=`"$dllPath`"") -or
      $restrictionText -match [Regex]::Escape("path='$dllPath'")) {
    Invoke-AppCmd -Arguments @('set', 'config', '/section:isapiCgiRestriction', "/-[path='$dllPath']", '/commit:apphost') -IgnoreExitCode | Out-Null
    $removed.isapi_restriction_removed = $true
  }
}

# 2. Stop and delete the site
if (Test-AppCmdExists -Arguments @('list', 'site', "/name:$SiteName") -MatchText $SiteName) {
  Invoke-AppCmd -Arguments @('stop', 'site', $SiteName) -IgnoreExitCode | Out-Null
  Invoke-AppCmd -Arguments @('delete', 'site', $SiteName) | Out-Null
  $removed.site_removed = $true
}

# 3. Stop and delete the app pool
if (Test-AppCmdExists -Arguments @('list', 'apppool', "/name:$PoolName") -MatchText $PoolName) {
  Invoke-AppCmd -Arguments @('stop', 'apppool', $PoolName) -IgnoreExitCode | Out-Null
  Invoke-AppCmd -Arguments @('delete', 'apppool', $PoolName) | Out-Null
  $removed.pool_removed = $true
}

# 4. Optionally remove site files
if ($RemoveFiles -and $siteRoot -and (Test-Path $siteRoot)) {
  Remove-Item -Path $siteRoot -Recurse -Force
  $removed.files_removed = $true
  $removed.files_path = $siteRoot
}

$removed | ConvertTo-Json -Depth 5
