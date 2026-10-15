# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Opus 5.5)
- 日期：2026-10-15

## 正在做

**比對區與 CAD 框分開（ADR-026）完成實作與驗證，等使用者確認三件事**（見下面「等使用者決定」）。分支 `feat/match-region`（從 `main` eb95b79 開），在 worktree `.claude/worktrees/match-region`；已在分支上 commit（訊息見 `git log`），**沒有合併到 `main`、沒有 tag、沒有 push**。主 checkout 在 `main`。另一個 session 在 worktree `loving-blackburn-8ecbfc` 做 `feat/lattice-snapped-positions`（CAD 框與鄰近對齊），和這裡沒有共用檔案以外的衝突預期，但兩邊都改到 `docs/*` 的紀錄類文件，合併時要手動合。

## 這次做了什麼（細節在 `docs/changelog-ai.md` 最上面那一筆與 ADR-026）

- 範本只用樣本裡想要的特徵（主體框內的矩形或遮罩；灰階或邊緣），CAD 框不變。port 加 `buildFromSample`（有預設實作），契約：`Match.pose` 一律是主體中心；設定存在 `ComponentType::match`（µm、相對主體中心）。
- OpenCV：裁比對區、遮罩 NCC（inf／NaN 設 0）、邊緣＝Sobel 大小。HALCON：domain 是比對區、參考點＝重心（不用 `set_shape_model_origin`）、視窗換成參考點的視窗。fake 忽略比對區。
- 自動擷取在 domain（`feature_extract`）；`M` 工具（拖框＝矩形、點一下＝擷取）、選取資訊表單的「比對區」（特徵、外擴、清除）；Command `SetMatchRegion`。
- 配方 v3、樣本庫 v2（舊檔遷移、嚴格檢查遮罩）。

## 真實影像（`perf_match_region_real`，`AOI_REAL_MATCH_IMAGE=D:\Benano\20261014_4677_AdvancePin_LeftTopRegion_Proj\DBG\FovImages1\0_0_HA.tif`）

- 整框兩個 back end 召回 93.1%；比對區 99–100%。OpenCV 邊緣門檻 0.6：100%、誤報 0；HALCON 矩形門檻 0.7：99.0%、誤報 16。
- 位置：比對區的誤差幾乎都在 x（y 的 p95 0.25–0.34 px），兩個演算法彼此 p95 0.45–1.2 px；標準答案（Otsu 重心）在 x 上不穩，所以「p95 ≤ 3 px」量不到（4.0–4.3 px）。

## 等使用者決定（`docs/current-status.md` 的待決定（11））

1. 標準答案（1109 根，`build/qmake-release/test-work/real-match-truth.png`）目視確認；位置的準則要不要改用 y 與一致性，或做人工標記。
2. HALCON 的比對區沒有一個門檻同時達到召回 ≥ 99.5%、誤報 ≤ 5，要不要另開 session 調。
3. 合併到 `main`。

## 建置與測試（實際執行，見 changelog 的「驗證」）

- Release 完整（最後一次）：**58／58**（HALCON 沒有跳過；有跳過項目的 5 個測試與 2 個 XFAIL 和 main 相同）；先前一次 55／58（3 項計時，單獨重跑通過）。`-NoOpenCv`、`-NoHalcon` 完整通過。Debug 57／58（`aSlowSearchShows…` 偶發，main 也一樣：4／12 對 3／12）。
- `ste-lint` 0 errors、`check-deps` 0 violations、`check-contrast` 0 failures。`perf.ps1` 沒有跑。

## 不要動

- 整框的程式路徑要和之前逐位元相同（契約 `aDefaultSpecMatchesTheRectBuild`）。
- `ContractTolerances.h` 的數字（新列用推導的上限，見 ADR-004 的後續）；`recipe-v1/v2/v3.json`、`library-v1.json`。
- 上一次交接的清單照舊：`mergeSampleProposals`、`accept` 的整批換掉、`RoiLayerItem` 的字形路徑、兩個 back end 的搜尋選項預設、opencv B1 的 `QEXPECT_FAIL`、`tokens.json`、`copy.json` 只新增 key、`budgets.json` 的 limit、`perf/baseline/*.json` 只用 `-WriteBaseline` 重建（等使用者說）。

## 已知問題與風險

- 樣本角度一律 0；畫布上比對區用草稿框畫（遮罩畫外接矩形）；流程 B 不讀比對區；OpenCV 小範本的粗搜沒優化（遮罩 2–3 s、邊緣 1.5 s／5 MP）。
- 對稱的比對區（只框半邊）在 shape model 會一個元件找到兩次。
