# AI 專案共用結構（Codex 與 Claude Code）

版本 1.0.0。目標：任何新資料夾，不論用 Claude Code 或 Codex 開發，都套用同一份規範，只維護一份知識，兩個平台各自的入口檔只做轉接。

## 用法

```powershell
powershell -ExecutionPolicy Bypass -File <本資料夾>\New-AiProject.ps1 -Path D:\Work\MyApp
```

| 參數 | 作用 |
|---|---|
| `-Name "My App"` | 專案名稱，預設用資料夾名稱 |
| `-DryRun` | 只列出會做什麼，不寫入 |
| `-Refresh` | 同時更新「範本管理」的檔案（見下方維護規則） |

腳本會：複製範本、替換 `{{PROJECT_NAME}}` 與 `{{DATE}}`、補上 `.gitignore` 缺的行、把 skill 鏡像到 `.claude/skills/`、跑一次結構檢查。它不會 `git init`、不會 commit，也不會覆蓋任何已存在的檔案。

建立後的第一件事：填寫 `AI_CONTEXT.md` 與 `.agents/harness/project-rules.md` 裡所有 `TODO(project)`，用 `scripts/ai/check-ai-structure.ps1 -Strict` 確認都填完了。

本資料夾可以整個搬到任何位置，腳本是用自己所在的位置找 `template/` 的。

## 結構

```text
<project>/
├─ AGENTS.md                      入口（Codex 讀；Claude 經 CLAUDE.md 匯入）
├─ CLAUDE.md                      入口（Claude 讀）：匯入 AGENTS.md 與三份穩定文件，加 Claude 專屬規則
├─ AI_CONTEXT.md                  專案事實（用途、技術棧、目錄、建置與測試、限制、Context Routing）
├─ .agents/
│  ├─ harness/
│  │  ├─ engineering-workflow.md  通用工作流程（範本管理）
│  │  ├─ project-rules.md         專案慣例：風格、Git、文件
│  │  ├─ skill-sync.json          skill 鏡像規則
│  │  └─ TEMPLATE_VERSION
│  └─ skills/<name>/SKILL.md      skill 的來源（Codex 直接讀）
├─ .claude/
│  ├─ settings.json               權限護欄（deny git add -A / .，ask push 與破壞性指令）
│  └─ skills/<name>/SKILL.md      鏡像複本（由腳本產生，不手改）
├─ .ai/handoff.md                 交接單（覆寫式）
├─ docs/
│  ├─ architecture.md             現在的結構
│  ├─ decisions.md                設計決策（只新增）
│  ├─ current-status.md           目前進度與待辦
│  └─ changelog-ai.md             每個 agent session 的紀錄（只新增）
└─ scripts/ai/
   ├─ sync-skills.ps1             .agents/skills → .claude/skills
   └─ check-ai-structure.ps1      結構檢查
```

## 各檔職責（互不重疊）

| 檔案 | 回答的問題 | 變動頻率 | 寫入方式 |
|---|---|---|---|
| `AGENTS.md`、`CLAUDE.md` | 開工先讀什麼、哪幾條不能省 | 幾乎不變 | 原地修改 |
| `AI_CONTEXT.md` | 這個專案是什麼、怎麼建置與驗證 | 少 | 原地修改 |
| `engineering-workflow.md` | agent 怎麼工作（所有專案相同） | 範本升級時 | 範本管理 |
| `project-rules.md` | 這個專案的慣例 | 少 | 原地修改 |
| `docs/decisions.md` | 為什麼這樣設計 | 有決策時 | 只新增，新的在上 |
| `docs/current-status.md` | 現在做到哪、還剩什麼 | 每次收工 | 原地修改 |
| `docs/changelog-ai.md` | 上一個 agent 做了什麼 | 每次收工 | 只新增，新的在上 |
| `.ai/handoff.md` | 下一位 agent 第一件要知道的事 | 每次收工 | 整份覆寫 |

入口檔裡只有三條「不可省略」的規則（繁中、不虛構驗證、不擅自 commit 與 stage 要明確列路徑）重複出現一行，因為簡單任務時 agent 可能不會往下讀。其他共通內容都只存在一處。

## 兩個平台怎麼讀（已查證的事實）

