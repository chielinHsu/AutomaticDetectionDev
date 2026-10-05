# AI 工作紀錄

> 只新增，新紀錄放最上方。每個 agent session 一筆，讓下一位 agent 知道上一位做過什麼、為什麼。
> agent 欄位寫實際的工具與模型；不要從舊紀錄抄。

## 格式

```markdown
## YYYY-MM-DD — Claude Code (模型) ／ Codex (模型)

### 完成
- 做了什麼（一行一件）

### 修改的檔案
- path/to/file

### 驗證
- 實際執行的指令與結果；沒執行就寫「未驗證」與原因

### 未完成
- 還沒做完的事

### 注意事項
- 下一位 agent 需要知道的限制、否決的做法、不要還原的修改
```

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)（S02：Qt UI 骨架與影像畫布）

### 完成
- 依 Step Protocol 做 S02：CMake＋presets、`src/` 三層（domain 只有 `ImageView`）、主視窗與 dock 空殼、影像畫布（游標中心縮放、平移、適配、零複製）、批次繪製層與單一物件 Item、四個工具與註冊表、暫代 controller、固定高度的訊息列、tokens → QPalette／QSS、文案直接讀 `copy.json`、`FakeScene`、5 個 Qt Test 單元測試執行檔、效能基準線，以及四種輸出與 `perf.md`。
- 第一件事（工具鏈驗證）：VS 2022 17.14／`cl` 19.44、Qt 6.8.0、CMake 3.30.5、Ninja 1.12.1 實際 configure、build、執行；寫回 `AI_CONTEXT.md` 環境表。發現 HALCON 只有 runtime（沒有標頭與程式庫）、OpenCV 的 `OpenCV_DIR` 要指到 `build\x64\vc16\lib`。
- 效能：第一版（描邊）I1 p95 約 50 ms，量測找到瓶頸是描邊的光柵化，改成整數像素的填色細矩形（純 QPainter 微基準約快 4 倍）。U1a、U1b、U4、I2、I5 通過；I1 在預算邊緣（offscreen 13 次執行 p95 14.1–20.7 ms）、真實視窗 22.1 ms 超標。預算數字沒改。
- 更正原本的假設：批次繪製**不是** 5 000 框變順的主因（Item 不多時對平移縮放幀沒有幫助），價值在第一幀與 Item 很多時的 p95。
- 測試抓到：Rejected 點線原本完全畫不出來（Qt 不畫長度 0 的虛線段）；變異測試確認三個關鍵行為的測試有效。
- 發現並處理兩個環境問題：效能量測會被混合架構 CPU 的核心與電源狀態影響 2 倍（`MeasurementHygiene.h`）；繁中 Windows 下字碼頁不一致會讓 Ninja 掉標頭相依（`env.ps1` 對齊字碼頁）。
- 新增 ADR-007（文案來源）、ADR-008（訊息列固定高度）、ADR-009（待使用者決定：S02 實測後重審 UI 工具集）；ADR-001 補了實測結果。

### 修改的檔案
- 新增：`CMakeLists.txt`、`CMakePresets.json`、`cmake/`、`src/`、`tests/ui/`、`scripts/dev/`（`env.ps1`、`test.ps1`）、`perf/baseline/casparhsu1.json`、`docs/steps/S02-ui-shell/`、`docs/adr/007`、`008`、`009`
- 修改：`AI_CONTEXT.md`（環境表、目錄、建置與測試指令）、`README.md`、`docs/architecture.md`（模組表）、`docs/decisions.md`、`docs/adr/001-ui-toolkit.md`（補充）、`docs/current-status.md`、`.ai/handoff.md`、`docs/ux/tokens.json`（`sizes.messageBarHeight`）、`docs/ux/copy.json`（新增殼層 key）、`docs/ux/ux-spec.md`（修改紀錄）、`perf/budgets.json`（校準狀態與實測值，limit 沒改）

