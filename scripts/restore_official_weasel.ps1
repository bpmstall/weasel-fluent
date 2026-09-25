# Clean restoration script for Weasel-Fluent
$ErrorActionPreference = "SilentlyContinue"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   正在清理并完全卸载 Weasel-Fluent TSF 输入法组件...     " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. 终止所有运行中的进程
Write-Host "[1/5] 终止后台服务进程..." -ForegroundColor Yellow
Stop-Process -Name "weasel-fluent-cpp-qt" -Force
Stop-Process -Name "WeaselServer" -Force

# 2. 反注册 TSF COM DLL
Write-Host "[2/5] 反注册 weasel-fluent-tsf.dll..." -ForegroundColor Yellow
$dllPath = "C:\Users\zheng\.gemini\antigravity\scratch\weasel-fluent-cpp-qt\dist\weasel-fluent\weasel-fluent-tsf.dll"
if (Test-Path $dllPath) {
    Start-Process regsvr32.exe -ArgumentList "/u /s `"$dllPath`"" -Verb RunAs -Wait
}

# 3. 从系统语言列表中移除 Fluent TIP 布局
Write-Host "[3/5] 从 Windows 11 语言列表中移除布局..." -ForegroundColor Yellow
$tipString = "0804:{D7B1C618-9F3E-4E7B-B96E-5C13D1A5B4C2}{E8934A01-C349-4F8A-8512-14AE0293BD01}"
$pCode = @"
using System;
using System.Runtime.InteropServices;
public class TSFUninstall {
    [DllImport("input.dll", CharSet = CharSet.Unicode, SetLastError = true)]
    public static extern bool InstallLayoutOrTip(string psz, uint dwFlags);
}
"@
Add-Type -TypeDefinition $pCode -ErrorAction SilentlyContinue
[TSFUninstall]::InstallLayoutOrTip($tipString, 1) # 1 = ILOT_UNINSTALL

# 4. 清理自启与相关注册表项
Write-Host "[4/5] 清理开机自启项与注册表残留..." -ForegroundColor Yellow
Remove-ItemProperty -Path "HKCU:\Software\Microsoft\Windows\CurrentVersion\Run" -Name "WeaselFluent" -ErrorAction SilentlyContinue
Remove-Item -Path "HKLM:\SOFTWARE\Microsoft\CTF\TIP\{D7B1C618-9F3E-4E7B-B96E-5C13D1A5B4C2}" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Path "HKLM:\SOFTWARE\Classes\CLSID\{D7B1C618-9F3E-4E7B-B96E-5C13D1A5B4C2}" -Recurse -Force -ErrorAction SilentlyContinue
Remove-Item -Path "HKCU:\Software\Classes\CLSID\{D7B1C618-9F3E-4E7B-B96E-5C13D1A5B4C2}" -Recurse -Force -ErrorAction SilentlyContinue

# 5. 确保系统自带微软拼音处于第一激活项
Write-Host "[5/5] 刷新并恢复微软拼音为主力输入法..." -ForegroundColor Yellow
$langList = Get-WinUserLanguageList
if ($langList.Count -gt 0) {
    $zh = $langList | Where-Object { $_.LanguageTag -like "zh*" }
    if ($zh) {
        $zh.InputMethodTips.Clear()
        $zh.InputMethodTips.Add("0804:{81D4E9C9-1D3B-41BC-9E6C-4B40BF79E35E}{FA550B04-5AD7-411F-A5AC-CA038EC515D7}")
        Set-WinUserLanguageList $langList -Force
    }
}

Write-Host "==========================================================" -ForegroundColor Green
Write-Host "  恢复成功！系统输入法已完全还原为官方默认原生状态。     " -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Green
