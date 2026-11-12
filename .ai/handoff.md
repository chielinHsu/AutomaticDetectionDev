# Agent Handoff

- 更新者：Codex (gpt-6.1-sol)，2026-11-12；目前 main。本輪依使用者「先合併回主線」完成 [R09](../docs/plans/active/R09-deep-learning-cad/status.md) R09.24；沒有 push，R09.7 正式品質仍未完成。

## 本輪本機整合

- R09.18–R09.23 共 30 個程式／測試／文件已提交 `b1670280803b1e8447429744f27b7d4e9edbcbd5`，來源分支 codex/r09-dependent-inspection。
- 以 no-ff 合入 main：`bf239a4a6bd1522b61a59aaa603d443e6f49ebba`；兩個 parent，沒有衝突，合併樹與來源分支完全相同。提交作者／committer 均 Caspar Hsu <chielin0025@hotmail.com>。
- 已包含前輪矩形／角點殘影、輪廓點位／完成／取消、草稿儲存保護，以及本體／導腳／附屬檢測窗的父子幾何、教框、共享、Undo、推論整組輸出與配方存讀。沒有開始新的功能。
- 更新 R09 plan 的 R09.18–R09.24 提交狀態、status 現在快照、changelog 與本交接；收尾文件另作本機 docs 提交，具體 hash 見 git log。R09 計畫仍進行中，INDEX 未改。

## 本輪驗證

- Python／Qt／ONNX 全套 59/59，197.318s，build/r09-main-merge-20261112/tests.log；包含兩模式真 ONNX fixture，不代表正式 PCB 模型品質。
- payload builder 成功（28 檔），獨立 build/r09-main-merge-20261112/payload；沒有操作或關閉使用者現有視窗，未重編 C++ launcher。
- check-deps 316 檔 0 違規；STE 1114 句 0 errors；結構檢查 OK（routing 68/68、10 個既有 architecture 路徑 warnings）；git diff --check 通過。來源 SHA／stage scope／保留目錄前後核對記錄在同一 ignored 目錄。
- 本輪 PARTIALLY VERIFIED：未重跑完整 C++／Rust、GPU、正式訓練、產線延遲或完整視窗高 DPI 人工驗收。前次 Debug Socket 大量框拖曳門檻失敗仍未修，不調門檻、不當成本輪通過。

## 使用與下一步

- 第 3 步選參考標註，可建雙端／八腳起始框，或畫本體／任意導腳／相依窗；樹清單選框修改局部座標，父框平移旋轉帶動子樹，尺寸不縮放子框。不同封裝使用不同類別。
- profile.inspection 與配方 inspection_boxes 保留父子關係；舊 v1 無群組仍用 window_ratio。資料匯出只教辨識元件，框組是規格套用，不是獨立導腳定位或焊接判定。軸向模 180°不保證 Pin 1／極性。
- 10/30 已更新 AiInspTool/bin/vision-workbench、來源比對與 native 兩模式啟動通過是當時證據。現有視窗不會熱載入，使用者須先完成／取消草稿並 Ctrl+S，再重啟；不要代為關閉未存檔視窗。
- R09.7 待足夠人工標註與獨立板子／批次，正式訓練及品質驗收。同板不同 Z／通道不可拆 train/val/test。合成 smoke 模型 mAP=0，不當作可用 PCB 模型。
- 獨立 IC 示範 build/r09-inspection/ic-demo-with-inspection.json、ic-related-*.png；不是使用者正式標註或模型預測。

## 不要動

- .archify/（142 檔）、AiInspTool/cargo/（16 檔）來源不明，不清理、不 stage；合併前後以雜湊核對保留。
- D:/BMP/PCB 原圖、使用者最新標註與現有程序；build/r09-shapes-backup/last-saved.json 是舊副本，不得覆蓋目前資料。
- R08／R10 及其他聊天室工作，已合併的 .claude/worktrees/r09-halcon-mask-hint-f0d064；本輪未改其來源／進度，刪除仍須使用者決定。
