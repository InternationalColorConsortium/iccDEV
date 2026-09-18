# Start of Script
Write-Host "============================= Starting Windows iccDEV PATH Modification & Report =============================" -ForegroundColor Green
Write-Host "Copyright (c) 2025-2026 International Color Consortium. All rights reserved." -ForegroundColor Green
Write-Host "Last Updated: 29-JUN-2026 1200 UTC by iccDEV Maintainers" -ForegroundColor Green
          $exeDirs = Get-ChildItem -Recurse -File -Include *.exe,*.lib,*.dll,*.exp -Path msvc |
              Where-Object { $_.FullName -match 'icc' -and $_.FullName -notmatch '\\CMakeFiles\\' -and $_.Name -notmatch '^CMake(C|CXX)CompilerId\.exe$' } |
              ForEach-Object { Split-Path $_.FullName -Parent } |
              Sort-Object -Unique
          $env:PATH = ($exeDirs -join ';') + ';' + $env:PATH
          $env:PATH -split ';' | Select-String "icc"
          $toolDirs = Get-ChildItem -Recurse -File -Include *.exe -Path .\Tools\ | ForEach-Object { Split-Path -Parent $_.FullName } | Sort-Object -Unique
          $env:PATH = ($toolDirs -join ';') + ';' + $env:PATH
          $env:PATH -split ';'
          pwd
Write-Host "Modified PATH" -ForegroundColor Green
Write-Host "Running Test" -ForegroundColor Green
iccToXml
Write-Host "iccDEV Path Modifications Completed" -ForegroundColor Green