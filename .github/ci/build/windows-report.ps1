# ========================== Windows Build Report Script ==========================
# Windows Build Report
# Copyright (c) 2025-2026 International Color Consortium. All rights reserved.
#
# Use a Developer PowerShell terminal from the iccDEV repository root.
# Run via pwsh:
# iex (iwr -Uri "https://raw.githubusercontent.com/InternationalColorConsortium/iccDEV/refs/heads/ci-qa-flags/.github/ci/build/windows-report.ps1").Content
# ============================================================

[CmdletBinding()]
param(
    [string]$Root = (Get-Location).Path,
    [string]$LogFile = ".\build-data.log",
    [string]$HtmlFile = ".\iccdev_build_report.html",
    [int]$MaxListItems = 300
)

$ErrorActionPreference = "Stop"

$script:RootPath = [System.IO.Path]::GetFullPath($Root)
$script:LogFilePath = [System.IO.Path]::GetFullPath($LogFile)
$script:HtmlFilePath = [System.IO.Path]::GetFullPath($HtmlFile)
$script:IsWindowsHost = if (Get-Variable -Name IsWindows -ErrorAction SilentlyContinue) {
    $IsWindows
} else {
    $env:OS -eq "Windows_NT"
}

if (-not (Test-Path -LiteralPath $script:RootPath -PathType Container)) {
    throw "Root path does not exist: $script:RootPath"
}

Write-Host "============================= Starting Windows iccDEV Build Report =============================" -ForegroundColor Green
Write-Host "Copyright (c) 2025-2026 International Color Consortium. All rights reserved." -ForegroundColor Green
Write-Host "Last Updated: 30-JUN-2026 2200 UTC by iccDEV Maintainers" -ForegroundColor Green

function Write-LogLine {
    param(
        [AllowEmptyString()]
        [string]$Message = ""
    )

    $Message | Tee-Object -FilePath $script:LogFilePath -Append
}

function ConvertTo-RelativeReportPath {
    param(
        [Parameter(Mandatory=$true)]
        [System.IO.FileInfo]$File
    )

    $basePath = $script:RootPath.TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    ) + [System.IO.Path]::DirectorySeparatorChar
    $fullPath = [System.IO.Path]::GetFullPath($File.FullName)

    if ($fullPath.StartsWith($basePath, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $fullPath.Substring($basePath.Length)
    }

    return $fullPath
}

function ConvertTo-HtmlText {
    param(
        [AllowNull()]
        [object]$Value
    )

    if ($null -eq $Value) {
        return ""
    }

    return [System.Net.WebUtility]::HtmlEncode([string]$Value)
}

function Test-CommandAvailable {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Name
    )

    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Log-Command {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Name,

        [Parameter(Mandatory=$true)]
        [scriptblock]$ScriptBlock,

        [string[]]$RequiredCommands = @(),

        [switch]$WindowsOnly
    )

    Write-LogLine "=== Command: $Name ==="

    if ($WindowsOnly -and -not $script:IsWindowsHost) {
        Write-LogLine "[SKIP] Windows-only probe on non-Windows host"
        Write-LogLine
        return
    }

    foreach ($requiredCommand in $RequiredCommands) {
        if (-not (Test-CommandAvailable -Name $requiredCommand)) {
            Write-LogLine "[SKIP] Required command not found: $requiredCommand"
            Write-LogLine
            return
        }
    }

    try {
        $output = & $ScriptBlock 2>&1
        if ($null -eq $output) {
            Write-LogLine "[OK] Command produced no output"
        } else {
            foreach ($line in $output) {
                Write-LogLine ([string]$line)
            }
        }
    } catch {
        Write-LogLine "[ERROR] $($_.Exception.Message)"
    }

    Write-LogLine
}

