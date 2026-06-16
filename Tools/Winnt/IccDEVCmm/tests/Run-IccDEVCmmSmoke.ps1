param(
    [string]$RepoRoot = (Resolve-Path "$PSScriptRoot\..\..\..\..").Path,
    [string]$Configuration = "Debug"
)

$ErrorActionPreference = "Stop"

$dllPath = Join-Path $RepoRoot "msvc\Tools\IccDEVCmm\x64\$Configuration\IccDEVCmm.dll"
$profilePath = Join-Path $RepoRoot "Testing\sRGB_v4_ICC_preference.icc"
$sourcePath = Join-Path $RepoRoot "Tools\Winnt\IccDEVCmm\tests\IccDEVCmmSmoke.cpp"
$outDir = Join-Path $RepoRoot "msvc\Tools\IccDEVCmm\tests\$Configuration"
$exePath = Join-Path $outDir "IccDEVCmmSmoke.exe"
$iccProfLibDir = Join-Path $RepoRoot "msvc\IccProfLib\$Configuration"
$cmmDir = Split-Path -Parent $dllPath

if (!(Test-Path $dllPath)) {
    throw "Missing IccDEVCmm DLL: $dllPath"
}
if (!(Test-Path $profilePath)) {
    throw "Missing test profile: $profilePath"
}
if (!(Test-Path $sourcePath)) {
    throw "Missing harness source: $sourcePath"
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

Write-Host "[INFO] Building IccDEVCmmSmoke.exe"
if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
    & cl.exe /nologo /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE $sourcePath /link /nologo /out:$exePath Mscms.lib
    if ($LASTEXITCODE -ne 0) {
        throw "cl failed with exit code $LASTEXITCODE"
    }
}
else {
    $programFilesX86 = [Environment]::GetFolderPath("ProgramFilesX86")
    $vswhere = Join-Path $programFilesX86 "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsRoot = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    }
    if (!$vsRoot) {
        $vsRoot = "C:\Program Files\Microsoft Visual Studio\2022\Community"
    }

    $vcvars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
    if (!(Test-Path $vcvars)) {
        throw "Missing vcvars64.bat: $vcvars"
    }

    $compileCmd = 'call "' + $vcvars + '" >nul && cl /nologo /std:c++17 /EHsc /W4 /DUNICODE /D_UNICODE "' +
                  $sourcePath + '" /link /nologo /out:"' + $exePath + '" Mscms.lib'
    & $env:ComSpec /d /c $compileCmd
    if ($LASTEXITCODE -ne 0) {
        throw "cl failed with exit code $LASTEXITCODE"
    }
}

$env:PATH = "$iccProfLibDir;$cmmDir;$env:PATH"

Write-Host "[INFO] Running IccDEVCmmSmoke.exe"
& $exePath $dllPath $profilePath
if ($LASTEXITCODE -ne 0) {
    throw "IccDEVCmmSmoke failed with exit code $LASTEXITCODE"
}
