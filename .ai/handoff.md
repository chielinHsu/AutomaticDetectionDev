# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Opus 5.5)
- 日期：2026-10-06

## 正在做

無。**S06（影像緩衝、轉接器、配方與樣本庫儲存）已完成**：分支 `step/06-image-storage`，以 `--no-ff` 合併到 `main`，tag `step-06-done`。全部只在本機，**沒有 push**。S07 由新的 session 做。
**S06 中使用者做的決定**：（1）樣本庫照 **TRI 的分層**（專案 → 使用者 → 唯讀的系統）；（2）這一步**不接存檔／開啟配方的 UI**，但它們是必要功能，已列在 `docs/current-status.md` 待辦；（3）影像**也有 12 bit**；（4）**改用 Qt 6.12.0**（6.8.0 仍在機器上）。
**仍等使用者決定**：ADR-009（I1）、U3 超標、T3 要不要涵蓋散佈的點；新增：存檔／開啟配方的 UI 放進哪一步（使用者已決定：分層樣本庫各層的檔案位置，等接樣本庫 UI 的那一步和這兩個 UI 一起定）、S06 的指標要不要有預算。

## 已完成（S06）

- domain：`ImageView` 的所有權規則 1–6、`Bgr24`／`Gray16`、`significantBits`（12 bit）、`subView`；`ImageBuffer`（緊密排列）、`image_copy`（複製計數）、`image_depth`（看最大值推測位元數）。
- application：`ImageSourceChain`（Qt 在前、OpenCV 在後）、`RecipeSaver`（背景存檔，同一個檔案依序、被取代的回 Cancelled）；port 契約補完；use case 的對比檢查讀新格式與 12 bit。
- infrastructure：新 adapter `image_qt`（Qt Core＋Gui；`check-deps` 加 `qt-gui` 類別與 2 個 fixture，共 15 個）、`storage`（JSON：`JsonCodec`、`JsonFile`、`JsonRecipeStore`、`JsonLibraryStore`、`LayeredLibraryStore`）、`backend_opencv`（`OpenCvImages`、`OpenCvImageSource`；**OpenCV 第一次連結成功**，preset `release-opencv`／`debug-opencv`）。
- presentation：開圖改走 `IImageSource`，訊號帶 `ImageView`；`ImageLoader`、`ImageViewAdapter` 刪除；12 bit 轉 8 bit 顯示（計數）。
- tests：`tests/infra/`、`tests/support/`（合成板子產生器）、`tst_image_view`、`tst_image_chain`、`tst_recipe_saver`；變異 31 個抓到 30 個（M24 等價）。
- 文件：`docs/steps/S06-image-storage/`（四種輸出＋`perf.md`）、ADR-014、ADR-015、`perf/baseline/casparhsu1-storage.json`、`docs/plan/S07`–`S12` 的「S06 完成後的提醒」。

## 下一步

**S07：OpenCV back end**（`docs/plan/S07-opencv.md`，開頭有 S06 的提醒）。開工時向使用者要真實影像，並問 12 bit 的影像怎麼存（16 bit 容器、值 0..4095？還是緊密打包的 TIFF？）。

## 不要動

- `ai-project-template/`；`docs/ux/tokens.json`、`copy.json`；`perf/budgets.json` 的 limit（S06 一個都沒改）；`knownOver("U3")` 等 S11。
- `tests/infra/fixtures/recipe-v1.json`、`library-v1.json`：v1 的範例檔，手寫，**永遠要讀得進來**；格式改了就升版＋遷移，不要改範例檔。
- `AOI_SYNTHETIC_REFERENCE_HASH`（`tests/infra/CMakeLists.txt`）：合成板子產生器的像素雜湊；只有刻意改產生器時才更新。
- `tests/ui/PresenterRig.h` 的視窗閘門；晶格的門檻與 ADR-013 的規則；port 契約、`ImageView`、`SearchParams`；`check-deps` 的 `RULES` 與 `docs/architecture.md` 的依賴表要一起改。

## 已知問題與風險

- **Qt 6.8.0 的 `qUncompress` 對截斷的資料是未定義行為**（Debug 撞上 `Q_UNREACHABLE`）：參考影像因此不壓縮；6.12.0 有沒有修好沒有驗證（已不呼叫它）。
- 開圖在 UI 執行緒解碼：25 MP 的 PNG 約 0.5 秒畫面停住。HALCON 的轉換與來源沒有做（沒有開發檔）。
- 已知例外：16 階灰色表的 BMP（Qt 給 Bgrx32、OpenCV 給 Gray8）；灰階＋alpha 的 PNG 沒有驗證。有效位元數是看最大值推測的。
- 分層的樣本庫（規則已做好）還沒有組裝進程式：三層的檔案路徑等接樣本庫 UI 的那一步再定（使用者決定，建議值在 ADR-015）；樣本庫存一筆要整份重寫。
- 這台機器同一份程式會飄 1.4–2 倍；S02–S05 的基準線是 Qt 6.8.0 的數字（`perf.md` 有 6.8.0 對 6.12.0 的一次比較）。
- 建置環境：`env.ps1` 之後才 configure／build；換 Qt 版本要刪掉 `build/<preset>` 重建；含中文或引號的檔案用 Write 寫（bash heredoc 會吃掉反斜線，JS 的 `replace` 會把 `$$` 變成 `$`）；Qt Test 要 `-o 檔案`。

## 驗證（都已實際執行）

- 見 `docs/changelog-ai.md` 本步驟那一筆（Qt 6.12.0 的 Release、Release＋OpenCV、Debug、Debug＋OpenCV、qmake＋jom、check-deps、ste-lint、Mermaid、效能 3 次、變異 31 個）。
