# Copyright (c) 2026 The International Color Consortium. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause

<#
.SYNOPSIS
  Convert hand-authored XML/JSON profiles and check binary round-trip fidelity.
.DESCRIPTION
  Writes seven <name>.json.icc / <name>.xml.icc pairs and the XML-only
  u16fixed16-xml-precision-coverage.xml.icc beside this script. Returned
  XML/JSON, reparse outputs, logs, text differences, and summaries go into a
  fresh QA directory outside Testing/Format.

  Put the six iccFrom*/iccTo*/iccDumpProfile/iccRoundTrip tools on PATH, or set
  ICCDEV_TOOLS_DIR (or -ToolsDir) to their build's Tools directory. Set
  ICCDEV_FORMAT_QA_OUTDIR (or -QaDir) to choose a fresh QA evidence directory.
#>
[CmdletBinding()]
param(
  [string]$ToolsDir = $env:ICCDEV_TOOLS_DIR,
  [string]$QaDir = $env:ICCDEV_FORMAT_QA_OUTDIR
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$Utf8NoBom = New-Object System.Text.UTF8Encoding($false)

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

function Invoke-ToolLogged {
  param(
    [Parameter(Mandatory = $true)][string]$Tool,
    [Parameter(Mandatory = $true)][string[]]$ToolArgs,
    [Parameter(Mandatory = $true)][string]$LogPath
  )

  # Windows PowerShell turns redirected native stderr into error records.
  # Preserve both streams and judge the process by its exit code.
  $priorPreference = $ErrorActionPreference
  $ErrorActionPreference = 'Continue'
  $global:LASTEXITCODE = -999
  try {
    & $Tool @ToolArgs *> $LogPath
    $code = $global:LASTEXITCODE
  }
  finally {
    $ErrorActionPreference = $priorPreference
  }
  return [int]$code
}

function Require-Output {
  param([Parameter(Mandatory = $true)][string]$Path)
  if (-not (Test-Path -LiteralPath $Path -PathType Leaf) -or
      (Get-Item -LiteralPath $Path).Length -eq 0) {
    throw "Missing or empty output: $Path"
  }
}

function Write-TextDiff {
  param(
    [Parameter(Mandatory = $true)][string]$Authored,
    [Parameter(Mandatory = $true)][string]$Returned,
    [Parameter(Mandatory = $true)][string]$DiffPath,
    [Parameter(Mandatory = $true)][System.Text.Encoding]$Encoding
  )

  $originalLines = [IO.File]::ReadAllLines($Authored)
  $returnedLines = [IO.File]::ReadAllLines($Returned)
  $lines = New-Object 'System.Collections.Generic.List[string]'
  $lines.Add("Authored: $Authored")
  $lines.Add("Returned: $Returned")
  $lines.Add('Lines prefixed - are authored; lines prefixed + are returned.')
  $differentLines = 0
  $count = [Math]::Max($originalLines.Length, $returnedLines.Length)
  for ($i = 0; $i -lt $count; $i++) {
    $original = if ($i -lt $originalLines.Length) { $originalLines[$i] } else { $null }
    $returned = if ($i -lt $returnedLines.Length) { $returnedLines[$i] } else { $null }
    if ($original -cne $returned) {
      $differentLines++
      if ($null -ne $original) { $lines.Add("- line $($i + 1): $original") }
      if ($null -ne $returned) { $lines.Add("+ line $($i + 1): $returned") }
    }
  }
  if ($differentLines -eq 0) {
    $lines.Add('Line content is equal; byte differences are in encoding or line endings.')
  }
  [IO.File]::WriteAllLines($DiffPath, $lines, $Encoding)
}

$fromJson = Resolve-Converter 'iccFromJson' 'IccFromJson'
$fromXml = Resolve-Converter 'iccFromXml' 'IccFromXml'
$toJson = Resolve-Converter 'iccToJson' 'IccToJson'
$toXml = Resolve-Converter 'iccToXml' 'IccToXml'
$dumpProfile = Resolve-Converter 'iccDumpProfile' 'IccDumpProfile'
$roundTrip = Resolve-Converter 'iccRoundTrip' 'IccRoundTrip'
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

if ([string]::IsNullOrWhiteSpace($QaDir)) {
  $qaBase = Join-Path $HOME 'qa'
  New-Item -ItemType Directory -Path $qaBase -Force | Out-Null
  $QaDir = Join-Path $qaBase ('iccdev-format-qa-' + [guid]::NewGuid().ToString('N'))
}
$qaRoot = [IO.Path]::GetFullPath($QaDir)
$formatRoot = [IO.Path]::GetFullPath($PSScriptRoot)
$formatPrefix = $formatRoot.TrimEnd([char[]]@('\', '/')) + [IO.Path]::DirectorySeparatorChar
$pathComparison = if ([IO.Path]::DirectorySeparatorChar -eq '\') {
  [StringComparison]::OrdinalIgnoreCase
} else {
  [StringComparison]::Ordinal
}
if ($qaRoot.Equals($formatRoot, $pathComparison) -or
    $qaRoot.StartsWith($formatPrefix, $pathComparison)) {
  throw "QA evidence directory must be outside Testing/Format: $qaRoot"
}
if (Test-Path -LiteralPath $qaRoot) {
  if (-not (Test-Path -LiteralPath $qaRoot -PathType Container)) {
    throw "QA evidence path is not a directory: $qaRoot"
  }
  if (Get-ChildItem -LiteralPath $qaRoot -Force | Select-Object -First 1) {
    throw "QA evidence directory must be fresh and empty: $qaRoot"
  }
} else {
  New-Item -ItemType Directory -Path $qaRoot -Force | Out-Null
}
$logsDir = Join-Path $qaRoot 'logs'
$reparsedDir = Join-Path $qaRoot 'reparsed'
$diffsDir = Join-Path $qaRoot 'diffs'
$profrawDir = Join-Path $qaRoot 'profraw'
New-Item -ItemType Directory -Path $logsDir, $reparsedDir, $diffsDir, $profrawDir | Out-Null
$conversionReport = Join-Path $qaRoot 'conversion-results.tsv'
$roundtripReport = Join-Path $qaRoot 'roundtrip-results.tsv'
$summaryReport = Join-Path $qaRoot 'summary.txt'
$tab = [char]9
$newline = [Environment]::NewLine
$conversionHeader = @('icc', 'format', 'authored_text_bytes',
  'returned_text_bytes', 'text_equal', 'diff_path',
  'reparsed_icc_equal', 'dump_status') -join $tab
[IO.File]::WriteAllText($conversionReport, $conversionHeader + $newline, $Utf8NoBom)
[IO.File]::WriteAllText($roundtripReport,
  (@('icc', 'class', 'exit', 'status') -join $tab) + $newline, $Utf8NoBom)
[IO.File]::WriteAllText($summaryReport, 'status=incomplete' + $newline, $Utf8NoBom)
Write-Host "QA evidence: $qaRoot"

$generatedIccFiles = @()
$dumpValid = 0
$reparseEqual = 0
$roundtripPass = 0
$roundtripUnsupported = 0

$priorProfileFile = $env:LLVM_PROFILE_FILE
$env:LLVM_PROFILE_FILE = Join-Path $profrawDir '%p-%m.profraw'
Push-Location -LiteralPath $PSScriptRoot
try {
  foreach ($name in @($profiles + $xmlOnlyProfile)) {
    $formats = if ($name -eq $xmlOnlyProfile) { @('xml') } else { @('json', 'xml') }
    foreach ($format in $formats) {
      $inputFile = "$name.$format"
      $outputFile = "$inputFile.icc"
      $inputPath = Join-Path $PSScriptRoot $inputFile
      $outputPath = Join-Path $PSScriptRoot $outputFile
      if (-not (Test-Path -LiteralPath $inputPath -PathType Leaf)) {
        throw "Missing authored input: $inputPath"
      }

      if (Test-Path -LiteralPath $outputPath) {
        if (-not (Test-Path -LiteralPath $outputPath -PathType Leaf)) {
          throw "Output path is not a file: $outputPath"
        }
        Remove-Item -LiteralPath $outputPath -Force
      }

      $converter = if ($format -eq 'json') { $fromJson } else { $fromXml }
      $logPath = Join-Path $logsDir "$outputFile.create.log"
      $code = Invoke-ToolLogged -Tool $converter -ToolArgs @($inputPath, $outputPath) -LogPath $logPath
      if ($code -ne 0) {
        Remove-Item -LiteralPath $outputPath -Force -ErrorAction SilentlyContinue
        throw "$inputFile conversion exited $code; see $logPath"
      }
      Require-Output -Path $outputPath
      $generatedIccFiles += $outputFile
      Write-Host "Created $outputFile"
    }
  }

  foreach ($iccName in $generatedIccFiles) {
    $iccPath = Join-Path $PSScriptRoot $iccName
    $dumpLog = Join-Path $logsDir "$iccName.dump.log"
    $code = Invoke-ToolLogged -Tool $dumpProfile -ToolArgs @('-v', '1', $iccPath) -LogPath $dumpLog
    $validVerdict = Select-String -LiteralPath $dumpLog -Pattern '^Profile is valid for version [0-9]+\.[0-9]+$' -Quiet
    if ($code -ne 0 -or -not $validVerdict) {
      throw "iccDumpProfile did not validate $iccName (exit $code); see $dumpLog"
    }
    $dumpValid++
    $originalHash = (Get-FileHash -LiteralPath $iccPath -Algorithm SHA256).Hash
    $baseName = $iccName -replace '\.(json|xml)\.icc$', ''

    foreach ($format in @('xml', 'json')) {
      $returnedPath = Join-Path $qaRoot "$iccName.$format"
      $toTool = if ($format -eq 'xml') { $toXml } else { $toJson }
      $fromTool = if ($format -eq 'xml') { $fromXml } else { $fromJson }
      $toLog = Join-Path $logsDir "$iccName.to-$format.log"
      $code = Invoke-ToolLogged -Tool $toTool -ToolArgs @($iccPath, $returnedPath) -LogPath $toLog
      if ($code -ne 0) { throw "iccTo$format exited $code for $iccName; see $toLog" }
      Require-Output -Path $returnedPath

      $reparsedPath = Join-Path $reparsedDir "$iccName.$format.icc"
      $fromLog = Join-Path $logsDir "$iccName.reparse-$format.log"
      $code = Invoke-ToolLogged -Tool $fromTool -ToolArgs @($returnedPath, $reparsedPath) -LogPath $fromLog
      if ($code -ne 0) { throw "Reparse of $iccName.$format exited $code; see $fromLog" }
      Require-Output -Path $reparsedPath
      $binaryEqual = if ($originalHash -eq
          (Get-FileHash -LiteralPath $reparsedPath -Algorithm SHA256).Hash) {
        'true'
      } else {
        'false'
      }
      if ($binaryEqual -eq 'true') { $reparseEqual++ }

      $authoredPath = Join-Path $PSScriptRoot "$baseName.$format"
      $authoredBytes = 'n/a'
      $textEqual = 'n/a'
      $diffRelative = '-'
      if (Test-Path -LiteralPath $authoredPath -PathType Leaf) {
        $authoredBytes = (Get-Item -LiteralPath $authoredPath).Length
        $textEqual = if ((Get-FileHash -LiteralPath $authoredPath -Algorithm SHA256).Hash -eq
            (Get-FileHash -LiteralPath $returnedPath -Algorithm SHA256).Hash) {
          'true'
        } else {
          'false'
        }
        if ($textEqual -eq 'false') {
          $diffRelative = "diffs/$iccName.$format.diff.txt"
          $diffPath = Join-Path $diffsDir "$iccName.$format.diff.txt"
          Write-TextDiff -Authored $authoredPath -Returned $returnedPath -DiffPath $diffPath -Encoding $Utf8NoBom
        }
      }
      $returnedBytes = (Get-Item -LiteralPath $returnedPath).Length
      $row = @($iccName, $format, $authoredBytes, $returnedBytes,
        $textEqual, $diffRelative, $binaryEqual, 'valid') -join $tab
      [IO.File]::AppendAllText($conversionReport, $row + $newline, $Utf8NoBom)
      if ($binaryEqual -ne 'true') {
        throw "Reparsed $iccName.$format differs from its ICC source; see $reparsedPath"
      }
    }
    $header = [IO.File]::ReadAllBytes($iccPath)
    if ($header.Length -lt 16) { throw "ICC header is too short: $iccPath" }
    $profileClass = [Text.Encoding]::ASCII.GetString($header, 12, 4)
    $roundtripLog = Join-Path $logsDir "$iccName.roundtrip.log"
    $code = Invoke-ToolLogged -Tool $roundTrip -ToolArgs @($iccPath) -LogPath $roundtripLog
    if ($profileClass -eq 'mntr' -and $code -eq 0) {
      $status = 'pass'
      $roundtripPass++
    }
    elseif ($profileClass -eq 'abst' -and
            $iccName -like 'acs-bookends-coverage.*.icc' -and
            ($code -eq 255 -or $code -eq -1) -and
            (Select-String -LiteralPath $roundtripLog -SimpleMatch -Pattern 'Unsupported profile class' -Quiet)) {
      $status = 'unsupported'
      $roundtripUnsupported++
    }
    else {
      $status = 'unexpected'
    }
    $roundtripRow = @($iccName, $profileClass, $code, $status) -join $tab
    [IO.File]::AppendAllText($roundtripReport, $roundtripRow + $newline, $Utf8NoBom)
    if ($status -eq 'unexpected') {
      throw "Unexpected iccRoundTrip result for $iccName (class $profileClass, exit $code); see $roundtripLog"
    }
  }
}
finally {
  Pop-Location
  if ($null -eq $priorProfileFile) {
    Remove-Item Env:LLVM_PROFILE_FILE -ErrorAction SilentlyContinue
  } else {
    $env:LLVM_PROFILE_FILE = $priorProfileFile
  }
}

if ($generatedIccFiles.Count -ne 15 -or $dumpValid -ne 15 -or
    $reparseEqual -ne 30 -or $roundtripPass -ne 13 -or
    $roundtripUnsupported -ne 2) {
  throw "Incomplete conversion QA: ICC=$($generatedIccFiles.Count), dump=$dumpValid, reparse=$reparseEqual, transform=$roundtripPass, unsupported=$roundtripUnsupported"
}
$summaryLines = @(
  'status=pass',
  'generated_icc=15',
  'dump_valid=15',
  'reparsed_icc_equal=30/30',
  'roundtrip_pass=13',
  'roundtrip_unsupported=2',
  "conversion_report=$conversionReport",
  "roundtrip_report=$roundtripReport"
)
[IO.File]::WriteAllText($summaryReport, ($summaryLines -join $newline) + $newline, $Utf8NoBom)
Write-Host "Converted $($profiles.Count) JSON and $($profiles.Count + 1) XML inputs in $PSScriptRoot"
Write-Host 'iccDumpProfile valid: 15/15; returned XML/JSON ICC byte equality: 30/30'
Write-Host 'iccRoundTrip transforms: 13 pass; 2 abstract profiles unsupported'
Write-Host "QA report: $summaryReport"
