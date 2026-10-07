# HALCON 24.11 SDK（標頭與匯入程式庫的複本）

| 修改日期 | 修改者 | 說明 |
|---|---|---|
| 2026-10-07 | Claude Code (Claude Opus 5.5) | S08 建立：使用者指示從 CV_Algorithm 專案複製 |

這台機器的 HALCON 安裝（`%HALCONROOT%` ＝ `C:\Program Files\MVTec\HALCON-24.11-Progress-Steady`）只有 runtime，沒有 `include\` 與 `lib\`。
使用者 2026-10-07 指示：從另一個專案複製 HALCON 的開發檔到本專案。

- **來源**：`D:\AOI_Project\SType\AlgoParamFineTune\BnSDKLibsController\CV_Algorithm\HALCON-24.11-Steady\`（該專案也把這份 SDK 放在版控裡）。
- **版本**：24.11.2（`include/HVersNum.h`），和這台機器的 runtime `24.11.2.0`（`%HALCONROOT%\version.txt`）相同。
- **複製了什麼**：`include/` 整個資料夾（照原樣）；`lib/x64-win64/halconcpp.lib`（C++ 介面的匯入程式庫，CV_Algorithm 也只連這一個）。
  沒有複製 `halcon.lib`、`halconc.lib`、`*xl.lib`（XL 版給超過 32k 像素的影像）、`hdevengine*.lib`：用不到。
- **不要改這些檔案**：`.gitattributes` 設了 `-text`，保持原本的位元組（標頭是 CRLF）。要換版本就整份換掉，並更新本檔與 `AI_CONTEXT.md`。
- **執行時**還需要 `%HALCONROOT%\bin\x64-win64` 的 DLL（安裝程式已放進 PATH）與授權（`%HALCONROOT%\license\`）。這兩樣不在這裡。
- **授權條款**：HALCON 是 MVTec 的商業軟體，這份複本只供本專案在公司內部建置使用。
- CMake 用 `cmake/FindHalconLocal.cmake` 找這裡；`AOI_HALCON_SDK_DIR` 可以指到別的 SDK。
