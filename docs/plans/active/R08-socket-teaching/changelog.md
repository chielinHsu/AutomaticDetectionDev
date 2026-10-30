# 2026-10-29 — 區域 CAD 註冊（Codex / gpt-6.1-sol）

- 新增 ISocketRegionRecognizer、GenerateSocketCad、OpenCvSocketRegionRecognizer、SolderCadRegistration 與 SocketCadWorkflow；預設改為區域結構辨識，不使用 NCC。
- 共享 CAD 類型／子窗、固定規格、單框 copy-on-write、重註冊保留人工修改；19 項操作測試通過。真實圖 686 個候選，未宣稱 precision／recall。
- Release、Debug 無 CV／HALCON 及獨立執行包啟動通過；保留 R08.8 的表格性能修正。
- R07 近期新增一維弱候選測資間距不明確；只補足連續種子及間距斷言，不動演算法，單項由失敗轉 3/0/0。
- 執行包、CRC／SHA-256、畫面及 log 在 build/r08-region；後續已建立 R09 計畫。
- 不 stage／commit／push。

# R08 變更紀錄

## 2026-10-29 — Codex (gpt-6.1-sol)：R08.5–R08.7 樣本全圖搜尋版

- 依使用者明確修正，主 UI 改為 R 教多個焊點、F5 搜尋整張 Socket；亮塊控制隱藏，舊 API 只供相容測試。
- 新增 ISocketSampleSearcher、SearchSocketSamples；由 app 組裝既有 OpenCV builder/searcher，使用主體周邊特徵、相似度門檻及跨樣本 NMS；不改 R07 晶格或低分驗證規則。
- 新結果一律使用第一樣本尺寸。Sample 元件與 Recipe.params 保存樣本及門檻，沿用 JSON v3；重新開配方可重搜。
- 更新 SolderTeachingSession、視窗／Designer UI、接線、文案、qmake、CompositionRoot 與 --socket 入口，補測試及 ADR-034、現行四種輸出。
- 合成資料：明亮但形狀不同的物件不匹配；不同大小及重複樣本合併為 3 個固定尺寸結果。UI 教樣本後不立即搜尋，F5 才搜尋；存讀後樣本、門檻恢復，重搜不累加。
- 真實圖：3 個代表樣本（含右側反向外觀），632 個結果，全部 44×24；不是精確率／召回率。沒有用高度圖。
- 共享版本專項 15/0/0，含另一聊天 R08.8 的 1017／5000 框拖移測試；保留其 Interactive 欄寬與批次量寬修正，不替該聊天更改進度。
- 全量 67/67，原 7 組 skip 及 2 個 XFAIL 保留。Release 與 Debug 無 OpenCV／HALCON 成功；移除 Qt 開發 PATH 後執行包啟動通過。
- 建置曾遇既有 cache obj 的 OS 寫入權限，以及 Debug 自動再生 Makefile 尚未納入新物件；分別以正常 Windows 帳戶重建及 qmake_all→jom 修復，最終皆成功，未改 ACL。
- 本次沒有 stage、commit、push。證據在 build/r08-samples；操作與前後流程見 [sample-workflow.md](../../../steps/R08-socket-teaching/sample-workflow.md)。

## 2026-10-29 — Codex (gpt-6.1-sol)：回歸與 Socket 補驗

- 依使用者要求保留 R07 工作、修復四個未通過群組；工作記於 [R07.5–R07.7](../../completed/R07-multi-sample-merge/tasks/regression-repair.md)，本處只更新 R08 受影響的驗證結果。
- 全量回歸從先前 63/67 更新為 67/67，沒有測試執行失敗；仍有 7 組既有 skip 與 2 個已知 XFAIL，整體 PARTIALLY VERIFIED。
- Socket 重連結後專項 10 passed、0 failed、0 skipped；真實圖 28 框，滑鼠圈選、單框編輯、刪除／移動、取消、配方存讀與錯圖保護通過。UI 往返 82.4 ms 為單次 smoke。
- 未修改 Socket 來源或 R07 演算法；更新 plan/status 的過期驗證資訊，保留歷史結果。最新證據在 build/r09-repro；本次不 stage、commit 或 push。

## 2026-10-28 — Codex (gpt-6.1-sol)

- R08.1–R08.3 由規劃轉為完成；R08.4 為部分驗證，MVP 已交付但全量回歸尚未收斂。
- 新增 application 的 SolderTeachingSession 與 ISolderRegionDetector；OpenCV adapter 以閾值、連通區、面積與邊界規則產框。
- 新增 Qt Designer 教學視窗與背景工作接線；主程式加 --socket 及入口；沿用 ImageCanvas、IImageSource、RecipeSaver、JsonRecipeStore。
- 新增專項測試與 scripts/dev/build-socket-mvp.ps1；更新對應 qmake 清單、文案、ADR-033、架構列與四種輸出。
- 最終 Socket 測試：10 passed、0 failed、0 skipped。真實圖原始區域 x=1100,y=120,w=900,h=550，28 框；UI 往返 86.1 ms。
- 最終全量回歸：63/67；失敗：tst_generate_objects、tst_lattice、tst_presenter、tst_domain_rs_diff。完整 log 留在 build/qmake-r08/test-logs；未修改晶格演算法或放寬測試。
- 初次自己的測試有 ErrorCode 的 QCOMPARE／ADL 編譯衝突，已改為布林比較並通過。另有來源樹既有 moc 污染，只修隔離建置的相依，未移動或刪除來源樹產物。
- 初次完整回歸在 Rust 差異測試連結完成前啟動，該項未找到執行檔；補測 170 passed、0 failed、1 skipped（缺真實 CSV），之後重新連結並跑本次全量回歸。
- 執行包已部署 Qt 與 OpenCV，--help 成功、--socket 示範圖啟動成功，且 Qt 開發 bin 不在 PATH。
- 保留原有 R07 工作；本次 R08 沒有 commit／push。
