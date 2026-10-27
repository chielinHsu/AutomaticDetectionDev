---
id: R06
title: AI 結構 3.0.0：每個計畫一個資料夾（含範本升級）
type: 文件
status: 完成
created: 2026-10-27
updated: 2026-10-27
depends_on: [R04]
related: [R05]
branch: docs/ai-structure-v3
---

# R06 — AI 結構 3.0.0：每個計畫一個資料夾（含範本升級）

## 目標

依使用者 2026-10-27 的要求（ChatGPT「架構分類建議」），把共用範本與本專案的全域需求進度表、AI 工作紀錄改成每個計畫一個資料夾，不同計畫的紀錄分開維護；並同步五個工作資料夾的共用規則檔與其他專案。決策：[ADR-032](../../../adr/032-ai-structure-v3.md)。

## 範圍

- `ai-project-template/` 升到 3.0.0（工作流程「計畫分開管理」、`docs/plans/_template/`、檢查與遷移腳本、`template-overrides.txt`）。
- 本專案的遷移；五份共用 `CLAUDE.md`（`D:\AOI_Project\Qt`、`D:\ClaudeWork`、`D:\CodexWork`、`D:\CopilotWork`、`D:\GoogleWork`，不在本 repository）；其他 9 個專案各自提交（不在本 repository）。

## 不做

- 拆分 2.x 已完成需求的歷史紀錄（原樣封存在 `docs/legacy/`）。

## 細項

| 編號 | 細項 | 狀態 | 分支／commit | 驗證 |
|---|---|---|---|---|
| R06.1 | 範本 3.0.0：工作流程、入口、skill、agents、`docs/plans/_template/`、`check-ai-structure.ps1`、`migrate-ai-structure-3.mjs`、README | ✅ | `docs/ai-structure-v3` 495d62d、6015d32 | 新專案的檢查 OK；負面測試（搬走計畫不改索引）4 個 FAIL；DiskAnalyzer／QAdvancePinInspect 複本試遷移 |
| R06.2 | `-Refresh` 不覆蓋 `.agents/harness/template-overrides.txt` 登記的檔案 | ✅ | 03161ec | 複本測試輸出 KEEP、內容保留 |
| R06.3 | 五份共用 `CLAUDE.md` 同步改成 3.0.0 | ✅ | （不在版控） | 五份 md5 相同 |
| R06.4 | 其他 9 個專案遷移 | ✅ | 各專案自己的 commit 與 PR | 各專案的 `check-ai-structure.ps1` |
| R06.5 | 本專案遷移：舊紀錄封存、S12 與 R05 拆成計畫、規則文字、ADR-032、`template-overrides.txt` | ✅ | `docs/ai-structure-v3` | `check-ai-structure.ps1 -Strict`、`check-model-routing.mjs`、`ste-lint` |

圖例：✅ 完成　⏳ 進行中　⬜ 尚未開始　⚠️ 有未解決問題　⛔ 取消

## 完成定義

- [x] 範本與本專案的結構檢查通過；其他專案各自驗證並提交
- [x] `git grep` 只剩 `docs/legacy/`、歷史 ADR、步驟的舊輸出與說明遷移的句子提到 2.x 的做法
