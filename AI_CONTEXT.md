# AutoCadCreated — AI Context

> 只寫「這個專案的事實」。工作流程在 `.agents/harness/engineering-workflow.md`，規則在 `.agents/harness/project-rules.md`，不要在此複製。
> 本檔的內容原本在 `CLAUDE.md`，2026-10-05 搬進共用結構（見 `docs/adr/000-ai-agent-structure.md`）。

## 專案用途

AOI 教學工具：使用者在影像上框一個樣本 ROI → 程式找出相似元件 → 推算陣列間距並補漏 → 產生檢測框（"CAD objects"）。
**OpenCV 與 HALCON 兩個 back end 共用同一份程式碼**，執行時可切換。

- 完整計畫：`docs/00-master-plan.md`
- 目前進度：`docs/current-status.md`（開工前先讀）
- 每一步的細節與交接 prompt：`docs/plan/Sxx-*.md`
- 效能測試計畫：`docs/perf-test-plan.md`；預算：`perf/budgets.json`

## 技術棧

| 層 | 技術 | 版本／備註 |
|---|---|---|
| 語言 | C++ | `domain` 層只用標準庫，見 `docs/architecture.md` |
| UI | Qt Widgets（預設）＋ QGraphicsView | 與 QML 的取捨由 S01 的 ADR-001 定案 |
| 建置 | CMake ＋ Ninja | S02 建立 `CMakeLists.txt` 與 `CMakePresets.json` |
| 視覺 | OpenCV、HALCON | 只能在對應的 `infrastructure/backend_*` 使用 |
| 測試 | Qt Test | ADR-003（S03 定案），以 CTest 執行 |
| 工具腳本 | Node.js | `tools/*.mjs` |
| 平台 | Windows 11、MSVC 2022 | 編譯器尚未驗證，見下表 |

## 環境（Windows 11，本機實際探測結果）

| 項目 | 位置 |
|---|---|
| Qt 6.8.0 MSVC2022 64-bit | `D:\Qt\6.8.0\msvc2022_64` |
| CMake / Ninja | `D:\Qt\Tools\CMake_64\bin` / `D:\Qt\Tools\Ninja` |
| OpenCV (vc16) | `D:\opencv\opencv\build`（`OpenCVConfig.cmake` 在此） |
| HALCON 24.11 Progress-Steady | `%HALCONROOT%` = `C:\Program Files\MVTec\HALCON-24.11-Progress-Steady` |
| MSVC 2022 編譯器 | **尚未驗證**，S02 第一件事：確認 `vcvars64.bat` 可用 |

- Shell 是 Windows PowerShell 5.1：沒有 `&&`；commit 訊息寫成檔案再 `git commit -F <檔案>`，不要用 `-F -`（格式見 `project-rules.md`「Commit 訊息」）。
- Node 可用，工具腳本放 `tools/*.mjs`。`python.exe` 只是 Store 捷徑，不要依賴。
- 以上路徑若在 S02 驗證後有差異，**更新此表**。

## 重要目錄

| 路徑 | 內容 |
|---|---|
| `src/` | 程式碼，依五層分（`domain`、`application`、`infrastructure`、`presentation`、`app`）。尚未建立，S02 起 |
| `docs/` | 計畫、ADR、AI 交接文件 |
| `docs/plan/` | S01–S12 每一步的計畫與 Start prompt |
| `docs/steps/` | 每一步的四種輸出；範本在 `_template/` |
| `docs/adr/` | 設計決策，一檔一決策（`NNN-slug.md`） |
| `perf/` | 效能預算；實測結果在 `perf/results/`（不入版控） |
| `tools/` | Node 工具：`ste-lint.mjs`；`check-deps.mjs` 在 S03 建立 |
| `ai-project-template/` | 與本產品無關的 AI 結構範本，保持未追蹤。不要修改、不要 stage，除非使用者要求 |
| `.agents/`、`.claude/`、`.ai/`、`scripts/ai/` | AI agent 的規範、skill、交接單與檢查腳本，各檔職責見 `.agents/harness/engineering-workflow.md` 第 0 節 |

## 建置與測試

驗證指令是 agent 判斷「做完了沒有」的唯一依據。尚未建立的檢查，回報時標明「不適用（尚未建立）」，不要寫成通過。

| 目的 | 指令 | 備註 |
|---|---|---|
| 建置 | 尚未建立 | S02 產生 `CMakePresets.json`（debug / release / relwithdebinfo），之後把指令填在這裡 |
| 單元與契約測試 | 尚未建立 | S03 起，Qt Test 經 CTest |
| 依賴方向檢查 | `node tools/check-deps.mjs` | S03 建立；掃 `#include`，違規即失敗 |
| 文件風格檢查 | `node tools/ste-lint.mjs` | 現在可用；只檢查 `<!--STE-->` 標記之間的文字 |
| 效能 | S10 的 harness | 預算在 `perf/budgets.json` |
| AI 結構檢查 | `powershell -ExecutionPolicy Bypass -File scripts/ai/check-ai-structure.ps1` | 改過入口檔或 skill 後執行 |

`VERIFIED` 的定義（取自 Step Protocol 第 3 步）：建置通過、單元與契約測試通過、`check-deps` 與 `ste-lint` 皆無 error，且本步驟的效能指標已量測並與 `perf/budgets.json` 比對。失敗就如實回報，不要宣稱完成。

## 限制與不變條件

- 分層與依賴方向見 `docs/architecture.md`，違規由 `check-deps` 擋下。
- 不要在 `domain` 或 `application` include Qt、`cv::`、`HalconCpp`。
- 不要讓 `cv::Mat`、`HalconCpp::HImage`、`QImage` 跨過 port（用 `ImageView`）。
- 不要沒量測就做效能優化（S11 規則：先有 before 數字）。
- 不要把效能預算數字當事實：`perf/budgets.json` 內為「提議值」，要在 S02 與 S07 用本機實測校準後才轉為正式。
- 真實產線影像（`/data/`）、`build/`、`perf/results/` 不入版控。

## Context Routing（任務 → 先讀什麼）

只載入與任務相關的文件，不要一次讀完。

| 任務 | 必讀文件／skill | 注意 |
|---|---|---|
| 開始任一步驟 | `docs/current-status.md`、`docs/plan/Sxx-*.md`、`project-rules.md` 的 Step Protocol | 一個 session 只做一步 |
| 改分層、新增模組 | `docs/architecture.md`、`docs/decisions.md`（索引，連到 `docs/adr/`） | 已記錄的決定沒有新證據不推翻 |
| 寫步驟文件 | `docs/steps/_template/`、`project-rules.md` 的「每一步的四種輸出」 | 跑 `node tools/ste-lint.mjs` |
| 效能相關 | `docs/perf-test-plan.md`、`perf/budgets.json` | 沒有 before 數字不動手 |
| 接續別人的工作 | `.ai/handoff.md`、`docs/changelog-ai.md` | 先看 `git status`，別覆蓋未完成的修改 |
| 收工、換 agent | `ai-handoff` skill | |

## 詞彙表

| 詞 | 意義 |
|---|---|
| ROI | 使用者在影像上框選的樣本區域 |
| CAD objects | 程式產生的檢測矩形 |
| back end | 視覺實作：OpenCV 或 HALCON，執行時可切換 |
| port | application 層定義的介面，由 infrastructure 的 adapter 實作 |
| `ImageView` | 跨過 port 傳遞影像的型別，不暴露任何函式庫的影像類別 |
| Step Protocol | 每完成一步必做的流程，見 `project-rules.md` |
