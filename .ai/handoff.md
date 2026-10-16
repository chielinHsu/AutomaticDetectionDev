# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Fable 5.1)
- 日期：2026-10-15

## 正在做

無。**CAD 框吸附到陣列的格點（[ADR-026](../docs/adr/026-lattice-snapped-positions.md)）完成**：分支 `feat/lattice-snapped-positions`（從 `main` 97495d3 開；兩個 commit：程式與測試、文件），這個 session 在 worktree 執行，`main` 在主 checkout（`D:\AOI_Project\Qt\AutoCadCreated`）上簽出，所以用 `git -C <主 checkout> merge --no-ff` 合併（合併前確認主 checkout 乾淨），沒有 tag、沒有 push。**「操作步驟提示與影片操作提示」（使用者 2026-10-15 的第二個需求；ADR 用 027）在另一個 session 做到一半就停了**（2026-10-16 額度用完）：worktree `.claude/worktrees/beautiful-wilbur-02b592`、分支 `feat/step-guidance`，**37 個檔案的修改都還沒 commit**（含 `aoi_deps.pri`、`.pro`、`aoi_ux.qrc`、`docs/architecture.md`、`.ai/handoff.md`）。接手時以本分支合併後的 `main` 為準，`.ai/handoff.md`、`docs/architecture.md`、`docs/changelog-ai.md`、`copy.json` 會衝突。**接下來仍是 S12**（Start prompt 在 `docs/plan/S12-release.md` 底部），或使用者指定的 P2。

## 這次做了什麼（細節在 `docs/changelog-ai.md` 最上面那一筆、ADR-026、`docs/current-status.md` 的「CAD 框吸附到陣列的格點」）

- 使用者回報「CAD 框大小一致，但與鄰近 CAD 框沒有對齊；CAD 框的本質是料件的理論位置」。確認：`Seed`、`Verified` 是 back end 各自的匹配位置，只有 `Predicted` 在格點上；契約測試與 T6b 量的是匹配位置。三個做法給使用者比較，使用者選建議的做法。
- 兩個位姿並存：`Candidate::pose` 不變，新增 `latticePose`、`latticeDeviationPx`（use case 第 6 階段用最終基底填：成員拿格點、非成員只記偏差）；`PositionMode`（`Lattice` 預設／`Matched`）在 `classify`、`ProposalSession`、`InspectionSession::setPositionMode`（像門檻：不進堆疊、`dropRedo`、復原回到當時的模式）；接受存的就是顯示的位姿。
- presentation：參數面板「位置」下拉、選取資訊表單「位置」列、預覽卡片「N 個不在陣列的格點上」；`copy.json` 新增 13 個 key。

## 建置與測試

- `test.ps1` 預設 Release：**56／57**，唯一失敗 `tst_cli` 的兩項（`halconStartsWhenLicensedAndFailsClearlyOtherwise`、`severalProgramsCanUseHalconAtTheSameTime`：等 8／6 秒讀 stderr，機器忙時程式還沒印出授權提醒）；單獨重跑 `-NoBuild -Filter 'tst_cli'` **通過**（17／17）。和改之前相同：5 個測試有跳過項目（HALCON 0 跳過）、2 個 XFAIL（opencv B1、`tst_image_sources_agree`）。
- `test.ps1 -Config Debug`：**55／57**。`tst_cli` 同樣的兩項單獨重跑**通過**；`tst_presenter` 只有 `aSlowSearchShowsBusyThenProgressAndEscCancelsToIdleAtOnce` 一項失敗（82 passed、1 skipped），**單獨重跑四次都一樣**——當時另一個 session 的建置與效能量測讓機器滿載（整組 281–292 秒，正常約 35 秒）。量過按〔搜尋〕到事件迴圈之間只有 22–24 ms；忙碌（40 ms）與進度（150 ms）兩個計時器在 UI 執行緒被餓到時同一輪一起觸發，測試只看到 Progress。同一項在 Release 單獨跑通過。**2026-10-16 機器安靜時（負載 6%）重跑 `test.ps1 -Config Debug -NoBuild -Filter 'tst_presenter'`：通過**（83 passed、1 skipped ＝ 截圖，134 秒），所以 Debug 的三個失敗都是機器滿載的計時問題。
- `node tools/ste-lint.mjs`：979 sentences、0 errors；`node tools/check-deps.mjs`：220 files、0 violations。
- `-Perf` 與基準線**沒有跑**。上一個 session（2026-10-15，`main` 的 2042435）跑過一次 `perf.ps1 -Runs 3 -WriteBaseline`，結果 INCONCLUSIVE（352／623 個指標變慢，含沒動過的 domain；VS、Chrome、遠端桌面在跑），寫出的基準線已還原、沒有採用；**使用者指示：基準線的重建等他說了再做**（他會先關掉 VS 與 Chrome）。這次 use case 多了第 6 階段（O(n)）、`classify` 多一個判斷，重建時要看 T5、U3。

## 不要動

- `Candidate::pose` 要留匹配位置（契約、A/B、T6b 看它）；吸附只在 `classify`／`ProposalSession`。
- 吸附的門檻 ＝ 晶格的內點容差（`LatticeOptions::inlierFraction`），不另外加數字；非內點保留匹配位置。
- 位置模式不是修改（不進堆疊）；切換後 `dropRedo`；復原回到預覽時模式跟著回來（`tst_session` 鎖住）。
- 樣本不自動移動（`accept` 不碰 Manual）；跨樣本不對齊是已知限制（ADR-026 §7）。
- 上一次交接的清單照舊：`SampleListCache`／`DocumentSceneCache` 的鍵、表單「Enter 或離開欄位才生效」、`ResizeTypes` 的規則、`mergeSampleProposals`、`accept` 整批換掉、`RoiLayerItem` 字形路徑、`HalconSearchOptions`／`OpenCvSearchOptions` 的預設、`ContractTolerances.h`、opencv B1 的 `QEXPECT_FAIL`、`tokens.json`、`copy.json` 只新增 key、`budgets.json` 的 limit、`ai-project-template/`、`third_party/halcon/`、fixture JSON、`PresenterRig.h` 的視窗閘門、晶格的門檻與 ADR-013、port 契約、`perf/baseline/*.json` 只用 `-WriteBaseline` 重建。

## 已知問題與風險

- 樣本自己的偏差沒有顯示（樣本不在合併後的提案裡）；位置模式不存檔；畫布沒有用顏色標偏離陣列的；`IParamsSink` 變成 6 個方法。
- `tst_cli` 的 HALCON 兩項對時間敏感（機器忙時可能失敗，重跑即過）。
- 等使用者決定的事沒有變（ADR-004、S09、S10、待決定（10））。

## 驗證（實際執行）

- `test.ps1`（Release）56／57 ＋ `tst_cli` 單獨重跑通過；Debug 55／57 ＋ `tst_cli` 重跑通過 ＋ `tst_presenter` 在安靜的機器上重跑通過；`ste-lint` 0 errors、`check-deps` 0 violations；`-Perf` 沒有跑。
