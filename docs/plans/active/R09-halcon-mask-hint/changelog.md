# R09 變更紀錄

> 只新增，新紀錄放最上方；只記本計畫的事。每個 agent session 一筆。

## 2026-10-29 — Claude Code (Claude Fable 5.1)：建立計畫並完成 R09.1–R09.5

### 細項
- R09.1：`Backend::traits`（`BackendTraits::suggestsMatchMask`），`makeHalconBackend` 填 true；新增 `application/interaction/MatchAdvice`（`pending(session, traits)`：旗標 ∧ Idle ∧ 類型的 `match.isDefault()` ∧ 沒按過〔關閉〕；`dismiss` 記類型 id；`reset` 開新影像時清）；`tests/core/tst_match_advice` 9 項。
- R09.2：`ISearchEngineSink::useSearchEngine` 多 `traits` 參數（`BackendPresenter::traits()`、`AppPresenters` 啟動時 `setBackendTraits`）；`AutoRoiPresenter::publishMessage` 的 Idle 分支用 `matchAdviceCard` 顯示卡片，`CardAction::PickMatchFeature`（選取樣本、`toolRequested(Match)`）與 `CloseAdvice`（dismiss）；Esc 在 Idle 沒有選取時關掉卡片；切換 back end 後重新發布；`copy.json` 多 `ui.match.advice.title`／`body`／`samples`／`action.pick`。
- R09.3：測試台多 `maskhint`（「MaskHint (test)」，排在 `broken` 之前）；`tst_presenter` 多 2 項（建議的整個流程、Esc 與開新影像）、`screenshotsForReview` 多一張 `p7-match-advice`；`tst_messages` 多 1 項與 4 個 key；可用的 back end 數 4 → 5。
- R09.4：真實程式 `--backend halcon --demo 5000` 框一個樣本後看到卡片（PrintWindow 抓視窗、PostMessage 送 S 與拖曳；腳本在 session 的 scratchpad，不入版控）。
- R09.5：`docs/steps/R09-halcon-mask-hint/`（spec、diagram、explainer、storyboard、perf）、ADR-036、`decisions.md`、`architecture.md`、計畫文件、`.ai/handoff.md`。

### 修改的檔案
- src/application/backend/BackendRegistry.h、src/application/interaction/MatchAdvice.h／.cpp（新增）、src/infrastructure/backend_halcon/HalconBackend.cpp
- src/presentation/presenters/ControllerInterfaces.h、MessageTexts.h／.cpp、AutoRoiPresenter.h／.cpp、BackendPresenter.h／.cpp、AppPresenters.cpp
- docs/ux/copy.json、AiInspTool/src/application/application.pro、AiInspTool/tests/tests.pro、AiInspTool/tests/core/tst_match_advice.pro（新增）
- tests/core/tst_match_advice.cpp（新增）、tests/ui/PresenterRig.h、tests/ui/tst_presenter.cpp、tests/ui/tst_messages.cpp
- docs/adr/036-match-mask-advice.md（新增）、docs/decisions.md、docs/architecture.md、docs/steps/R09-halcon-mask-hint/（新增）、docs/plans/INDEX.md、docs/plans/active/R09-halcon-mask-hint/（新增）、.ai/handoff.md

### 驗證
- 完整 `test.ps1`（Release＋OpenCV＋HALCON＋Rust，含建置，67 項）：64／67。3 個失敗：`tst_compare` 2 項是本計畫測試台的順序（`maskhint` 插在 `broken` 後面讓 A/B 的預設配對變了），改到 `broken` 之前後 `tst_compare` 11／11、`tst_presenter` 88 passed、1 skipped（原本就有）重跑通過；`tst_lattice` 2 項與 `tst_generate_objects` 1 項和本計畫無關——`main` 已提交的狀態（`37bd396`）本來就失敗，主 checkout 靠 Codex 未提交的 `tests/domain/tst_lattice.cpp`、`tests/core/tst_generate_objects.cpp` 修改才通過（本分支沒動 domain、use case、Rust）。
- 提交前再跑一次 `test.ps1 -NoBuild`：65／67，只剩 `tst_lattice`、`tst_generate_objects`（`main` 本來就失敗）。
- 原本就有的 2 個 XFAIL（`tst_contract_end_to_end` OpenCV B1、`tst_image_sources_agree` palette BMP）不變；7 個測試有跳過項目（HALCON 可用，`tst_halcon_*` 沒有跳過）。
- `node tools/check-deps.mjs`：296 files、0 violations。`node tools/ste-lint.mjs`：0 errors、0 warnings。
- 真實程式：`AiInspTool.exe --backend halcon --demo 5000`，stderr 是 runtime 授權的提醒，框一個樣本後訊息列出現「HALCON：建議用遮罩當比對區」卡片、〔用 M 點端子〕〔關閉〕，狀態列「HALCON · runtime 授權」。沒有用真實影像試（觸發條件和影像無關）。
- `tst_presenter` 的 `switchingToTheRealHalconKeepsTheUiThreadResponsive` 在建置剛結束時失敗過一次（UI 執行緒停頓 345 ms），重跑 16 ms 通過：機器負載，和本計畫無關（記憶體的「對時間敏感的測試」）。
- 沒有跑 `-Config Debug`、`-DomainImpl Cpp`、`-Perf`。

### Commit
- 見本筆底下的補充（提交後填 hash）。

### 注意事項
- 不要把 `maskhint` 移到 `broken` 之後（`tst_compare` 的預設配對）或 `other` 之後（錯誤卡片的〔改用 Other (test)〕）。
- 卡片用的是 `IMessageView::showCard`，不是提醒卡片（`showNotice` 歸 `BackendPresenter`）；兩個不要混用。
- 不自動擷取遮罩（ADR-036 決定 5）；要自動就得先確定樣本中心在端子上。
