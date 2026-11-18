# Copyright (c) 2026 The International Color Consortium. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause

<#
.SYNOPSIS
  Convert the hand-authored XML and JSON coverage profiles to ICC files.
.DESCRIPTION
  Writes seven <name>.json.icc / <name>.xml.icc pairs and the XML-only
  u16fixed16-xml-precision-coverage.xml.icc beside this script. Put
  iccFromJson and iccFromXml on PATH, or set ICCDEV_TOOLS_DIR (or -ToolsDir)
  to the build's Tools directory or a directory containing both executables.
#>
[CmdletBinding()]
param(
  [string]$ToolsDir = $env:ICCDEV_TOOLS_DIR
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Resolve-Converter {
  param(
    [Parameter(Mandatory = $true)][string]$ToolName,
    [Parameter(Mandatory = $true)][string]$ToolSubdirectory
  )

  if (-not [string]::IsNullOrWhiteSpace($ToolsDir)) {
    $subdirectory = Join-Path $ToolsDir $ToolSubdirectory
    $directories = @(
      $ToolsDir,
      $subdirectory,
      (Join-Path $subdirectory 'Debug'),
      (Join-Path $subdirectory 'Release')
    )
    foreach ($directory in $directories) {
      foreach ($suffix in @('.exe', '.cmd', '')) {
        $candidate = Join-Path $directory ($ToolName + $suffix)
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
          return (Resolve-Path -LiteralPath $candidate).ProviderPath
        }
      }
    }
    throw "Cannot find $ToolName below ICCDEV_TOOLS_DIR/ToolsDir: $ToolsDir"
  }

  $command = Get-Command -Name $ToolName -CommandType Application `
    -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($null -eq $command) {
    throw "Cannot find $ToolName on PATH; set ICCDEV_TOOLS_DIR or -ToolsDir"
  }
  return $command.Source
}

$fromJson = Resolve-Converter 'iccFromJson' 'IccFromJson'
$fromXml = Resolve-Converter 'iccFromXml' 'IccFromXml'
$profiles = @(
  'acs-bookends-coverage',
  'compressed-text-coverage',
  'format-feature-coverage',
  'sequence-metadata-coverage',
  'metadata-numeric-coverage',
  'dict-localization-coverage',
  'response-curve-coverage'
)
$xmlOnlyProfile = 'u16fixed16-xml-precision-coverage'

Push-Location -LiteralPath $PSScriptRoot
try {
  foreach ($name in @($profiles + $xmlOnlyProfile)) {
    $formats = if ($name -eq $xmlOnlyProfile) { @('xml') } else { @('json', 'xml') }
    foreach ($format in $formats) {
      $inputFile = "$name.$format"
      $outputFile = "$inputFile.icc"
      if (-not (Test-Path -LiteralPath $inputFile -PathType Leaf)) {
        throw "Missing authored input: $inputFile"
      }

      if (Test-Path -LiteralPath $outputFile) {
        if (-not (Test-Path -LiteralPath $outputFile -PathType Leaf)) {
          throw "Output path is not a file: $outputFile"
        }
        Remove-Item -LiteralPath $outputFile -Force
      }

      $converter = if ($format -eq 'json') { $fromJson } else { $fromXml }
      Write-Host "$($converter): $inputFile -> $outputFile"
      try {
        & $converter $inputFile $outputFile
        if ($LASTEXITCODE -ne 0) {
          throw "$inputFile conversion exited $LASTEXITCODE"
        }
        if (-not (Test-Path -LiteralPath $outputFile -PathType Leaf) -or
            (Get-Item -LiteralPath $outputFile).Length -eq 0) {
          throw "$inputFile conversion did not produce a nonempty ICC file"
        }
      }
      catch {
        Remove-Item -LiteralPath $outputFile -Force -ErrorAction SilentlyContinue
        throw
      }
    }
  }
}
finally {
  Pop-Location
}

Write-Host "Converted $($profiles.Count) JSON and $($profiles.Count + 1) XML inputs in $PSScriptRoot"