| 項目 | Claude Code | Codex |
|---|---|---|
| 入口檔 | `CLAUDE.md`。沒有 CLAUDE.md 時才自動讀 `AGENTS.md`（v2.1.277 以上）；兩者都有時只讀 CLAUDE.md | `AGENTS.md`，從 git 根目錄往下到工作目錄逐層合併 |
| 匯入其他檔案 | 支援，`@path` 寫在獨立一行 | **不支援**，所以 AGENTS.md 用「必讀清單」 |
| 大小限制 | 官方建議保持精簡（越具體簡短，遵守度越高） | 合併後預設上限 32 KiB，超過就不再加入 |
| Skill 位置 | `.claude/skills/` | `.agents/skills/`（repo 內），不讀 `.claude/skills/` |
| `.agents/` 下的內容 | 不會自動讀 | 只有 skill 會被掃描，`.agents/harness/` 不會自動載入 |

因此設計成：CLAUDE.md 一定存在，並用 `@AGENTS.md` 匯入。官方說明這樣不會重複載入，而且在讀不到 AGENTS.md 的舊版或 Bedrock 環境也能運作。Skill 因為兩邊不共用目錄，只能用複本，所以 `check-ai-structure.ps1` 會檢查兩份是否一致。

來源：<https://code.claude.com/docs/en/memory>、<https://learn.chatgpt.com/docs/agent-configuration/agents-md>、<https://learn.chatgpt.com/docs/build-skills>。

## 兩份來源怎麼合併

### ChatGPT 文章（<https://chatgpt.com/share/6ac21911-ddf4-83ee-b3b6-8fdb31144456>）

採用：入口檔是 bootloader 不是 database；共用知識集中一份（`AI_CONTEXT.md`）；`architecture`、`decisions`、`current-status`、`changelog-ai` 四份 docs；`.ai/handoff.md` 交接單。

調整：

- 文章讓兩個入口各抄一份必讀清單，會漂移。改成 `CLAUDE.md` 匯入 `AGENTS.md`，清單只存一處。
- 刪掉 `.ai/tasks/active.md` 與 `backlog.md`：和 `current-status.md` 的待辦、`handoff.md` 的「下一步」重疊，三處都要更新就會不一致。待辦併入 `current-status.md`。
- 文章只靠「請 agent 記得更新」，沒有檢查。加了 `check-ai-structure.ps1`，入口檔漏列必讀文件、匯入失效、skill 兩份不一致、`AGENTS.md` 超過 32 KiB 都會 FAIL。
- 交接單加了「未提交的變更」與「假設與未驗證項目」兩欄，因為兩個 agent 會在同一個 dirty working tree 上接力。

### AlgoParamFineTune

帶過來：薄入口加共用 harness 的分層；工作流程（證據分級、`VERIFIED` 三種結果、決策審查的九個面向與 Decision／Evidence／Verification、回報格式、修改前後流程圖）；skill 以 `.agents/skills` 為來源、`.claude/skills` 為鏡像並用 manifest 管理；禁止 `git add .`、commit 訊息用 `-F`、他人未提交修改視為使用者資料；文件修改紀錄表新的在上。

**排除（AOI 專屬）**：

| 排除項目 | 原因 |
|---|---|
| `verify.ps1`、`verification-map.json`、Verification gate hooks、`VerificationGate.psm1`、`VerificationLedger.psm1` | 綁 MSBuild 與 AOI 模組路由。gate 的概念保留為專案自選，見 `project-rules.md`「驗證」一節 |
| `engineering-review.ps1`、`engineering-review-map.json` | 把路徑對應到 AOI 模組。審查的九個面向保留成通用清單 |
| `hokue-commit-validate`、`hokue-version-validate`、`memory-check`、`scan-view-callback-modify`、`cv-algorithm-add-class`、`procpipe-add-task`、`clr-wrapper`、`coordinate-systems`、`file-formats`、`shared-version`、`log-standard`、`coding-standard`、`benano-*`、`wiki` | AOI 測試、判別、CLR、座標、NAS 發佈等專屬 skill |
| `.clauderules` 的 AOI 編碼標準、`.cursor/rules/`、`Find-MergeDuplicateImports.ps1` | AOI 與 C# 專屬 |

注意：`.clauderules` 的 **commit 訊息格式**（`[作者]屬性:`、`測試 :`、`Co-Authored-By` 不帶 email、`[Test]` 結尾）一開始被我當成 AOI 專屬而排除，這是錯的：它是使用者的個人慣例，不是 AOI 測試邏輯。現在已設為範本的預設（`project-rules.md`「Commit 訊息」）。

`grill-me`、`grilling`、`interactive-algorithm-explainer`、`ui-ux-pro-max`、`claude-statusline` 看起來可以跨專案，但我只看過它們的 description，沒有逐一確認內容，所以沒有放進範本。要用時複製到 `.agents/skills/` 再執行 `sync-skills.ps1`。

## 維護規則

