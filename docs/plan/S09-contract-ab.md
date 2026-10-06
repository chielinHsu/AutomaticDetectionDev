# S09 — 契約測試與 A/B 比較

| 項目 | 內容 |
|---|---|
| 層級 | 底層架構（驗證互換性） |
| 分支 | `step/09-contract-ab` |
| 依賴 | S07、S08 |
| 產出位置 | `tests/contract/`、`src/presentation/compare/`、`docs/steps/S09-contract-ab/` |

> **S01 修訂（2026-10-05）**：契約測試要涵蓋 `ILibraryMatcher`：辨識正確率、候選排序、位姿誤差、庫大小 N 的行為（ADR-006）。

> **S03 完成後的提醒（2026-10-05）**：`tests/core/tst_fake_backend.cpp` 已經是「對假 back end 跑的契約測試」：把它的斷言改成以工廠參數化、對每個已編入的 back end 各跑一次（沒編入的標示 SKIPPED）。ADR-010 的角度慣例要用旋轉過的合成影像鎖住（目前只有 `tst_domain` 用數字鎖 domain 的數學）。

> **S06 完成後的提醒（2026-10-06）**：`IImageSource` 已經有 Liskov 的測試：`tests/infra/tst_image_sources_agree`（Qt 與 OpenCV，18 種檔案：16 種完全相同；已知例外：16 階灰色表的 BMP，`QEXPECT_FAIL`；灰階＋alpha 的 PNG 沒有驗證）。HALCON 加進來時把它改成以來源參數化。`IRecipeStore`、`ILibraryStore` 的契約測試在 `tests/infra/tst_recipe_store`、`tst_library_store`、`tst_library_layers`（只有 JSON 一個實作）。

<!--STE-->
## Purpose
This step proves that the two back ends are interchangeable. It also shows their differences on the screen.

## Reason
A common interface does not guarantee a common behavior. The coordinates, the angles and the scores can differ without a compile error.

## Effect
One test suite runs on each back end. The user can compare OpenCV and HALCON on the same sample.
<!--/STE-->

**繁中**：介面相同不代表行為相同。用同一組契約測試跑每個 back end（Liskov），並提供 A/B 對照畫面，讓差異看得見。

## 任務

1. **契約測試套件**（以 factory 參數化，對每個已編入的 back end 各跑一次）：
   - 座標原點與角度正負號慣例。
   - 分數範圍 0..1；結果依分數排序。
   - 空結果行為（找不到時回傳空集合，不是錯誤也不是當機）。
   - 搜尋區域限制確實有效（區域外不會有結果）。
   - 取消旗標在 `I4` 預算內生效。
   - 合成資料集上的容差（位置誤差、角度誤差、召回率）；**容差寫在一個地方並說明依據**。
2. **A/B 比較功能**：同一樣本同時跑兩個 back end；畫布疊圖（兩邊都找到＝綠、只有 A＝藍、只有 B＝橘；不只靠顏色，用不同標記形狀）；表格顯示各階段耗時與數量差異。
3. **差異紀錄（decision log）**：強度式 NCC 與邊緣式 shape model 的行為差異（對光照、反光、缺件的反應），記錄在 `docs/adr/004-backend-divergence.md`；哪些差異可接受、哪些是 bug。
4. **CI 友善**：沒有 HALCON 授權時契約測試跳過該 back end 並**明確標示 SKIPPED**，不是靜默通過。

## SOLID 重點

這一步就是 **L（Liskov）的驗證**；也驗證 O：兩個 back end 都沒有逼 application 層改動。

## 效能檢查（寫入 `perf.md`）

兩個 back end 在同一測試矩陣子集的並排：時間、準確度、峰值記憶體。A/B 疊圖的 UI 更新時間（U1 類）。

## 四種輸出（本步驟具體內容）

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | 契約的 Purpose/Reason/Effect；每條契約寫出「違反時會發生什麼壞事」 |
| 2 | diagram.md | 契約測試的參數化結構圖；A/B 疊圖的資料流 |
| 3 | explainer.html | 互動頁：載入 A/B 結果 JSON，切換顯示、點一個差異看原因與兩邊分數 |
| 4 | storyboard.md | 75 秒「兩個 back end 為什麼可以互換」 |

## 完成定義

- [ ] 兩個 back end 都通過契約測試（或差異已記入 ADR-004 並有明確容差）
- [ ] A/B 比較畫面可用
- [ ] 無授權環境的測試輸出為 SKIPPED，不是 PASSED
- [ ] `node tools/ste-lint.mjs --strict` 無 error

## Commit 範本

```
test(contract): add back-end contract suite and A/B compare view

- Parameterized contract tests: conventions, ordering, empty result, region limits, cancel, tolerances
- A/B overlay and timing table in the canvas
- ADR-004 documents accepted differences between NCC and shape-model back ends
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續 AOI 專案。請先讀 AGENTS.md、docs/current-status.md、docs/plan/S09-contract-ab.md，
再依 .agents/harness/project-rules.md 的 Step Protocol 完成步驟 S09（分支 step/09-contract-ab）。只做 S09。
完成後：commit、合併到 main、更新 docs/current-status.md 與交接單（ai-handoff skill）、對我提示 commit 內容，並用 spawn_task 為 S10 開新 session。
```
