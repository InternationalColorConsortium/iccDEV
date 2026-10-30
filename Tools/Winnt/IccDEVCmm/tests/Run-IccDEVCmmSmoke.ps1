param(
    [string]$RepoRoot = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path,
    [string]$BuildDirectory = "msvc",
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Debug",
    [string]$DllPath = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path -LiteralPath $RepoRoot).Path
if ([IO.Path]::IsPathRooted($BuildDirectory)) {
    $buildRoot = (Resolve-Path -LiteralPath $BuildDirectory).Path
}
else {
    $buildRoot = (Resolve-Path -LiteralPath (Join-Path $RepoRoot $BuildDirectory)).Path
}

if ($DllPath) {
    $selectedDll = if ([IO.Path]::IsPathRooted($DllPath)) {
        $DllPath
    }
    else {
        Join-Path $RepoRoot $DllPath
    }
    $resolvedDllPath = (Resolve-Path -LiteralPath $selectedDll).Path
}
else {
    $dllCandidates = @(
        (Join-Path $buildRoot "bin\$Configuration\IccDEVCmm.dll"),
        (Join-Path $buildRoot "bin\IccDEVCmm.dll"),
        (Join-Path $buildRoot "Tools\IccDEVCmm\x64\$Configuration\IccDEVCmm.dll")
    )
    $resolvedDllPath = $dllCandidates |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
        Select-Object -First 1
    if (-not $resolvedDllPath) {
        throw "Missing IccDEVCmm.dll under $buildRoot. Build RefIccMAX for $Configuration or pass -DllPath."
    }
}

$profilePath = Join-Path $RepoRoot "Testing\sRGB_v4_ICC_preference.icc"
$namedColorProfilePath = Join-Path $RepoRoot ".github\ci\test-data\cmm-namedcolor-v4.icc"
$sourcePath = Join-Path $RepoRoot "Tools\Winnt\IccDEVCmm\tests\IccDEVCmmSmoke.cpp"
$cmmDir = Split-Path -Parent $resolvedDllPath
if (!(Test-Path -LiteralPath $resolvedDllPath -PathType Leaf)) {
    throw "Missing IccDEVCmm DLL: $resolvedDllPath"
}
if (!(Test-Path -LiteralPath $profilePath -PathType Leaf)) {
    throw "Missing test profile: $profilePath"
}

$harnessCandidates = @(
    (Join-Path $cmmDir "iccDEVCmmSmokeTest.exe"),
    (Join-Path $cmmDir "IccDEVCmmSmoke.exe"),
    (Join-Path $buildRoot "bin\$Configuration\iccDEVCmmSmokeTest.exe"),
    (Join-Path $buildRoot "bin\iccDEVCmmSmokeTest.exe"),
    (Join-Path $buildRoot "Tools\IccDEVCmm\tests\$Configuration\IccDEVCmmSmoke.exe")
)
$exePath = $harnessCandidates |
    Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    Select-Object -First 1

if (-not $exePath) {
    if (!(Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Missing harness source: $sourcePath"
    }
    $outDir = Join-Path $buildRoot "Tools\IccDEVCmm\tests\$Configuration"
    $exePath = Join-Path $outDir "IccDEVCmmSmoke.exe"
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null

    Write-Host "[INFO] Building standalone MSVC IccDEVCmmSmoke.exe"
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        & cl.exe /nologo /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE $sourcePath /link /nologo /out:$exePath Mscms.lib
        if ($LASTEXITCODE -ne 0) {
            throw "cl failed with exit code $LASTEXITCODE"
        }
    }
    else {
        $programFilesX86 = [Environment]::GetFolderPath("ProgramFilesX86")
        $vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
        if (Test-Path -LiteralPath $vswhere) {
            $vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        }
        if (!$vsRoot) {
            $vsRoot = "C:\Program Files\Microsoft Visual Studio\2022\Community"
        }

        $vcvars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
        if (!(Test-Path -LiteralPath $vcvars)) {
            throw "Missing vcvars64.bat: $vcvars"
        }

        $compileCmd = 'call "' + $vcvars + '" >nul && cl /nologo /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE "' +
                      $sourcePath + '" /link /nologo /out:"' + $exePath + '" Mscms.lib'
        & $env:ComSpec /d /c $compileCmd
        if ($LASTEXITCODE -ne 0) {
            throw "cl failed with exit code $LASTEXITCODE"
        }
    }
}

$harnessDir = Split-Path -Parent $exePath
$runtimeDirs = @($cmmDir)
$isolatedStage = [bool]($DllPath -and $harnessDir -eq $cmmDir)
if ($harnessDir -ne $cmmDir) {
    if ($DllPath) {
        Write-Warning "Harness runs from the build tree; this does not prove package isolation."
    }
    $runtimeDirs += @(
        $harnessDir,
        (Join-Path $buildRoot "bin\$Configuration"),
        (Join-Path $buildRoot "IccProfLib\$Configuration")
    )
}

# Match the CTest runtime lookup for build-tree runs. A staged harness beside
# an explicit DLL must use only staged DLLs so missing package dependencies fail.
if (-not $isolatedStage) {
    $cachePath = Join-Path $buildRoot "CMakeCache.txt"
    if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
        $cacheLines = Get-Content -LiteralPath $cachePath
        $vcpkgInstalled = $null
        foreach ($key in @("VCPKG_INSTALLED_DIR", "_VCPKG_INSTALLED_DIR")) {
            $line = $cacheLines | Where-Object { $_ -match "^${key}:[^=]*=" } |
                Select-Object -First 1
            if ($line) {
                $vcpkgInstalled = ($line -split "=", 2)[1]
                break
            }
        }
        $tripletLine = $cacheLines | Where-Object { $_ -match '^VCPKG_TARGET_TRIPLET:[^=]*=' } |
            Select-Object -First 1
        if ($vcpkgInstalled -and $tripletLine) {
            $triplet = ($tripletLine -split "=", 2)[1]
            $vcpkgPrefix = Join-Path $vcpkgInstalled $triplet
            if ($Configuration -eq "Debug") {
                $runtimeDirs += @(
                    (Join-Path $vcpkgPrefix "debug\bin"),
                    (Join-Path $vcpkgPrefix "bin")
                )
            }
            else {
                $runtimeDirs += @(
                    (Join-Path $vcpkgPrefix "bin"),
                    (Join-Path $vcpkgPrefix "debug\bin")
                )
            }
        }
    }
}

$originalPath = $env:PATH
try {
    $pathPrefix = ($runtimeDirs |
        Where-Object { Test-Path -LiteralPath $_ -PathType Container } |
        Select-Object -Unique) -join ';'
    if ($isolatedStage) {
        $env:PATH = "$pathPrefix;$([Environment]::SystemDirectory);$env:SystemRoot"
    }
    else {
        $env:PATH = "$pathPrefix;$originalPath"
    }
    Write-Host "[INFO] Running $exePath against $resolvedDllPath"
    $smokeArgs = @($resolvedDllPath, $profilePath)
    if (Test-Path -LiteralPath $namedColorProfilePath -PathType Leaf) {
        $smokeArgs += $namedColorProfilePath
    }
    & $exePath @smokeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "IccDEVCmmSmoke failed with exit code $LASTEXITCODE"
    }
}
finally {
    $env:PATH = $originalPath
}
