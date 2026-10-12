# Agent Handoff

> 覆寫式交接單：每次收工整份重寫，不累積歷史（歷史在 `docs/changelog-ai.md`）。保持在 60 行內。
> 流程見 `ai-handoff` skill。

- 最後一位 agent：Claude Code (Claude Fable 5.1)
- 日期：2026-10-12

## 正在做

無。**B1–B3 的回歸測試完成**（分支 `test/halcon-accuracy-regression`，`--no-ff` 合併到 `main`，沒有 tag、沒有 push）。

**接下來的順序（使用者 2026-10-12 決定）**：（1）~~B1–B3 的回歸測試~~ 完成 →（2）**S10**（這個 session 收工時用 `spawn_task` 開；`docs/plan/S10-perf-harness.md` 的提醒第 6、7 點）→（3）**HALCON 準確度的修正**（S10 收工時開，分支 `fix/halcon-accuracy`，Start prompt 在 `docs/plan/S11-optimize.md` 開頭；現在要處理的是 B1–B3 **加上 O1**）→（4）S11。

## 這次做了什麼

- `tests/contract/tst_contract_end_to_end.cpp`：`syntheticBoards` 多三列（ADR-004 §5）。B1 低對比、B2 重模糊用 `perf_ab` 的 5 MP 板子（複製成 `abBoardSpec()`，原本的 1280×960 板子重現不了），B3 雜訊 12＋模糊 2 用原本的板子。`evaluate()` 多了「重複」的計數（每個有元件的格子最多一個有分數的候選），套在所有列，既有的 16 列照樣通過。
- **XFAIL 4 列**（`QEXPECT_FAIL`，Abort，放在第一個會失敗的檢查之前；修好時 XPASS＝失敗，要拿掉）：HALCON B1「全部找到」（841 個只找到樣本）、HALCON B2「沒有誤判」（242 個誤判、570 個重複）、HALCON B3「位置 ≤ 0.5 px」（最大 0.709）、**OpenCV B1「角度」（O1：最大 1.200°，上限 0.6°；加測試時發現，使用者決定一樣標 XFAIL）**。反向確認：拿掉 B3 的標記那一列真的 FAIL。
- `scripts/dev/test.ps1`：最後列出「有預期失敗（XFAIL）項目的測試」與列名（讀 log 的 `XFAIL` 行；UTF-8 with BOM）。它也讓既有的一個 XFAIL 浮出來：`tst_image_sources_agree` 的 `bothSourcesGiveTheSameImage(palette BMP, 16 grays)`（S06 就有，沒有動）。
- 文件：ADR-004 §5（回歸測試、O1、探測時看到的現象）、AI_CONTEXT（XFAIL 的意思、HALCON dongle 的 5 分鐘限制）、current-status、decisions、S10／S11 計畫的提醒與 Start prompt。
- 沒有改 `src/`、`ContractTolerances.h`、兩個 back end 的預設；測試數不變（56／54／53／51）。

## 建置與測試

- `test.ps1`：預設 **56 項**（`-NoHalcon` 54、`-NoOpenCv` 53、兩個都關 51）。
- XFAIL 不是通過也不是失敗：`test.ps1` 最後會列出來，回報時要寫；新增一列 XFAIL 要有使用者的決定。

## 不要動

- `ai-project-template/`；`docs/ux/tokens.json`；`copy.json` 只新增 key；`perf/budgets.json` 的 limit；`knownOver("U3")` 等 S11。
- `third_party/halcon/`；`tests/infra/fixtures/*.json`；`AOI_SYNTHETIC_REFERENCE_HASH`；`PresenterRig.h` 的視窗閘門；晶格的門檻與 ADR-013；port 契約的語意、`ImageView`、`SearchParams`；`OpenCvSearchOptions`、`HalconSearchOptions` 的預設值（修正的 session 要改時先把證據給使用者）。
- `tst_contract_end_to_end` 的四個 `QEXPECT_FAIL`：只有修好那一列才拿掉；不要為了讓它通過而放寬容差。
- `AiInspTool/main.cpp`、`mainwindow.*`；`aoi_target.pri` 的 `/IGNORE:4199`；表單的文字不要改成可翻譯（ADR-022）。

## 已知問題與風險

- **HALCON dongle 的 5 分鐘限制（2026-10-12 查到）**：重插後 CodeMeter 偵測到 Box Event，把 dongle 限制 5 分鐘（`%ProgramData%\CodeMeter\Logs`）；之前四次「重插沒用、過一陣子自己好」很可能就是它。**讀不到時先等 5 分鐘，不要一直重插**；判斷用 `test.ps1 -Filter 'tst_halcon_backend'`（0 skipped）或 `cmu --list`。使用者今天裝了 CodeMeter Runtime 9.10a，HALCON 照樣能用。`SESSIONS=1`，跑 HALCON 的量測時別同時開其他用 HALCON 的程式。
- 探測時看到（ADR-004 §5，沒有加測試）：低對比的 5 MP 板子**沒有雜訊**時，HALCON 與 OpenCV 都會在半個格距多出候選。修正 B2 時一起看。
- 等使用者決定（ADR-004）：分數門檻要不要依 back end 分開、`nativeGray16` 要不要當預設。
- 真實影像不在 `D:\Benano\20260922_*` 了；`D:\Benano\20261007_4677_AdvancePin_Proj\` 有新的，能不能用要問使用者。
- AB-U3 p95 48.8–63.4 ms，沒有預算（S10、S11）。UI 效能量測受機器負載影響很大，比較時要前後交替量。

## 驗證（實際執行）

- `tst_halcon_backend` 0 skipped（dongle 恢復後）；`test.ps1` 預設 Release **56／56**（HALCON 0 跳過；`tst_contract_end_to_end` 4 列 XFAIL、OpenCV 的 B2、B3 PASS）；`-NoHalcon` **54／54**（HALCON 的列 SKIPPED）；`-Config Debug` **56／56**（HALCON 0 跳過、XFAIL 相同）；反向確認 1 次；`node tools/ste-lint.mjs --strict` 0 errors。
