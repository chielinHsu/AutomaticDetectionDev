# S01 — UX 規格與可點擊原型

| 項目 | 內容 |
|---|---|
| 層級 | 介面架構（設計） |
| 分支 | `step/01-ux-spec` |
| 依賴 | S00 |
| 產出位置 | `docs/steps/S01-ux-spec/`、`docs/ux/`、`docs/adr/` |

<!--STE-->
## Purpose
This step defines how the user works with the program. It builds a clickable prototype before any Qt code.

## Reason
A wrong screen layout costs more to fix after coding. A prototype shows problems in minutes.

## Effect
The team agrees on screens, states and shortcuts first. Step S02 then builds a known design.
<!--/STE-->

**繁中**：先定義「使用者怎麼用」，並在寫任何 Qt 程式之前做出可點擊的原型。畫面寫完才發現版面錯，修正成本高；原型幾分鐘就能暴露問題。

## 任務

1. **人物與主要任務**：AOI 配方工程師。三件事：教一個元件、修一個錯的框、快速驗證整批結果。
2. **使用流程**：開啟影像 → 選 Sample 工具 → 拖曳樣本 → 搜尋中（進度＋取消）→ 預覽提案 → 調整（分數門檻、刪除、補一個）→ 接受 → 匯出。
3. **線框圖（6 個狀態）**：空白、已載入影像、拖曳樣本中、搜尋中、預覽、無匹配/錯誤。版面依 `docs/00-master-plan.md` §4：左側工具列、中央畫布、右側物件與參數 dock、底部狀態列。
4. **視覺狀態語言**：Sample＝藍實線粗；Found＝綠實線；Inferred＝琥珀虛線；Rejected＝紅點線；選取＝加粗＋把手。文字對比 ≥ 4.5:1、圖形對比 ≥ 3:1（WCAG 2.1 AA）；**不只靠顏色，線型也要不同**。輸出 `docs/ux/tokens.json`（淺色/深色）。
5. **互動規則**：滾輪以游標為中心縮放、Space＋拖曳或中鍵平移、`F` 適配視窗、`Enter` 接受、`Esc` 取消/捨棄、`Del` 刪除、`Ctrl+Z/Y`。把手點擊區 ≥ 24 px。
6. **回饋時間規則**：> 200 ms 顯示忙碌；> 1 s 顯示進度＋取消；狀態列永遠顯示「back end · 耗時 ms · 數量」。
7. **文案**：空白、錯誤、無匹配的訊息（繁中＋英文，i18n key）。無匹配時要告訴使用者下一步（樣本太小？對比太低？降低分數門檻？）。
8. **主題與縮放**：淺色/深色、高 DPI、字級縮放。
9. **ADR**：`docs/adr/001-ui-toolkit.md`（Widgets＋QGraphicsView vs QML，含取捨）、`002-undo-model.md`。
10. **列出 S04 要實作的 Presenter 介面名稱**（只列名稱與責任，每個 ≤ 5 個方法，ISP）。
11. **與使用者確認** `perf/budgets.json` 的 UI 類預算（U1–U4、I1–I6）合不合理。

## SOLID 重點

這一步沒有程式碼；重點是**先把介面切窄**（任務 10），讓 S04 的實作自然符合 I 與 D。

## 效能檢查

不量測（沒有程式）。要交付的是：UI 預算（U/I 類）經使用者確認。

## 四種輸出（本步驟具體內容）

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | UX 原則的 Purpose/Reason/Effect（STE）＋繁中摘要；每條互動規則寫出「為什麼」 |
| 2 | diagram.md | 使用流程圖、互動狀態機、6 個狀態的線框圖（Mermaid 或 SVG） |
| 3 | explainer.html | **可點擊原型**：在合成板子上拖出樣本，真的會走過 6 個狀態；快捷鍵可用；可切換深色；可調 Found/Inferred 顏色與線型預覽 |
| 4 | storyboard.md | 60 秒「三個動作教會一個元件」 |

## 完成定義

- [ ] ADR-001 由使用者確認
- [ ] 使用者看過原型並給回饋，已納入
- [ ] `docs/ux/tokens.json` 存在且淺/深色對比數值已驗算
- [ ] UI 預算已確認
- [ ] `node tools/ste-lint.mjs --strict` 無 error

## Commit 範本

```
docs(ux): add UX spec, state language and clickable prototype

- User flow, 6 wireframe states, interaction and feedback-timing rules
- Visual state language tokens (light/dark, WCAG AA contrast)
- ADR-001 UI toolkit, ADR-002 undo model
- Clickable prototype, storyboard, UI performance budgets confirmed
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續 AOI 專案。請先讀 AGENTS.md、docs/current-status.md、docs/plan/S01-ux-spec.md，
再依 .agents/harness/project-rules.md 的 Step Protocol 完成步驟 S01（分支 step/01-ux-spec）。只做 S01。
完成後：commit、合併到 main、更新 docs/current-status.md 與交接單（ai-handoff skill）、對我提示 commit 內容，並用 spawn_task 為 S02 開新 session。
```
