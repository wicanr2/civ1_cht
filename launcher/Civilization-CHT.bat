@echo off
REM Civilization 1 Windows 繁體中文化版 portable launcher
REM 1993 Sid Meier MicroProse / 繁中化 2026 wicanr2

REM Set Big5 font substitution for CIVTIMES* faces (Win10 native MingLiU has Big5 glyphs)
REM Uses otvdm's redirected registry so we don't touch real HKLM
setlocal

set DIR=%~dp0

REM Add CIVTIMES -> MingLiU font subst (Big5 charset 136 = CHINESEBIG5_CHARSET)
reg add "HKCU\Software\otvdm\HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes" /v "CIVTIMES10,0"  /d "MingLiU,136" /f >nul 2>&1
reg add "HKCU\Software\otvdm\HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes" /v "CIVTIMES12,0"  /d "MingLiU,136" /f >nul 2>&1
reg add "HKCU\Software\otvdm\HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes" /v "CIVTIMES14,0"  /d "MingLiU,136" /f >nul 2>&1
reg add "HKCU\Software\otvdm\HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes" /v "CIVTIMES18,0"  /d "MingLiU,136" /f >nul 2>&1
reg add "HKCU\Software\otvdm\HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes" /v "CIVTIMES24,0"  /d "MingLiU,136" /f >nul 2>&1
reg add "HKCU\Software\otvdm\HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes" /v "CIVTIMES30,0"  /d "MingLiU,136" /f >nul 2>&1
reg add "HKCU\Software\otvdm\HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes" /v "CIVTIMES36,0"  /d "MingLiU,136" /f >nul 2>&1

REM Launch via otvdmw (windowed Win16 NTVDM replacement)
cd /d "%DIR%game"
"%DIR%otvdm\otvdmw.exe" "%DIR%game\CIV.EXE"

endlocal
