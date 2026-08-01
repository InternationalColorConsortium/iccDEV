# Copyright (c) 2026 The International Color Consortium. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause
<#
.SYNOPSIS
Create a dated iccDEV work session directory.

.DESCRIPTION
Creates directories named DD-mmm-YYYY-NNN under a workspace root. Dot-source
this file to load the icc-session helper into the current PowerShell session, or
run the script directly to create a directory and print its path.

.EXAMPLE
. .\.github\scripts\icc-session.ps1
icc-session

.EXAMPLE
.\.github\scripts\icc-session.ps1 -Suffix docker
#>

param(
  [string]$Root,
  [string]$Suffix = "",
  [datetime]$Date = (Get-Date),
  [switch]$NoLocation
)

function Get-IccSessionDefaultRoot {
  if (-not [string]::IsNullOrWhiteSpace($env:ICC_SESSION_ROOT)) {
    return $env:ICC_SESSION_ROOT
  }

  if (-not [string]::IsNullOrWhiteSpace($env:COPILOT_WORKSPACE)) {
    return $env:COPILOT_WORKSPACE
  }

  $sessionNamePattern = '^\d{2}-[a-z]{3}-\d{4}-\d{3}(?:-.+)?$'
  $directory = [System.IO.DirectoryInfo](Get-Location).ProviderPath
  while ($null -ne $directory) {
    if ($directory.Name -match $sessionNamePattern -and $null -ne $directory.Parent) {
      return $directory.Parent.FullName
    }
    $directory = $directory.Parent
  }

  return (Get-Location).ProviderPath
}

function New-IccSession {
  [CmdletBinding()]
  param(
    [string]$Root,
    [string]$Suffix = "",
    [datetime]$Date = (Get-Date),
    [switch]$NoLocation
  )

  if ([string]::IsNullOrWhiteSpace($Root)) {
    $Root = Get-IccSessionDefaultRoot
  }

  if (-not [string]::IsNullOrWhiteSpace($Suffix) -and $Suffix -notmatch '^[A-Za-z0-9][A-Za-z0-9._-]*$') {
    throw "Suffix must contain only letters, digits, dot, underscore, or hyphen, and must start with a letter or digit."
  }

  $resolvedRoot = [System.IO.Path]::GetFullPath($Root)
  New-Item -ItemType Directory -Path $resolvedRoot -Force | Out-Null

  $datePart = $Date.ToString("dd-MMM-yyyy", [System.Globalization.CultureInfo]::InvariantCulture).ToLowerInvariant()
  $pattern = "^{0}-(\d{{3}})(?:-.+)?$" -f [regex]::Escape($datePart)
  $maxNumber = 0

  Get-ChildItem -LiteralPath $resolvedRoot -Directory -ErrorAction Stop | ForEach-Object {
    $match = [regex]::Match($_.Name, $pattern)
    if ($match.Success) {
      $number = [int]$match.Groups[1].Value
      if ($number -gt $maxNumber) {
        $maxNumber = $number
      }
    }
  }

  $nextNumber = $maxNumber + 1
  while ($true) {
    $name = "{0}-{1:D3}" -f $datePart, $nextNumber
    if (-not [string]::IsNullOrWhiteSpace($Suffix)) {
      $name = "$name-$Suffix"
    }

    $path = Join-Path $resolvedRoot $name
    try {
      New-Item -ItemType Directory -Path $path -ErrorAction Stop | Out-Null
      break
    } catch [System.IO.IOException] {
      $nextNumber++
    }
  }

  if (-not $NoLocation) {
    Set-Location -LiteralPath $path
  }

  Write-Host "[OK] Session: $path"
  return $path
}

Set-Alias -Name icc-session -Value New-IccSession -Scope Global

if ($MyInvocation.InvocationName -ne ".") {
  New-IccSession -Root $Root -Suffix $Suffix -Date $Date -NoLocation:$NoLocation
}