### 驗證
- `scripts/dev/test.ps1`：Release 與 Debug 各 5 個測試執行檔全過；變異測試（縮放錨點、LOD 門檻、框夾限）都會被抓到。
- `node tools/ste-lint.mjs --strict`：434 句 0 錯誤；`node tools/check-contrast.mjs`：全過；`diagram.md` 4 張 Mermaid 用 mermaid 11 解析器全過（只驗語法）；`scripts/ai/check-ai-structure.ps1`：OK。
- 效能基準線：offscreen 全套（量測衛生開啟，多次執行）、真實視窗子集、純 QPainter 微基準，結果在 `docs/steps/S02-ui-shell/perf.md` 與 `perf/baseline/casparhsu1.json`。
- `-DAOI_WITH_HALCON=ON`：缺標頭時 configure 明確失敗；`-DAOI_WITH_OPENCV=ON`：找到 4.9.0（只 configure，沒有連結）。

### 未完成
- 等使用者決定 ADR-009（I1 與 UI 工具集）。
- 拖曳移動元件、方向鍵微調、設定記憶、減少動態、高 DPI 螢幕實測：沒有做。U2、U3、I3、I4、I6：S04。
- 暫代品：`ShellController`（S04）、`ImageLoader`（S06）、`ui.shell.*` 文案（S04）。

### 注意事項
- 一定要在 `scripts/dev/env.ps1` 之後 configure 與 build；Bash heredoc 會壓縮連續反斜線（寫含反斜線的內容用 Write／Edit）。
- 效能數字只在同一台機器、同樣的量測衛生下可比較；同一份程式不同時間會飄 ±30%，回歸閘門（S10）要考慮。
- `docs/ux/tokens.json`、`copy.json` 改了要重建（內嵌進執行檔）。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)（S01：UX 規格與可點擊原型）

### 完成
- 依 Step Protocol 做 S01（設計步驟，沒有 Qt 程式）：UX 規格、狀態語言 token（含顏色對比驗算工具）、文案（繁中＋英文）、ADR-001／002／006、可點擊原型，以及四種輸出與 perf.md。
- 使用者看原型後把範圍擴大：加入樣本庫與區域掃描（流程 B）、Board › Component › Box 階層、元件類型編輯器（矩陣、相依窗、套用範圍）、CAD 清單。對照了使用者提供的三份 TRI 文件。
- 使用者回饋「訊息蓋住料件」：所有要閱讀的訊息改放畫布下方的訊息列。
- 使用者決定庫與板子是複本關係、庫存 µm；原型隨之改成複本語意（〔從樣本庫更新〕、〔存入樣本庫〕）。
- 使用者找不到「存入樣本庫」的按鈕（原本只有 Enter 鍵、按鈕藏在 dock 下方）：預覽時在訊息列加上〔接受〕〔接受並存入樣本庫…〕〔捨棄〕，並修正說明文字的順序。
- 使用者問「樣本庫很多不是要比很久」：新增「比對範圍」（先比板子已有的類型，找不到再擴大）與狀態列「比對 N 個樣本」；預篩選等寫入 ADR-006 §7，未量測。

### 修改的檔案
- 新增：`docs/ux/`（ux-spec.md、tokens.json、copy.json）、`docs/adr/001-ui-toolkit.md`、`002-undo-model.md`、`006-sample-library-and-hierarchy.md`、`docs/steps/S01-ux-spec/`（spec.md、diagram.md、explainer.html、storyboard.md、perf.md）、`tools/check-contrast.mjs`
- 修改：`docs/decisions.md`、`docs/current-status.md`、`.ai/handoff.md`、`AI_CONTEXT.md`、`docs/00-master-plan.md`、`perf/budgets.json`（記錄 UI 預算確認與三點定義）、`docs/plan/S02`–`S10`、`S12`（開頭各加一行「S01 修訂」）

### 驗證
- `node tools/ste-lint.mjs --strict`：386 句，0 錯誤，0 警告；負向測試（長的被動句）會失敗。
- `node tools/check-contrast.mjs`：淺色 18、深色 18、疊圖 24 組全過；負向測試（壞的 token）會失敗。
- `diagram.md` 的 12 張 Mermaid 圖用 mermaid 11 的解析器（jsdom）解析全過，只驗語法，沒有渲染；解析器對壞語法會失敗。
- 原型頁內腳本，對最終版：整套 124 項斷言全過，另有比對範圍約 20 項，主控台無錯誤。涵蓋流程 A／B／C、三個範圍的復原、複本語意、11 種原因中的 9 種無匹配、三種錯誤、忙碌 200 ms／進度 1 s 的時機、把手 24 px 點擊區正反面、CAD 清單、訊息不蓋在影像上、淺深色、繁中／英文、無非法轉移。
- 建置、單元測試、`check-deps`：不適用（尚未建立）。效能：全部未量測。

