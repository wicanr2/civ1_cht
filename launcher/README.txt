Civilization 1 (1993 MicroProse) 繁體中文化版 Win10 portable
=============================================================

啟動方式：
    雙擊 Civilization-CHT.bat

包含元件：
    Civilization-CHT.bat   主啟動 launcher (純 ASCII)
    otvdm/                 Win16-on-Win10 runtime (otvdm v0.9.0)
    game/                  Civ1 patched 遊戲檔
        CIV.EXE            (Phase 1+3 patched: dialog + 主選單 Big5)
        CIVFONTS.FON       (Phase 2 patched: dfCharSet=0x88)
        Civdata*.RSC       資源檔
        *.wav              領袖語音

技術說明：
    Civ1 1993 是 Win16 NE，Win10 64-bit 無法原生跑。
    本套件用 otvdm (開源 Win16 reimpl) 在 Win10 上跑 Civ1。
    launcher 透過 otvdm 的 redirected registry 注入 Big5 font
    substitution (CIVTIMES12 -> MingLiU)，不污染 Win10 系統 registry。

如果遇到問題：
    1. 確認 Win10 內建 MingLiU 字型存在 (zh-TW 中文版預設有)
    2. 若中文字顯示成方塊或亂碼，回報以調整 launcher 字型對應
    3. 若 otvdm 啟動失敗，嘗試 Right-click otvdm/otvdmw.exe -> 屬性
       -> 相容性 -> 用相容模式執行: Windows XP SP3

著作權：
    Civilization for Windows (c) 1993 MicroProse Software, Inc.
    繁體中文化 patches 2026 by wicanr2 (CC BY-SA 4.0)
    otvdm by otya128 (LGPL 2.1+, github.com/otya128/winevdm)

GitHub: https://github.com/wicanr2/civ1_cht
