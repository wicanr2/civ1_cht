@echo off
rem ============================================================
rem  Civilization 1 (1993 MicroProse) Traditional Chinese launcher
rem  Pure ASCII only - cmd.exe parses .bat as system ANSI codepage
rem  so any non-ASCII char here may break parsing.
rem ============================================================

setlocal
set DIR=%~dp0

rem Inject Big5 font substitution into otvdm redirected registry.
rem Charset 136 = 0x88 = CHINESEBIG5_CHARSET. MingLiU is Win10 built-in TC.
set KEY=HKCU\Software\otvdm\HKEY_LOCAL_MACHINE\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes
reg add "%KEY%" /v "CIVTIMES10,0" /d "MingLiU,136" /f >nul 2>&1
reg add "%KEY%" /v "CIVTIMES12,0" /d "MingLiU,136" /f >nul 2>&1
reg add "%KEY%" /v "CIVTIMES14,0" /d "MingLiU,136" /f >nul 2>&1
reg add "%KEY%" /v "CIVTIMES18,0" /d "MingLiU,136" /f >nul 2>&1
reg add "%KEY%" /v "CIVTIMES24,0" /d "MingLiU,136" /f >nul 2>&1
reg add "%KEY%" /v "CIVTIMES30,0" /d "MingLiU,136" /f >nul 2>&1
reg add "%KEY%" /v "CIVTIMES36,0" /d "MingLiU,136" /f >nul 2>&1

rem Launch CIV.EXE via otvdmw (windowed Win16 NTVDM replacement)
cd /d "%DIR%game"
start "" "%DIR%otvdm\otvdmw.exe" "%DIR%game\CIV.EXE"

endlocal
exit /b 0