### 未完成
- ADR-006 未決項目（見 `docs/current-status.md`）。
- S02 由新的 session 做（Start prompt 在 `docs/plan/S02-ui-shell.md` 底部）。

### 注意事項
- 過程中抓到並修正的真實缺陷：接受後復原再重做會先重做殘留動作；畫面快取鍵漏看群組內容，編輯樣本庫後「與樣本庫不同」的提示不會出現；框到多個元件時的提示是錯的（舊提示說「沒框到元件」）。
- 測試失敗多半是測試腳本自己的錯（期望值、順序、沒先選取群組），已改正重測；報告裡的數字是改正後對最終版的結果。
- 真實滑鼠／鍵盤／滾輪的抽查只在第一版原型做過；新版是合成事件加截圖。
- `main` 已在另一個工作樹 checkout，合併要在那邊做。
- 使用者的 S01 提示寫「commit 範本在 plan 檔、用 `-m`」，與 `project-rules.md` 的格式規則衝突；問過一次，使用者沒回答格式，只說 commit。我選了 `project-rules.md` 的格式（`[Caspar]docs:`、`git commit -F` 讀檔），因為那是使用者稍早確認的；不合要求可以 amend（沒有 push）。
- 合併是在 `main` 的工作樹（`D:/AOI_Project/Qt/AutoCadCreated`）用 `git -C` 做的，因為 `main` 在那邊已 checkout；做之前確認它是乾淨的。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)

### 完成
- 依使用者決定，把 `ai-project-template/`（共用結構範本、套用腳本、設計說明，共 18 個檔案）納入版控，放在這個 repo 內。這個範本是同一個 session 依使用者「設計共用結構」的要求產生的，並非產品計畫的一部分。
- 更新文件中「未追蹤、不要 stage」的說法：`AI_CONTEXT.md`、`project-rules.md`、`current-status.md`、`handoff.md`。`current-status.md` 原本寫「非本計畫產生」，造成誰做的、為什麼做的看不出來，改成寫明來源。

### 修改的檔案
- 新增：`ai-project-template/`（18 個檔案）
- 修改：`AI_CONTEXT.md`、`.agents/harness/project-rules.md`、`docs/current-status.md`、`.ai/handoff.md`、`docs/changelog-ai.md`

### 驗證
- `git add -n` 預覽：18 個檔案，沒有被 ignore 的內容；掃描沒有密鑰或機器路徑（只有規則裡拿 `HQ\Caspar.Hsu` 當作者名字的範例）。
- `scripts/ai/check-ai-structure.ps1 -Strict`：通過，0 警告。
- `node tools/ste-lint.mjs`：206 句，0 錯誤（它只掃 `docs/`，不含範本）。
- 用 repo 內這份範本重新建立範例專案：建置腳本結束碼 0，結構檢查通過。

### 未完成
- 已合併的分支（`chore/ai-agent-structure`、`chore/commit-format`）與備份分支 `backup/before-commit-reformat` 還沒清。

### 注意事項
- 範本放在產品 repo 內但與產品無關。它底下的 `template/CLAUDE.md`、`template/AGENTS.md`、`template/.claude/settings.json` 是給「新專案」用的檔案，不是這個 repo 根目錄的設定；agent 在這個 repo 工作時，以根目錄的 `AGENTS.md`、`CLAUDE.md` 為準。
- 若日後要把範本搬離這個 repo，同步更新 `AI_CONTEXT.md` 等引用它的文件。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)

### 完成
- 把前三個 commit 的訊息改寫成使用者的 `[作者]屬性:` 格式（檔案內容、作者、時間不變；新編號 `fd4fd2a`、`13230f8`、`74272a2`）。改寫前的完整歷史留在分支 `backup/before-commit-reformat`。
- `project-rules.md` 的 Commit 規則由 Conventional Commits 改為該格式，並訂為唯一權威；Step Protocol 第 5 步同步調整。訊息一律寫成檔案再 `git commit -F`，`AI_CONTEXT.md` 的 shell 備註同步修改。
- ADR-000 補上這次格式變更的說明。
- 依使用者要求以 `--no-ff` 合併到 `main`，沒有 push。

