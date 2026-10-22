# <ID> — <步驟名稱>：目的、原因、效果

> 規則：英文區塊用 ASD-STE100 風格（短句、主動語態、一詞一義）。繁中摘要放在每段之後。
> 檢查：`node tools/ste-lint.mjs`
> 範圍：Step Protocol 的步驟與進度表中類型為「功能」的需求要有四種輸出；小修正免（見 `project-rules.md`）。

## 修改紀錄

| 日期 | 修改者 | 說明 |
|---|---|---|
| YYYY-MM-DD | | 建立 |

<!--STE-->
## Purpose
(What does this step build? Two to four short sentences.)
<!--/STE-->

**繁中**：（這一步做什麼）

<!--STE-->
## Reason
(Why do we build it this way? What problem does it solve?)
<!--/STE-->

**繁中**：（為什麼這樣設計；放棄了哪些替代方案、為什麼）

<!--STE-->
## Effect
(What does the user or the developer gain? Give a number when you have one.)
<!--/STE-->

**繁中**：（使用者或開發者得到什麼；有量測就寫數字，沒量測就明說是目標值）

## SOLID 對照

| 原則 | 本步驟如何落實 | 如何驗證 |
|---|---|---|
| S | | |
| O | | |
| L | | |
| I | | |
| D | | |

## 如何驗證

- [ ] 建置通過
- [ ] 測試通過（列出測試名稱）
- [ ] `node tools/ste-lint.mjs` 無 error
- [ ] `perf.md` 內的指標都已量測（若專案有預算，已與預算比對）；沒有效能指標已明寫
