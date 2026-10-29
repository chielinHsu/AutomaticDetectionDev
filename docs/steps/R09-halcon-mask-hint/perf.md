# R09 — 效能量測紀錄

方法見 [docs/perf-test-plan.md](../../perf-test-plan.md)。

## 結論

本步驟**無效能指標**。原因：

- 規則 `MatchAdvice::pending` 只在 `publishMessage`（狀態變化時）跑一次，對板上的樣本做一次線性掃描（樣本通常個位數），
  每個樣本查一次類型（`findType`）；沒有進任何預算的路徑（U3 門檻拖動留在 Preview，不經過 Idle 的分支；I1–I6 的操作與之前相同）。
- 搜尋器、use case、domain、Rust 沒有改，`perf_matrix`、`perf_e2e`、`perf_ab` 量的程式路徑一個節點都沒變。
- 卡片的顯示用既有的 `MessageBar::showCard`（和預覽、無匹配的卡片同一條路）。

沒有跑 `test.ps1 -Perf`（未量測）；若要確認，`scripts/dev/perf.ps1 -Filter perf_ui_s04` 前後交替各跑兩次即可。

## 機器指紋

和 R07 相同的機器（`perf/baseline/casparhsu1-harness.json`）；本步驟沒有新的量測，不另列。
