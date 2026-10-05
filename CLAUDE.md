# CLAUDE.md — AutoCadCreated

AOI 教學工具：使用者框一個樣本 ROI → 程式找出相似元件 → 推算陣列間距並補漏 → 產生檢測框（"CAD objects"）。
**OpenCV 與 HALCON 兩個 back end 共用同一份程式碼**，執行時可切換。

- 完整計畫：`docs/00-master-plan.md`
- 目前進度：`docs/STATUS.md`（開工前先讀）
- 每一步的細節與交接 prompt：`docs/plan/Sxx-*.md`
- 效能測試計畫：`docs/perf-test-plan.md`；預算：`perf/budgets.json`

## 環境（Windows 11，本機實際探測結果）

| 項目 | 位置 |
|---|---|
| Qt 6.8.0 MSVC2022 64-bit | `D:\Qt\6.8.0\msvc2022_64` |
| CMake / Ninja | `D:\Qt\Tools\CMake_64\bin` / `D:\Qt\Tools\Ninja` |
| OpenCV (vc16) | `D:\opencv\opencv\build`（`OpenCVConfig.cmake` 在此） |
| HALCON 24.11 Progress-Steady | `%HALCONROOT%` = `C:\Program Files\MVTec\HALCON-24.11-Progress-Steady` |
| MSVC 2022 編譯器 | **尚未驗證**，S02 第一件事：確認 `vcvars64.bat` 可用 |

- Shell 是 Windows PowerShell 5.1：沒有 `&&`；用 `git commit -m @'...'@`（here-string），不要用 `-F -`。
- Node 可用，工具腳本放 `tools/*.mjs`。`python.exe` 只是 Store 捷徑，不要依賴。
- 以上路徑若在 S02 驗證後有差異，**更新此表**。

## 分層與依賴規則（Dependency Rule）

```
app ──► presentation ──► application ──► domain
 │                           ▲              ▲
 └──────► infrastructure ────┴──────────────┘   (實作 application 的 ports)
```

| 層（`src/` 下） | 可以依賴 | 禁止 |
|---|---|---|
| `domain` | C++ 標準庫 | Qt、OpenCV、HALCON、任何 I/O |
| `application` | `domain` | Qt、OpenCV、HALCON |
| `infrastructure/backend_opencv` | `application`(ports)、`domain`、OpenCV | Qt Widgets、HALCON、其他 adapter |
| `infrastructure/backend_halcon` | `application`(ports)、`domain`、HALCON | Qt Widgets、OpenCV、其他 adapter |
| `infrastructure/storage`、`perf` | `application`、`domain`、Qt Core | Qt Widgets、視覺函式庫 |
| `presentation` | `application`、`domain`、Qt Widgets | OpenCV、HALCON |
| `app` | 全部（只做組裝） | 業務規則、繪圖 |

`tools/check-deps.mjs`（S03 建立）會在 CTest 檢查 `#include` 方向，違規即失敗。

## SOLID 檢查清單（每個 PR / commit 前自問）

- **S** 這個類別只有一個變更理由嗎？超過 300 行要在 commit 說明。
- **O** 新增功能是「新增類別 + 註冊」，還是「修改既有 use case」？後者要有理由。
- **L** 新的 `IPatternSearcher` 實作能通過同一組契約測試（S09）嗎？
- **I** 介面 ≤ 5 個方法？UI 只看到它需要的最小介面？
- **D** use case 只依賴 port，具體 adapter 只在 `app/` 組裝？

## 每一步都必須交付四種輸出（Karpathy 的 4 種輸出形式）

目的：讓讀者清楚知道這個設計的**目的（Purpose）、原因（Reason）、效果（Effect）**。放在 `docs/steps/Sxx-<slug>/`，範本在 `docs/steps/_template/`。

| # | 輸出 | 檔案 | 要求 |
|---|---|---|---|
| 1 | 寫作 Writing | `spec.md` | 用 ASD-STE100 風格英文寫 Purpose / Reason / Effect（放在 `<!--STE-->` … `<!--/STE-->` 之間），每段後附繁中摘要。短句（描述句 ≤ 25 字、程序句 ≤ 20 字）、主動語態、一詞一義。執行 `node tools/ste-lint.mjs` 必須無 error。這是長度/被動語態的檢查，**不是**完整 STE 驗證。 |
| 2 | 圖解 Diagrams | `diagram.md` | Mermaid 圖（結構圖、流程圖、序列圖、狀態圖擇需要者）。能用圖就不要用長文。 |
| 3 | 互動網頁 Web page | `explainer.html` | 單一檔案、離線可開、深色/淺色皆可讀、手機寬度可用；要有可操作的元件（slider、按鈕、步進）。需要的文字放在 `<script type="application/json" id="aoi-data">`，其中 `ste` 欄位會被 lint 檢查。 |
| 4 | 解說影片 Explainer video | `storyboard.md` | 分鏡表（時間、畫面、旁白 STE 英文、繁中字幕）。實際影片/語音合成是選配，需使用者同意再做。 |

另外每一步要有 `perf.md`：本步驟量測了哪些指標、預算、實測值、機器指紋。

## Step Protocol（每完成一個步驟必做；不要在同一個 session 連做下一步）

1. 開工：讀 `docs/STATUS.md` 與 `docs/plan/Sxx-*.md`。在 `main` 上建分支 `step/NN-slug`。
2. 實作，並同時產出四種輸出與 `perf.md`。
3. 驗證：建置通過、單元/契約測試通過、`node tools/check-deps.mjs`（S03 起）、`node tools/ste-lint.mjs` 皆無 error；本步驟的效能指標已量測並與 `perf/budgets.json` 比對。**失敗就如實回報，不要宣稱完成。**
4. 更新 `docs/STATUS.md`。
5. Commit（Conventional Commits，訊息範本在該步驟的 plan 檔），合併到 `main`（`git merge --no-ff`），打 tag `step-NN-done`。僅限本機，**不要 push**，除非使用者要求。
6. 對使用者**提示 commit 內容**：commit hash、訊息、變更檔案摘要、效能結果、未解決事項。
7. 呼叫 `spawn_task` 為下一步建立新 session（prompt 取自**下一步** plan 檔底部的「Start prompt」）。若無法呼叫，改為把 prompt 印出來讓使用者貼上。**到此為止，不要接著做下一步。**

## Commit 規則

- Conventional Commits：`feat(ui): …`、`feat(core): …`、`feat(opencv): …`、`test(perf): …`、`docs(ux): …`、`chore: …`。
- 結尾加上 harness 指定的 `Co-Authored-By` 行。
- 一個 commit 一個意圖；格式化、重新命名不要混進功能 commit。
- 不要提交：`build/`、真實產線影像（`/data/`）、`perf/results/`。

## 不要做的事

- 不要在 `domain` / `application` include Qt、`cv::`、`HalconCpp`。
- 不要讓 `cv::Mat` / `HalconCpp::HImage` / `QImage` 跨過 port（用 `ImageView`）。
- 不要沒量測就做效能優化（S11 規則：先有 before 數字）。
- 不要把效能預算數字當事實：`perf/budgets.json` 內為「提議值」，要在 S02 / S07 用本機實測校準後才轉為正式。
