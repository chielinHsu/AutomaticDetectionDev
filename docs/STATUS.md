# 進度表（STATUS）

新 session 開工前先讀這份，再讀 `docs/plan/Sxx-*.md`。步驟完成後由該步驟的 session 更新。

| 步驟 | 層級 | 主題 | 狀態 | 分支 | tag |
|---|---|---|---|---|---|
| S00 | 基礎 | 版控 + 總體計畫 + 架構導覽 | ✅ 完成 | `step/00-master-plan` | `step-00-done` |
| S01 | 介面 | UX 規格與可點擊原型 | ⏳ 待開始 | `step/01-ux-spec` | |
| S02 | 介面 | Qt UI 骨架與影像畫布 | ⬜ | `step/02-ui-shell` | |
| S03 | 系統 | 核心：ports、use case、DI、執行緒、PerfProbe | ⬜ | `step/03-core` | |
| S04 | 系統 | Presenter、互動狀態機、復原/重做 | ⬜ | `step/04-presenter` | |
| S05 | 底層 | Domain：晶格估計與補漏（純 C++） | ⬜ | `step/05-lattice` | |
| S06 | 底層 | 影像緩衝、轉接器、Recipe 儲存 | ⬜ | `step/06-image-storage` | |
| S07 | 底層 | OpenCV back end | ⬜ | `step/07-opencv` | |
| S08 | 底層 | HALCON back end | ⬜ | `step/08-halcon` | |
| S09 | 底層 | 契約測試與 A/B 比較 | ⬜ | `step/09-contract-ab` | |
| S10 | 驗證 | 效能測試 harness 與回歸閘門 | ⬜ | `step/10-perf-harness` | |
| S11 | 驗證 | 效能優化（先量再改） | ⬜ | `step/11-optimize` | |
| S12 | 發行 | 匯出、打包、最終文件 | ⬜ | `step/12-release` | |

圖例：✅ 完成　⏳ 下一個　⬜ 尚未開始　⚠️ 有未解決問題

## 預算校準狀態

`perf/budgets.json` 目前 `status: "proposed"`。S02 與 S07 完成實測後改為 `calibrated`。

## 未解決事項

- UI 工具集（Widgets vs QML）— S01 決定（ADR-001）
- MSVC 2022 工具鏈是否可用 — S02 第一件事驗證
- 真實板子影像（去識別化 1–2 張）— S07 前請使用者提供
- `ai-project-template/`（2026-10-05 在 S00 進行中出現，非本計畫產生）：保持未追蹤、不修改。**stage 時一律明確列出路徑，不要 `git add -A`。** 是否要套用此範本由使用者決定；若套用，需與本專案的 `CLAUDE.md` 手動合併。
