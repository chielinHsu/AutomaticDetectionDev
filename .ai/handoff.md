# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Opus 5.5)
- 日期：2026-10-16

## 正在做

**`main`（1d3c137：晶格位置吸附 ADR-026、操作步驟 ADR-027）併進 `feat/match-region`，衝突已解、Release 驗證通過，合併還沒 commit**（`git merge --no-commit`，在 worktree `.claude/worktrees/match-region`；檔案已 stage）。下一步：使用者同意後 commit 這個合併，再決定要不要把分支 `--no-ff` 合併回 `main`。沒有 tag、沒有 push。

## 這次做了什麼

- **ADR 改號**：比對區在分支上原本是 ADR-026，和 `main` 的晶格位置吸附撞號，改為 **ADR-028**（檔案 `docs/adr/028-match-region-separate-from-cad-box.md`，開頭加了「編號」一行）。只改分支自己新增、提到比對區的那幾行（104 行，含程式註解）；`main` 的 ADR-026 引用（位置模式、晶格）都沒動。
- **程式碼衝突（4 個）**：兩邊各加欄位，都保留——`InspectionSession.h` 的 `SelectionInfo`（比對區＋晶格）、`ViewInterfaces.h` 的選取資訊（比對區＋`proposal`／晶格）、`AutoRoiPresenter.cpp` 填表單的兩段、`ObjectsPanel.ui`：比對區四列排在 `main` 新增的「位置」與〔套用到全部〕之後（列 9–12），`selectionNote` 移到列 13。
- **文件衝突**：`AI_CONTEXT.md`（測試數改 **59**／57／56／54：`tst_step_guide` 與 `tst_match_region` 各加一個；執行說明加回 `M`）、`architecture.md`、`decisions.md`（028 列在最上、引言說明改號）、`current-status.md`（階段、比對區一節、待決定（11））、`changelog-ai.md`（兩邊的紀錄都留）、`ux-spec.md` 修改紀錄的順序。

## 等使用者決定

1. Commit 這個合併（訊息照 `project-rules.md`）；之後要不要合併回 `main`。
2. 原本的兩件事照舊（`docs/current-status.md` 的待決定（11））：真實影像的標準答案與位置準則；HALCON 比對區的門檻。
3. 操作步驟的示範畫面（`docs/ux/guidance/*.png`，抓整個視窗）沒有 `M` 工具與比對區的表單列，要不要用 `record_guidance` 重產（ADR-027：版面改了要重跑再重新建置）。

## 建置與測試（2026-10-16，合併後實際執行）

- `scripts/dev/test.ps1`（Release＋OpenCV＋HALCON）：**59／59**，結束碼 0；HALCON 沒有跳過；有跳過項目的 5 個測試（`tst_backend_registry`、`tst_shell`、`tst_presenter`、`tst_opencv_image`、`tst_image_sources_agree`）與 2 個 XFAIL（opencv B1、palette BMP）和合併前相同。
- `ste-lint` 0 errors、`check-contrast` 0 failures（`check-deps` 在 `test.ps1` 裡通過）。
- **沒有跑**：`-Config Debug`、`-NoOpenCv`、`-NoHalcon`、`perf.ps1`。

## 不要動

- 整框的程式路徑要和之前逐位元相同（契約 `aDefaultSpecMatchesTheRectBuild`）；`ContractTolerances.h` 的數字；`recipe-v1/v2/v3.json`、`library-v1.json`。
- ADR-026（晶格）：`Candidate::pose` 留匹配位置；吸附只在 `classify`／`ProposalSession`。ADR-027：`publishGuidance` 只在狀態變了才碰 dock。
- `copy.json` 只新增 key、`budgets.json` 的 limit、`perf/baseline/*.json` 只用 `-WriteBaseline` 重建（等使用者說）。

## 已知問題與風險

- 比對區：樣本角度一律 0；畫布上用草稿框畫；流程 B 不讀比對區；OpenCV 小範本的粗搜沒優化；對稱的比對區在 shape model 會一個元件找到兩次。
- 合併後沒有在真實影像上跑過（`perf_match_region_real` 這次沒跑），比對區＋晶格吸附一起用的結果未驗證。
- 對時間敏感的測試：Debug 的 `tst_presenter::aSlowSearchShows…` 偶發失敗（main 也一樣）。