**範本管理**（`-Refresh` 會覆蓋，不要在專案裡改）：`engineering-workflow.md`、`TEMPLATE_VERSION`、`.agents/skills/ai-handoff/`、`scripts/ai/`。
**專案擁有**（建立後永遠不會被覆蓋）：其餘所有檔案。

- 新增共用必讀文件：在 `AGENTS.md` 的清單加一行，在 `CLAUDE.md` 加 `@路徑`，執行 `check-ai-structure.ps1` 確認。
- 新增 skill：放進 `.agents/skills/<name>/SKILL.md`，執行 `sync-skills.ps1`。只給 Claude 用的 skill 放 `.claude/skills/`，並登記到 `skill-sync.json` 的 `claudeOnly`；只給 Codex 用的登記 `codexOnly`。
- 升級範本：改 `template/` 後遞增 `TEMPLATE_VERSION`，對各專案執行 `New-AiProject.ps1 -Path <專案> -Refresh`。
- 套用到已有 `CLAUDE.md` 或 `AGENTS.md` 的專案：腳本不會動它們，檢查會列出缺少的匯入與必讀項目，請手動合併。合併做法（AutoCadCreated 的實例見其 `docs/adr/000-ai-agent-structure.md`）：
  1. 先確認沒有進行中的分支或 worktree。
  2. 若已有自己的狀態檔（如 `docs/STATUS.md`），在套用前先更名為 `docs/current-status.md`，再全專案掃描並更新舊引用，用區分大小寫的搜尋確認沒有殘留。
  3. 把舊入口檔的內容搬到共用文件：專案事實進 `AI_CONTEXT.md`，結構與依賴規則進 `docs/architecture.md`，慣例與流程進 `project-rules.md`。舊入口檔改成薄入口。
  4. 若舊入口檔裡有「每個 session 都該看到」的護欄（如分層規則），把搬走後的文件加進 `AGENTS.md` 必讀清單與 `CLAUDE.md` 的 `@` 匯入，否則護欄會從常駐 context 消失。
  5. 若專案已用 `docs/adr/` 一檔一決策，`docs/decisions.md` 改為索引。
  6. 若專案的流程授權了 commit（例如每步在本機 commit），寫進 `project-rules.md` 的 Git 一節，並限定範圍。
  7. 執行 `check-ai-structure.ps1 -Strict`，再用 `claude -p` 實測匯入有載入。

## 已知限制

- **Codex 端沒有在本機端到端測過**：這台的 Codex CLI 0.136.0 無法解析伺服器回傳的模型清單（`unknown variant 'max'`），`codex exec` 直接失敗。Codex 的行為是依官方文件設計，並由檢查腳本驗證結構（含 32 KiB 上限）。Claude Code 端已實測，三份文件都由 `@` 匯入載入。
- `.claude/settings.json` 的 deny 規則是護欄，不是安全邊界。Claude Code 在新資料夾要先接受信任對話框，`permissions` 才會生效。
- Skill 鏡像是複製，不是連結（Windows 符號連結需要權限，git 設定也會影響）。所以改了來源要記得跑 `sync-skills.ps1`；忘記時 `check-ai-structure.ps1` 會擋。
- commit 格式結尾的 `[Test]` 與 `Build OK, Test OK` 是使用者的固定規則，沒有建置的專案也照寫，所以它不代表本次有建置；實際驗證了什麼只看 `測試 :` 區塊，且必須如實。
- 腳本只用 Windows PowerShell 5.1 測過，沒有在 PowerShell 7 與 macOS／Linux 上測。
- 與 AlgoParamFineTune 的 `skill-sync.json` 欄位不同（它用 `sharedSkills`、`excludedSkills`、`allowedTargetOnlySkills`），那個專案不能直接套用本範本的腳本，遷移需要另外做。

## 與 AlgoParamFineTune 現有檔案的對照（供日後遷移）

| AlgoParamFineTune | 本結構 |
|---|---|
| `CLAUDE.md`（匯入三份 harness） | `CLAUDE.md`：`@AGENTS.md` 加三份穩定文件，只放 Claude 專屬規則 |
| `AGENTS.md`（Codex 專屬入口） | `AGENTS.md`：平台中立，Claude 也透過匯入使用 |
| `.agents/harness/engineering-workflow.md` | 同路徑，去除 AOI 內容，改為範本管理 |
| `.agents/harness/context-map.md` | 專案事實與路由併入 `AI_CONTEXT.md` |
| `.agents/harness/project-rules.md` | 同路徑 |
| `.claude/.clauderules` | 通用部分併入 `project-rules.md`；AOI 編碼標準留在該專案 |
| `scripts/agent/*` | 通用部分改寫為 `scripts/ai/*`；AOI 驗證相關不帶入 |
