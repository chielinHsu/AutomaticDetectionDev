# S05 — Domain：晶格估計與補漏（純 C++）

| 項目 | 內容 |
|---|---|
| 層級 | 底層架構 |
| 分支 | `step/05-lattice` |
| 依賴 | S03 |
| 產出位置 | `src/domain/lattice/`、`tests/domain/`、`docs/steps/S05-lattice/` |

> **S01 修訂（2026-10-05）**：照 ADR-006：晶格推算屬於**元件層級**（板上元件的陣列）。框層級的矩陣（Pin 的列數／欄數／間距）是 `BoxGroup` 展開的簡單算術，不需要晶格估計。

> **S03 完成後的提醒（2026-10-05）**：S03 的暫代 `SimpleGrid`（`src/application/usecases/SimpleGrid.*`：`fitGrid`、`gridHoles`）就是這一步要取代的對象；use case 只透過這兩個函式用它。`tests/core/tst_grid.cpp` 是現成的規格，其中「間距差 20 倍會被當成一排」記錄的是暫代版的已知限制，S05 取代後應改成預期找得到第二個方向。實測：暫代版在約 4 900 點要 97–134 ms（`docs/steps/S03-core/perf.md`），T3 的目標是 1 000 點 ≤ 2 ms。座標與角度慣例見 ADR-010。

<!--STE-->
## Purpose
This step builds the array rules in pure C++. It finds the pitch, assigns grid indexes and fills the gaps.

## Reason
These rules are the same for both back ends. Plain code is easy to test with exact numbers.

## Effect
One tested code finds the missing parts for both back ends. We know its speed before any vision code exists.
<!--/STE-->

**繁中**：把「陣列規律」做成純 C++：推算 pitch、指派格點索引、補漏。兩個 back end 共用，且可以用精確數字測試。

## 任務

1. **`LatticeEstimator::fit(matches)`**
   - 最近鄰位移向量 → 以網格投票/群聚找出主要 (dx, dy) → 取兩個線性獨立的最短向量為 u、v。
   - 以四捨五入指派 (i, j) 索引；最小平方法精修（每個座標軸 3 個參數：o、u、v，與 S00 示範頁相同）。
   - 以 RANSAC 排除離群點；支援一維（共線）陣列；信心不足時回傳「無晶格」（呼叫端退回一般 NMS 結果）。
   - 回傳 `LatticeModel{o, u, v, rms, coverage, confidence}`。
2. **多組晶格**：依基底向量群聚，回傳多個模型（例如上下兩排方向不同）。
3. **`Predictor`**：在影像範圍內預測所有格點；提供「由近到遠」的走訪順序供漸進驗證。
4. **漸進重新擬合**：用正規方程累加器，**每新增一點 O(1) 更新**，不必每次重算全部。
5. **`NonMaxSuppression`**：空間雜湊 O(n)；旋轉矩形重疊容差。
6. **信心分數**：殘差 RMS、涵蓋率 → 對應 UI 的「建議檢查」提示。
7. **測試**：可重現的 PRNG；掃描雜訊、缺件比例、傾斜角、離群點；退化情況（1 點、2 點共線、全部重疊）；與 S00 示範頁的演算法交叉驗證（匯出同一份 JSON fixture）。
8. **微基準 T3**（1 000 點擬合）與 NMS。

## SOLID 重點

- **S**：估計、預測、NMS 各一個類別。
- **O**：群聚方法以策略物件注入，可替換。
- **無依賴**：不得 include Qt／OpenCV／HALCON（`check-deps` 會擋）。

## 效能檢查（寫入 `perf.md`）

T3；NMS 在 1 000／10 000 點的時間；漸進重新擬合 vs 每次全量重算的比較（證明 O(1) 更新值得）。

## 四種輸出（本步驟具體內容）

| # | 輸出 | 內容 |
|---|---|---|
| 1 | spec.md | 演算法的 Purpose/Reason/Effect；列出**已知會失敗的情況**與對策 |
| 2 | diagram.md | 擬合流程圖；資料結構圖；退化情況決策樹 |
| 3 | explainer.html | 互動頁：載入單元測試匯出的真實 C++ 結果 JSON，拖雜訊/缺件/傾斜，看擬合如何成功或失敗（延伸 S00 的示範，但資料來自 C++） |
| 4 | storyboard.md | 75 秒「為什麼找到幾個就能推出全部」 |

## 完成定義

- [ ] 所有情境測試通過，退化情況不當機
- [ ] T3 已量測；與預算比對
- [ ] 與示範頁的交叉驗證通過
- [ ] `node tools/ste-lint.mjs --strict` 無 error

## Commit 範本

```
feat(core): add lattice estimation, gap filling and non-max suppression

- Robust lattice fit (LS + RANSAC), 1-D arrays, multiple lattices
- Incremental refit with O(1) update per point
- O(n) spatial-hash NMS, confidence score
- Scenario and degenerate-case tests; T3 micro-benchmark in perf.md
```

## Start prompt（開新 session 時貼上）

```
你正在 D:\AOI_Project\Qt\AutoCadCreated 繼續 AOI 專案。請先讀 AGENTS.md、docs/current-status.md、docs/plan/S05-lattice.md，
再依 .agents/harness/project-rules.md 的 Step Protocol 完成步驟 S05（分支 step/05-lattice）。只做 S05。
完成後：commit、合併到 main、更新 docs/current-status.md 與交接單（ai-handoff skill）、對我提示 commit 內容，並用 spawn_task 為 S06 開新 session。
```
