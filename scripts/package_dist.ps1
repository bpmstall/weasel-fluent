$ErrorActionPreference = "Stop"

$ProjectDir = "C:\Users\zheng\.gemini\antigravity\scratch\weasel-fluent-cpp-qt"
$DistBase = "$ProjectDir\dist"
$DistDir = "$DistBase\weasel-fluent"

Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Packaging Weasel-Fluent C++ Qt + Rime Standalone Bundle   " -ForegroundColor Cyan
Write-Host "============================================================" -ForegroundColor Cyan

# 1. Prepare dist directory
if (Test-Path $DistDir) {
    Remove-Item -Recurse -Force $DistDir
}
New-Item -ItemType Directory -Path $DistDir -Force | Out-Null

# 2. Copy Frontend Executable
$ExeSrc = "$ProjectDir\build\Release\weasel-fluent-cpp-qt.exe"
if (-not (Test-Path $ExeSrc)) {
    Write-Host "Executable not found at $ExeSrc! Running xmake build..." -ForegroundColor Yellow
    Push-Location $ProjectDir
    xmake build
    Pop-Location
}
Copy-Item $ExeSrc -Destination "$DistDir\weasel-fluent-cpp-qt.exe" -Force
Write-Host "[1/7] Copied weasel-fluent-cpp-qt.exe" -ForegroundColor Green

# 3. Deploy Qt6 Runtime
$Windeployqt = "C:\Qt\6.8.2\msvc2022_64\bin\windeployqt.exe"
if (Test-Path $Windeployqt) {
    Write-Host "[2/7] Running windeployqt to bundle Qt6 dependencies..." -ForegroundColor Green
    & $Windeployqt "$DistDir\weasel-fluent-cpp-qt.exe" --no-translations --no-compiler-runtime | Out-Null
} else {
    Write-Host "[WARNING] windeployqt not found at $Windeployqt" -ForegroundColor Yellow
}

# 4. Copy Librime Backend (rime.dll)
$RimeDll = "C:\Program Files\Rime\weasel-0.17.4\rime.dll"
if (Test-Path $RimeDll) {
    Copy-Item $RimeDll -Destination "$DistDir\rime.dll" -Force
    Write-Host "[3/7] Copied Librime 64-bit backend (rime.dll)" -ForegroundColor Green
}

# 5. Copy Rime Shared Data
$RimeDataSrc = "C:\Program Files\Rime\weasel-0.17.4\data"
if (Test-Path $RimeDataSrc) {
    Copy-Item -Recurse $RimeDataSrc -Destination "$DistDir\data" -Force
    Write-Host "[4/7] Copied Rime shared data directory" -ForegroundColor Green
}

# 6. Copy Rime Ice Dictionaries & Schemas
$UserRimeSrc = "$env:APPDATA\Rime"
$UserDictsDst = "$DistDir\user_dicts"
New-Item -ItemType Directory -Path $UserDictsDst -Force | Out-Null
if (Test-Path $UserRimeSrc) {
    Get-ChildItem -Path $UserRimeSrc -Exclude "*.userdb","*.log" | ForEach-Object {
        Copy-Item -Recurse $_.FullName -Destination $UserDictsDst -Force
    }
    Write-Host "[5/7] Copied Rime-Ice full schemas and dictionaries" -ForegroundColor Green
}

# 7. Copy Assets and Config
$AssetsSrc = "$ProjectDir\assets"
if (Test-Path $AssetsSrc) {
    Copy-Item -Recurse $AssetsSrc -Destination "$DistDir\assets" -Force
}
$DefaultConfig = @{
    orientation = "horizontal"
    page_size = 5
    font_size = 12
    font_family = "Microsoft YaHei UI"
    theme_mode = "system"
    accent_color = "auto"
    sound_enabled = $false
    show_preedit = $false
    show_label = $true
    show_comment = $true
} | ConvertTo-Json -Depth 4
$DefaultConfig | Out-File -FilePath "$DistDir\config.json" -Encoding utf8
Write-Host "[6/7] Copied assets and generated default config.json" -ForegroundColor Green

# 8. Create Scripts
$InstallScript = @"
@echo off
chcp 65001 >nul
title Rime Fluent IME - 启动并接管系统输入
echo ===================================================
echo   正在停止原版 Weasel 服务并启动 Rime Fluent 前端...
echo ===================================================

taskkill /F /IM WeaselServer.exe 2>nul
taskkill /F /IM weasel-fluent-cpp-qt.exe 2>nul

echo 正在启动 Rime Fluent 输入服务（任务栏托盘驻留）...
start "" "%~dp0weasel-fluent-cpp-qt.exe"

echo 启动完成！可在任意应用中输入拼音进行打字。
echo 按 Shift 切换中英文，按 Tab 展开 4 行候选词大矩阵，托盘图标右键可进入设置中心。
exit
"@
$InstallScript | Out-File -FilePath "$DistDir\install_and_run.bat" -Encoding utf8

$StopScript = @"
@echo off
chcp 65001 >nul
title 停止 Rime Fluent IME 服务
taskkill /F /IM weasel-fluent-cpp-qt.exe 2>nul
echo Rime Fluent 服务已停止。
"@
$StopScript | Out-File -FilePath "$DistDir\stop.bat" -Encoding utf8

$SettingsScript = @"
@echo off
start "" "%~dp0weasel-fluent-cpp-qt.exe" --settings
"@
$SettingsScript | Out-File -FilePath "$DistDir\settings.bat" -Encoding utf8

Write-Host "[7/7] Generated control scripts (install_and_run.bat, stop.bat, settings.bat)" -ForegroundColor Green

# 9. Compress to zip
$ZipDst = "$DistBase\weasel-fluent-v1.0.0-windows-x64.zip"
if (Test-Path $ZipDst) {
    Remove-Item -Force $ZipDst
}
Write-Host "Compressing bundle into $ZipDst..." -ForegroundColor Cyan
Compress-Archive -Path "$DistDir\*" -DestinationPath $ZipDst -CompressionLevel Optimal

$ZipSizeMB = [math]::Round(((Get-Item $ZipDst).Length / 1MB), 2)
Write-Host "============================================================" -ForegroundColor Cyan
Write-Host "  Packaging complete! Artifact: $ZipDst ($ZipSizeMB MB)     " -ForegroundColor Green
Write-Host "============================================================" -ForegroundColor Cyan