function Test-ReportBinary {
    param(
        [Parameter(Mandatory=$true)]
        [System.IO.FileInfo]$File
    )

    $relativePath = ConvertTo-RelativeReportPath -File $File
    $normalizedPath = $relativePath -replace "\\", "/"

    $excludedPathPatterns = @(
        "(^|/)\.git/",
        "(^|/)CMakeFiles/",
        "(^|/)CMakeScratch/",
        "(^|/)CMakeTmp/",
        "(^|/)_deps/",
        "(^|/)Testing/",
        "(^|/)Tests?/",
        "(^|/)test-output/",
        "(^|/)install_manifest_files/"
    )

    foreach ($pattern in $excludedPathPatterns) {
        if ($normalizedPath -match $pattern) {
            return $false
        }
    }

    $excludedNamePatterns = @(
        "^CMake(C|CXX)CompilerId\.exe$",
        "^cmTC_.*",
        "^(ALL_BUILD|ZERO_CHECK|INSTALL|PACKAGE|RUN_TESTS)\.(exe|dll|lib)$"
    )

    foreach ($pattern in $excludedNamePatterns) {
        if ($File.Name -match $pattern) {
            return $false
        }
    }

    return $File.Name -match "^(?i:icc).*\.(exe|dll|lib)$"
}

function Add-SummaryRow {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Name,

        [Parameter(Mandatory=$true)]
        [object]$Value
    )

    return "            <tr><th>$(ConvertTo-HtmlText $Name)</th><td>$(ConvertTo-HtmlText $Value)</td></tr>"
}

function Add-FileList {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Title,

        [AllowEmptyCollection()]
        [array]$Files = @()
    )

    $items = @()
    $items += "        <h2>$(ConvertTo-HtmlText $Title)</h2>"

    if ($Files.Count -eq 0) {
        $items += "        <p>No matching files found.</p>"
        return ($items -join "`r`n")
    }

    $items += "        <ul>"
    foreach ($file in ($Files | Select-Object -First $MaxListItems)) {
        $relativePath = ConvertTo-RelativeReportPath -File $file
        $items += "            <li><code>$(ConvertTo-HtmlText $relativePath)</code> ($($file.Length) bytes)</li>"
    }

    if ($Files.Count -gt $MaxListItems) {
        $omitted = $Files.Count - $MaxListItems
        $items += "            <li>... $omitted additional item(s) omitted; increase -MaxListItems to include them.</li>"
    }

    $items += "        </ul>"
    return ($items -join "`r`n")
}

function Find-VisualStudioDevCmd {
    if (-not $script:IsWindowsHost) {
        return $null
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $installationPath = & $vswhere -latest -property installationPath 2>$null
        if ($installationPath) {
            $candidate = Join-Path $installationPath "Common7\Tools\VsDevCmd.bat"
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                return $candidate
            }
        }
    }

    $fallbacks = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat"
    )

    foreach ($candidate in $fallbacks) {
        if (Test-Path -LiteralPath $candidate -PathType Leaf) {
            return $candidate
        }
    }

    return $null
}

Set-Content -Path $script:LogFilePath -Value "=== Windows Build Host Report ===" -Encoding utf8
Write-LogLine "Last Updated: $(Get-Date)"
Write-LogLine "Repository Root: $script:RootPath"
Write-LogLine

$candidateFiles = @(Get-ChildItem -LiteralPath $script:RootPath -Recurse -File -Include *.exe,*.dll,*.lib -ErrorAction SilentlyContinue)
$reportFiles = @($candidateFiles | Where-Object { Test-ReportBinary -File $_ } | Sort-Object Extension, Name, FullName)
$executables = @($reportFiles | Where-Object { $_.Extension -ieq ".exe" })
$dynamicLibs = @($reportFiles | Where-Object { $_.Extension -ieq ".dll" })
$staticLibs = @($reportFiles | Where-Object { $_.Extension -ieq ".lib" })
$filteredCount = $candidateFiles.Count - $reportFiles.Count

Write-LogLine "=== Report Inventory Filter ==="
Write-LogLine "Candidate binaries scanned: $($candidateFiles.Count)"
Write-LogLine "ICC report binaries included: $($reportFiles.Count)"
Write-LogLine "Build internals and non-essential binaries filtered: $filteredCount"
Write-LogLine

