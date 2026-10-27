# 計畫索引

> 所有計畫的單一入口（AI 結構 3.0.0）。只做導覽：每個計畫一列，細項與進度在各計畫的資料夾裡。
> 只在新增計畫或計畫層級的狀態改變時更新。規則見 `.agents/harness/engineering-workflow.md`「計畫分開管理」；新計畫從 [`_template/`](_template/) 複製。
> 3.0.0 之前已完成的需求記在 [`../legacy/current-status.md`](../legacy/current-status.md) 的需求進度表（只讀），這裡只列遷移時還沒完成的需求與之後的新計畫。

下一個可用編號：**R07**

| 編號 | 類型 | 計畫 | 狀態 | 位置 | 依賴／相關 | 分支 |
|---|---|---|---|---|---|---|
| R06 | 文件 | AI 結構 3.0.0：每個計畫一個資料夾（含範本升級） | ✅ 完成 | [completed/R06-ai-structure-v3](completed/R06-ai-structure-v3/plan.md) | R04 | `docs/ai-structure-v3` |
| R05 | 重構 | Rust 混合架構第一階段：domain 的演算法搬進 Rust（R05.1–R05.4 完成） | ⏳ 進行中 | [active/R05-rust-domain](active/R05-rust-domain/plan.md) | ADR-031 | `refactor/r05-<N>-<slug>` |
| S12 | 功能 | 匯出、打包、最終文件 | ⬜ 規劃中 | [active/S12-release](active/S12-release/plan.md) | S11 | `step/12-release` |

圖例：⬜ 規劃中　⏳ 進行中　⚠️ 受阻／暫停　✅ 完成　⛔ 取消。新的計畫加在表格最上方。
