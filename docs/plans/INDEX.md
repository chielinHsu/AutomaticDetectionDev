# 計畫索引

> 所有計畫的單一入口（AI 結構 3.0.0）。只做導覽：每個計畫一列，細項與進度在各計畫的資料夾裡。
> 只在新增計畫或計畫層級的狀態改變時更新。規則見 `.agents/harness/engineering-workflow.md`「計畫分開管理」；新計畫從 [`_template/`](_template/) 複製。
> 3.0.0 之前已完成的需求記在 [`../legacy/current-status.md`](../legacy/current-status.md) 的需求進度表（只讀），這裡只列遷移時還沒完成的需求與之後的新計畫。

下一個可用編號：**R11**

| 編號 | 類型 | 計畫 | 狀態 | 位置 | 依賴／相關 | 分支 |
|---|---|---|---|---|---|---|
| R10 | 功能 | HALCON 建好樣本後在訊息列建議用 `M` 點端子的遮罩當比對區（R07 的後續 (1)） | ⏳ 進行中 | [active/R10-halcon-mask-hint](active/R10-halcon-mask-hint/plan.md) | R07、ADR-028、ADR-035、ADR-042 | `claude/r09-halcon-mask-hint-f0d064` |
| R09 | 功能 | PCB 多類元件深度學習 CAD：OBB 與實例分割，含標註／訓練流程 | ⏳ 進行中 | [active/R09-deep-learning-cad](active/R09-deep-learning-cad/plan.md) | 先交付 R08 區域 CAD | `codex/r09-vision-workbench` → `main`（本機） |
| R08 | 功能 | Socket 焊點教學最小版本 | ⏳ 進行中 | [active/R08-socket-teaching](active/R08-socket-teaching/plan.md) | ADR-015、ADR-033 | `feat/r08-socket-teaching`（2026-10-30 收進 `main`；R08.8 仍進行中） |
| R07 | 修正 | 多樣本搜尋的合併與驗證規則：樣本越多結果不應越差（主因是晶格：R07.2 索引區域、R07.8／R07.9 補漏的回報規則；R07.4 取消） | ✅ 完成 | [completed/R07-multi-sample-merge](completed/R07-multi-sample-merge/plan.md) | ADR-013、ADR-025、ADR-026、ADR-035 | `fix/r07-multi-sample`（2026-10-29 合併回 `main`） |
| R06 | 文件 | AI 結構 3.0.0：每個計畫一個資料夾（含範本升級） | ✅ 完成 | [completed/R06-ai-structure-v3](completed/R06-ai-structure-v3/plan.md) | R04 | `docs/ai-structure-v3` |
| R05 | 重構 | Rust 混合架構第一階段：domain 的演算法搬進 Rust（R05.1–R05.5；預設 `rust`，`cpp` 保留） | ✅ 完成 | [completed/R05-rust-domain](completed/R05-rust-domain/plan.md) | ADR-031 | `refactor/r05-<N>-<slug>` |
| S12 | 功能 | 匯出、打包、最終文件 | ⬜ 規劃中 | [active/S12-release](active/S12-release/plan.md) | S11 | `step/12-release` |

圖例：⬜ 規劃中　⏳ 進行中　⚠️ 受阻／暫停　✅ 完成　⛔ 取消。新的計畫加在表格最上方。
