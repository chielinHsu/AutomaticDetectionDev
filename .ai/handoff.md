# Agent Handoff

- 更新者：Claude Code (Claude Fable 5.1)
- 更新日期：2026-10-29（R10 實作完成，提交在分支上）
- 分支：`claude/r09-halcon-mask-hint-f0d064`（worktree `.claude/worktrees/r09-halcon-mask-hint-f0d064`，從 `main` 的 `37bd396` 切出；沒有 push、沒有合併）

## 本次工作與狀態

- [R10 status](../docs/plans/active/R10-halcon-mask-hint/status.md)：R10.1–R10.5 ✅，計畫狀態「進行中（待合併）」。HALCON 建好樣本、還沒設定比對區時，訊息列用卡片建議「用 M 點端子的遮罩當比對區」並說明原因（R07.8 的量測：整框召回 855／1068、鋸齒 0.70；遮罩 1059、鋸齒 0.48）；OpenCV 與假 back end 不提醒。設計：[ADR-042](../docs/adr/042-match-mask-advice.md)。
- 做法：`Backend::traits.suggestsMatchMask`（HALCON 的工廠填）→ `application/interaction/MatchAdvice`（Idle、類型的 match 是預設、沒按過〔關閉〕）→ `AutoRoiPresenter::publishMessage` 的 Idle 分支用 `MessageTexts::matchAdviceCard`（〔用 M 點端子〕選取樣本並切到 M 工具；〔關閉〕或 Esc 之後同類型不再提醒；新樣本再提醒；開新影像重來）。不擋搜尋。
- 四種輸出在 `docs/steps/R10-halcon-mask-hint/`；`perf.md` 說明本步驟無效能指標。

## 實際驗證

- 完整 `test.ps1`（Release＋OpenCV＋HALCON＋Rust，67 項）第一次 64／67、修好測試台後 `-NoBuild` 重跑 65／67；`tst_compare` 的 2 項是測試台順序造成，修好後重跑通過（`tst_compare` 11／11、`tst_presenter` 88 passed、1 skipped）；`tst_lattice` 2 項、`tst_generate_objects` 1 項是 `main` 已提交狀態本來就有的失敗（見下）。`check-deps` 0、`ste-lint` 0。真實程式 `--backend halcon --demo 5000` 看到卡片（截圖在 session 的共用筆記資料夾）。沒跑 Debug、`-DomainImpl Cpp`、`-Perf`。

## 未提交與不要動

- **主 checkout**（`D:\AOI_Project\Qt\AutoCadCreated`）有 Codex 的 R08（Socket 教學）大量未提交修改，以及 `tests/domain/tst_lattice.cpp`、`tests/core/tst_generate_objects.cpp` 的未提交修改（R07.5–R07.7 的回歸修復）。本 worktree 看不到這些；`main` 已提交的狀態 `tst_lattice`、`tst_generate_objects` 會失敗，要讓 `main` 自己通過得把那兩個測試檔提交。不要去動主 checkout。
- `docs/plans/INDEX.md`：本分支加了 R10 列、下一個可用編號 R11；主 checkout 的 INDEX 有 R08、R09（Codex 的深度學習 CAD）兩列，合併時會衝突，三列都留、編號 R11。2026-10-30 從 R09／ADR-036 改號成 R10／ADR-042（Codex 先用了 R09 與 ADR-036..041）。
- `%APPDATA%\AOI\AutoCadCreated.ini` 的 `backend/last` 因為實際執行程式變成 `halcon`。

## 待使用者決定

- 合併本分支回 `main`（建議 `--no-ff`）；合併後把 `docs/plans/active/R10-halcon-mask-hint/` 搬到 `completed/`、INDEX 改 ✅。
- R07 留下的另外兩件後續：通用樣本比對與 Socket 結構偵測（R08）並存或整合；右側 dock 重疊。都還沒開計畫。
