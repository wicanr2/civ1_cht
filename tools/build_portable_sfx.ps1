# Build Civilization-CHT-portable.exe single-file Win10 SFX
# Pre-requisites: 7-Zip installed at C:\Program Files\7-Zip (7z.exe + 7z.sfx)
# Input: stage/ tree containing {game/, otvdm/, Civilization-CHT.bat}
# Output: Civilization-CHT-portable.exe

$ErrorActionPreference = 'Stop'

$root  = 'D:\03_game_tmp\_sfx_build_civ1'
$stage = "$root\stage"
$7z    = 'C:\Program Files\7-Zip\7z.exe'
$sfx   = 'C:\Program Files\7-Zip\7z.sfx'
$out   = "$root\Civilization-CHT-portable.exe"
$archive = "$root\civ1cht.7z"

# 1. Compress stage tree with max compression
if (Test-Path $archive) { Remove-Item $archive -Force }
Write-Host "[1/3] Compressing stage tree..."
$prevCwd = Get-Location
Set-Location $stage
& $7z a -mx9 -mf=on -t7z $archive '.\*' | Select-Object -Last 5
Set-Location $prevCwd

$arcSize = (Get-Item $archive).Length
Write-Host "  archive size: $([math]::Round($arcSize/1MB, 2)) MB"

# 2. Build SFX config (UTF-8 BOM required by 7zsd)
$cfgPath = "$root\sfx_config.txt"
$cfgText = @'
;!@Install@!UTF-8!
Title="Civilization 1 (1993) 繁體中文化 portable"
BeginPrompt="解壓並啟動 Civilization for Windows 繁體中文化版?"
ExtractTitle="解壓中..."
ExtractDialogText="正在解壓 Civilization 中文化版..."
RunProgram="Civilization-CHT.bat"
GUIMode="1"
OverwriteMode="2"
;!@InstallEnd@!
'@
# Write UTF-8 with BOM (7zsd config requires)
$bom = [byte[]](0xEF, 0xBB, 0xBF)
$normalized = $cfgText -replace "`r?`n", "`r`n"
$txt = [System.Text.Encoding]::UTF8.GetBytes($normalized)
[System.IO.File]::WriteAllBytes($cfgPath, $bom + $txt)
Write-Host "[2/3] SFX config written ($($cfgText.Length) chars)"

# 3. Concatenate: 7z.sfx + config + archive -> output .exe
if (Test-Path $out) { Remove-Item $out -Force }
Write-Host "[3/3] Building SFX..."
$sfxBytes = [System.IO.File]::ReadAllBytes($sfx)
$cfgBytes = [System.IO.File]::ReadAllBytes($cfgPath)
$arcBytes = [System.IO.File]::ReadAllBytes($archive)
$total = $sfxBytes + $cfgBytes + $arcBytes
[System.IO.File]::WriteAllBytes($out, $total)

$outSize = (Get-Item $out).Length
Write-Host "  output: $out"
Write-Host "  size:   $([math]::Round($outSize/1MB, 2)) MB"
Write-Host "Done. Double-click to test."
