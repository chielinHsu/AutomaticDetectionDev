# 目前狀態

> 原地修改，反映「現在」。每次收工更新；歷史在 `changelog-ai.md`。需求進度表的規則見 `.agents/harness/engineering-workflow.md`「需求展開與進度表」。

## 需求進度表

| 編號 | 類型 | 需求／細項 | 狀態 | 計畫 | 分支／commit | 驗證 |
|---|---|---|---|---|---|---|
| R01 | 雜項 | 套用 AI 共用結構範本（2.0.0） | ✅ | [ADR-000](adr/000-ai-agent-structure.md) | 範本建立（{{DATE}}） | 範本建立時未驗證；以 `scripts/ai/check-ai-structure.ps1` 檢查 |
| R02 | 雜項 | 填寫所有 `TODO(project)`：`AI_CONTEXT.md`、`project-rules.md`、`MODEL_ROUTING.md`「本專案案例」、`docs/00-master-plan.md`、`docs/architecture.md` | ⬜ | | | |
| R02.1 | | `AI_CONTEXT.md`：用途、技術棧、建置與測試指令 | ⬜ | | | |
| R02.2 | | `project-rules.md`：風格、Git 授權範圍、不提交的路徑 | ⬜ | | | |
| R02.3 | | `MODEL_ROUTING.md`「本專案案例」與 `00-master-plan.md`、`architecture.md` | ⬜ | | | |

圖例：✅ 完成　⏳ 進行中　⬜ 尚未開始　⚠️ 有未解決問題　⛔ 取消

### 待使用者決定

1. 無。

## 目前進度

- 更新日期：{{DATE}}
- 階段：專案建立

## 進行中

- 無。

## 待辦（依優先順序）

1. 完成上表 R02（填寫 `TODO(project)`），再用 `scripts/ai/check-ai-structure.ps1 -Strict` 確認。
2. 依 `docs/00-master-plan.md` 規劃步驟，把第一個需求展開進上表。

## 已知問題

- 無。

## 阻塞與待使用者決定

- 無。
