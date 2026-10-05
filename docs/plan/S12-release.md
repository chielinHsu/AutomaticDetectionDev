# S12 — 匯出、打包、最終文件

| 項目 | 內容 |
|---|---|
| 層級 | 發行 |
| 分支 | `step/12-release` |
| 依賴 | S11 |
| 產出位置 | `src/infrastructure/export/`、`packaging/`、`docs/manual/`、`docs/steps/S12-release/` |

<!--STE-->
## Purpose
This step finishes the first release. It adds export, builds an installer and completes the user manual.

## Reason
The user needs the boxes in other tools. A program that the user cannot install or learn has no value.

## Effect
The user installs the program, makes the boxes, exports them and finds the answer in the manual.
<!--/STE-->

**繁中**：讓第一版可以真正交給人用：匯出、安裝、說明文件。

## 任務

1. **匯出**：Recipe JSON（定案 schema）、CSV（座標/角度/狀態/分數）、DXF（矩形，作法在 ADR-005 決定：自寫簡易 writer 或使用函式庫）。匯出走 `IExporter` port，每種格式一個 adapter。
2. **打包**：`windeployqt`、OpenCV DLL、**HALCON 執行環境與授權的說明**（HALCON 授權不隨安裝檔散布，文件要清楚寫出）；沒有 HALCON 的安裝選項。
3. **Release 重跑**：在 Release 組態重跑 S10 全套，更新 baseline；確認 `/W4 /WX` 乾淨。
4. **使用手冊**（`docs/manual/`，STE 英文＋繁中）：快速開始、每個工具、疑難排解（無匹配、授權、效能慢）。
5. **文件總覽**：`docs/index.html` 互動入口，串起每一步的 explainer、diagram、storyboard、perf 結果。
6. **解說影片**：詢問使用者是否要實際製作（語音合成與剪輯工具選擇需要使用者決定）；不要擅自呼叫付費服務。
7. **v0.1.0 tag**，撰寫 CHANGELOG。

## SOLID 重點

匯出格式以 adapter 增減（O）；使用者介面只依賴 `IExporter`（D）。

## 效能檢查（寫入 `perf.md`）

Release 版 S10 全套；匯出 5 000 個物件的時間；安裝後冷啟動時間（目標 ≤ 2 s，量測後確認）。

## 四種輸出（本步驟具體內容）

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | 匯出與打包的 Purpose/Reason/Effect；手冊目錄 |
| 2 | diagram.md | 匯出 adapter 結構圖；安裝內容圖（哪些 DLL 從哪來） |
| 3 | explainer.html | 文件總覽入口＋「30 秒上手」互動導覽 |
| 4 | storyboard.md | 90 秒產品介紹（若使用者同意才製作成片） |

## 完成定義

- [ ] 在乾淨的 Windows 環境安裝並能啟動（有／無 HALCON 兩種）
- [ ] 三種匯出格式的往返或外部工具開啟測試通過
- [ ] Release 版效能全套通過
- [ ] 手冊與文件入口完成
- [ ] `node tools/ste-lint.mjs --strict` 無 error
- [ ] tag `v0.1.0`

## Commit 範本

```
feat(release): add exporters, packaging, manual and docs index

- JSON/CSV/DXF exporters behind IExporter
- windeployqt packaging, optional HALCON runtime notes
- Release-build performance baseline refreshed
- User manual (STE + zh-TW) and interactive documentation index
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續 AOI 專案。請先讀 CLAUDE.md、docs/STATUS.md、docs/plan/S12-release.md，
再依 CLAUDE.md 的 Step Protocol 完成步驟 S12（分支 step/12-release）。只做 S12。
製作實際解說影片前先詢問使用者是否同意與工具選擇。完成後：commit、合併到 main、打 tag v0.1.0、更新 STATUS.md、對我提示 commit 內容。
```