Log-Command "PowerShell version" { $PSVersionTable.PSVersion.ToString() }
Log-Command "PowerShell edition" { $PSVersionTable.PSEdition }
Log-Command ".NET OS version" { [System.Environment]::OSVersion.VersionString }
Log-Command "systeminfo" { & systeminfo } -RequiredCommands @("systeminfo") -WindowsOnly
Log-Command "wmic os get Caption, Version, BuildNumber" { & wmic os get Caption, Version, BuildNumber } -RequiredCommands @("wmic") -WindowsOnly
Log-Command "wmic cpu get Name" { & wmic cpu get Name } -RequiredCommands @("wmic") -WindowsOnly
Log-Command "wmic logicaldisk get Caption, FileSystem, FreeSpace, Size" { & wmic logicaldisk get Caption, FileSystem, FreeSpace, Size } -RequiredCommands @("wmic") -WindowsOnly

Log-Command "Get-Command cl.exe" { Get-Command cl.exe | Select-Object -ExpandProperty Source } -RequiredCommands @("cl.exe") -WindowsOnly
Log-Command "Get-Command link.exe" { Get-Command link.exe | Select-Object -ExpandProperty Source } -RequiredCommands @("link.exe") -WindowsOnly
Log-Command "cl /Bv" { & cl.exe /Bv } -RequiredCommands @("cl.exe") -WindowsOnly

$firstExecutable = @($executables | Select-Object -First 1)
if ($firstExecutable.Count -gt 0) {
    $script:LinkHeadersTarget = $firstExecutable[0].FullName
    Log-Command "link /dump /headers <first ICC executable>" { & link.exe /dump /headers $script:LinkHeadersTarget } -RequiredCommands @("link.exe") -WindowsOnly
} else {
    Write-LogLine "=== Command: link /dump /headers <first ICC executable> ==="
    Write-LogLine "[SKIP] No ICC executable found in filtered report inventory"
    Write-LogLine
}