### 修改的檔案
- `.agents/harness/project-rules.md`、`AI_CONTEXT.md`、`docs/adr/000-ai-agent-structure.md`、`docs/changelog-ai.md`、`.ai/handoff.md`、`docs/current-status.md`
- 範本 `ai-project-template/`（未追蹤，不在 commit 內）：`project-rules.md` 改成同一份預設，README 更正「commit 格式屬 AOI 專屬」的說法。

### 驗證
- `scripts/ai/check-ai-structure.ps1 -Strict`：通過，0 警告。
- `node tools/ste-lint.mjs`：206 句，0 錯誤。
- 用修改後的範本重新建立範例專案：建置腳本結束碼 0，結構檢查通過，新的 Commit 訊息一節已帶入。
- 新 commit 訊息以腳本逐項檢查格式（標題、`測試 :`、Co-Authored-By、結尾兩行、每行寬度）。

### 未完成
- 12 份 `docs/plan/S*.md` 的「Commit 範本」仍是英文 Conventional 風格，沒有改寫；`project-rules.md` 規定它們只當內容參考，格式以規則為準。
- 確認改寫後的歷史沒問題後，刪除備份分支 `backup/before-commit-reformat`。

### 注意事項
- 結尾的 `[Test]` 與 `Build OK, Test OK` 是使用者的固定規則；專案目前沒有建置，所以它不代表本次有建置。實際驗證只看 `測試 :` 區塊，且必須如實。
- 兩份規範衝突時（repo 內文件與使用者慣例），先問使用者，不要自己挑。

## 2026-10-05 — Claude Code (Claude Sonnet 5.5)

### 完成
- 把 AI 共用結構（Codex 與 Claude Code）套用到本專案，並與既有的 `CLAUDE.md` 合併：原內容搬到 `AI_CONTEXT.md`、`docs/architecture.md`、`.agents/harness/project-rules.md`，`CLAUDE.md` 改為薄入口。
- `docs/STATUS.md` 更名為 `docs/current-status.md`；14 個檔案（12 份 `docs/plan/S*.md`、`00-master-plan.md`、`S00-overview/spec.md`）的舊引用改成新路徑。
- 新增 `docs/adr/000-ai-agent-structure.md`；`docs/decisions.md` 作為 ADR 索引。
- `.gitignore` 補上 `.claude/worktrees/`、`CLAUDE.local.md`。

### 修改的檔案
- 新增：`AGENTS.md`、`AI_CONTEXT.md`、`.agents/`、`.claude/`、`.ai/handoff.md`、`scripts/ai/`、`docs/architecture.md`、`docs/decisions.md`、`docs/changelog-ai.md`、`docs/adr/000-ai-agent-structure.md`
- 改寫：`CLAUDE.md`、`.gitignore`
- 更名：`docs/STATUS.md` → `docs/current-status.md`
- 引用更新：`docs/plan/S01`–`S12`、`docs/00-master-plan.md`、`docs/steps/S00-overview/spec.md`

### 驗證
- `scripts/ai/check-ai-structure.ps1 -Strict`：通過，0 警告（Windows PowerShell 5.1）。
- `node tools/ste-lint.mjs`：206 句，0 錯誤，0 警告。
- 舊名稱（`STATUS.md`、`CLAUDE.md 的…`）區分大小寫掃描：除了刻意保留的歷史說明，沒有殘留。
- Claude Code 實測：`AI_CONTEXT.md`、`project-rules.md`（含 Step Protocol）、`AGENTS.md`、`architecture.md` 都經 `CLAUDE.md` 的 `@` 匯入載入。
- 未驗證：Codex 端。這台的 Codex CLI 0.136.0 無法執行 `codex exec`（解析模型清單失敗），所以 `AGENTS.md` 的必讀清單流程沒有實測。

### 未完成
- 變更已 commit（`fd4fd2a`），並依使用者要求以 `--no-ff` 合併到 `main`。沒有 push，是否推到遠端由使用者決定。
- `ai-project-template/` 仍在 repo 內且未追蹤，不在這次 commit，要放哪裡由使用者決定。

### 注意事項
- 分層規則原本每個 Claude session 都載入，所以 `CLAUDE.md` 額外匯入 `docs/architecture.md`（`AGENTS.md` 也列入必讀）。不要移除，否則護欄會變弱。
- 舊版 `CLAUDE.md` 在 git 歷史（commit `28414b1`）。
- `.claude/settings.json` 的 deny 規則在 Claude Code 信任此資料夾後才生效。