Log-Command "Get-CimInstance Win32_OperatingSystem Version" { (Get-CimInstance Win32_OperatingSystem).Version } -RequiredCommands @("Get-CimInstance") -WindowsOnly
Log-Command "Get-CimInstance Win32_OperatingSystem Caption" { (Get-CimInstance Win32_OperatingSystem).Caption } -RequiredCommands @("Get-CimInstance") -WindowsOnly
Log-Command "Current Windows build" { (Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\Windows NT\CurrentVersion").CurrentBuild } -WindowsOnly
Log-Command ".NET Framework v4 Full Release" { (Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\NET Framework Setup\NDP\v4\Full").Release } -WindowsOnly
Log-Command "Visual Studio registry roots" { Get-ItemProperty "HKLM:\SOFTWARE\Microsoft\VisualStudio\SxS\VS7" | Select-Object * } -WindowsOnly
Log-Command "vswhere latest installationVersion" {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        & $vswhere -latest -property installationVersion
    } else {
        "[SKIP] vswhere.exe not found"
    }
} -WindowsOnly
Log-Command "Windows SDK registry roots" { Get-ChildItem "HKLM:\SOFTWARE\Microsoft\Microsoft SDKs\Windows" | ForEach-Object { $_.Name } } -WindowsOnly
Log-Command "wsl --list --verbose" { & wsl.exe --list --verbose } -RequiredCommands @("wsl.exe") -WindowsOnly
Log-Command "Get-AppxPackage Microsoft.WindowsTerminal" { Get-AppxPackage -Name Microsoft.WindowsTerminal } -RequiredCommands @("Get-AppxPackage") -WindowsOnly

$vsDevCmd = Find-VisualStudioDevCmd
if ($vsDevCmd) {
    Log-Command "VsDevCmd.bat -no_logo && ver" { & cmd.exe /d /s /c "`"$vsDevCmd`" -no_logo && ver" } -RequiredCommands @("cmd.exe") -WindowsOnly
} else {
    Write-LogLine "=== Command: VsDevCmd.bat -no_logo && ver ==="
    Write-LogLine "[SKIP] Visual Studio developer command prompt batch file not found"
    Write-LogLine
}

$codesignedFiles = 0
$unsignedFiles = 0
$codesignedFilesList = @()
$unsignedFilesList = @()
$signatureStatus = "Skipped: Get-AuthenticodeSignature is only meaningful on Windows report hosts."

if ($script:IsWindowsHost -and (Test-CommandAvailable -Name "Get-AuthenticodeSignature")) {
    $signatureStatus = "Completed"
    foreach ($file in @($executables + $dynamicLibs)) {
        $signingStatus = Get-AuthenticodeSignature -FilePath $file.FullName
        if ($signingStatus.Status -eq "Valid") {
            $codesignedFilesList += $file
            $codesignedFiles++
        } else {
            $unsignedFilesList += $file
            $unsignedFiles++
        }
    }
}

$totalFiles = $executables.Count + $dynamicLibs.Count + $staticLibs.Count
$logContent = Get-Content -Raw -Path $script:LogFilePath

$summaryRows = @()
$summaryRows += Add-SummaryRow "Repository Root" $script:RootPath
$summaryRows += Add-SummaryRow "Candidate Binaries Scanned" $candidateFiles.Count
$summaryRows += Add-SummaryRow "Filtered Build Internals / Non-Essentials" $filteredCount
$summaryRows += Add-SummaryRow "Included ICC Binaries" $totalFiles
$summaryRows += Add-SummaryRow "Executables" $executables.Count
$summaryRows += Add-SummaryRow "Dynamic Libraries" $dynamicLibs.Count
$summaryRows += Add-SummaryRow "Static Libraries" $staticLibs.Count
$summaryRows += Add-SummaryRow "Code Signing Status" $signatureStatus
$summaryRows += Add-SummaryRow "Codesigned Files" $codesignedFiles
$summaryRows += Add-SummaryRow "Unsigned Files" $unsignedFiles

$htmlSections = @()
$htmlSections += Add-FileList "Executables" $executables
$htmlSections += Add-FileList "Dynamic Libraries" $dynamicLibs
$htmlSections += Add-FileList "Static Libraries" $staticLibs
$htmlSections += Add-FileList "Codesigned Files" $codesignedFilesList
$htmlSections += Add-FileList "Unsigned Files" $unsignedFilesList

$htmlContent = @"
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>iccDEV Developer Report</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; color: #222; }
        header, footer { border-bottom: 1px solid #ddd; margin-bottom: 1rem; padding-bottom: 0.5rem; }
        footer { border-top: 1px solid #ddd; border-bottom: 0; margin-top: 1rem; padding-top: 0.5rem; }
        table { width: 100%; border-collapse: collapse; margin-bottom: 1rem; }
        th, td { border: 1px solid #ddd; padding: 8px; text-align: left; vertical-align: top; }
        th { background-color: #f4f4f4; width: 28%; }
        code, pre { font-family: Consolas, "Courier New", monospace; }
        pre { background: #f7f7f7; border: 1px solid #ddd; padding: 1rem; overflow-x: auto; white-space: pre-wrap; }
    </style>
</head>
<body>
    <header>
        <h1>iccDEV Developer Report</h1>
        <p>Filtered Windows developer build inventory and host diagnostics.</p>
    </header>
    <main>
        <h2>Build Summary</h2>
        <table>
$($summaryRows -join "`r`n")
        </table>
$($htmlSections -join "`r`n")
        <h2>Build Host Details</h2>
        <pre>$(ConvertTo-HtmlText $logContent)</pre>
    </main>
    <footer>
        <p>(c) 2025-2026 International Color Consortium | All Rights Reserved.</p>
    </footer>
</body>
</html>
"@

$htmlContent | Out-File -FilePath $script:HtmlFilePath -Encoding utf8
Write-Host "HTML report generated at: $script:HtmlFilePath"
