# 專案規則

本檔放「這個專案的慣例」，可依專案調整。流程在 `engineering-workflow.md`，專案事實在 `AI_CONTEXT.md`。標示 `TODO(project)` 的地方，建立專案時填寫或刪除；其餘段落是範本 2.0.0 的通用規則，專案可依需要改寫，但不要刪掉 SOLID、四種輸出、Step Protocol、Git 與 Commit 訊息的骨架。

## 程式碼風格

- 沿用周邊程式碼的命名、縮排、註解密度與慣用寫法；不做無關的重構或格式化。
- 新增依賴前，先確認既有依賴解決不了。
- 換行：文字檔 LF；`*.bat`、`*.cmd`、`*.ps1` 在 checkout 時為 CRLF（用 `.gitattributes` 設定）。含中文的 `.ps1` 存成 UTF-8 with BOM，否則 Windows PowerShell 5.1 會用系統字碼頁解析而亂碼；Markdown 與其他文字檔用 UTF-8 無 BOM。
- TODO(project): 編碼細節、縮排、命名規則（型別、函式、變數、常數）、namespace 或套件結構。
- TODO(project): 註解語言與格式（例如 Doxygen、JSDoc）。

### SOLID 檢查清單（每個 PR / commit 前自問）

- **S** 這個類別只有一個變更理由嗎？超過 300 行要在 commit 說明。
- **O** 新增功能是「新增類別 + 註冊」，還是「修改既有 use case」？後者要有理由。
- **L** 新的實作能通過同一組介面的契約測試嗎？
- **I** 介面 ≤ 5 個方法？UI 只看到它需要的最小介面？
- **D** use case 只依賴抽象介面（port），具體 adapter 只在組裝處（composition root）建立？

## 每一步都必須交付四種輸出（Karpathy 的 4 種輸出形式）

目的：讓讀者清楚知道這個設計的**目的（Purpose）、原因（Reason）、效果（Effect）**。放在 `docs/steps/<ID>-<slug>/`，範本在 `docs/steps/_template/`。

**適用範圍**：Step Protocol 的步驟，以及 `docs/current-status.md` 需求進度表中類型標為「功能」的需求。**小修正（fix、docs、chore、refactor 等）免四種輸出**，只要依 `ai-handoff` skill 更新進度表、交接單與 changelog。

| # | 輸出 | 檔案 | 要求 |
|---|---|---|---|
| 1 | 寫作 Writing | `spec.md` | 用 ASD-STE100 風格英文寫 Purpose / Reason / Effect（放在 `<!--STE-->` … `<!--/STE-->` 之間），每段後附繁中摘要。短句（描述句 ≤ 25 字、程序句 ≤ 20 字）、主動語態、一詞一義。執行 `node tools/ste-lint.mjs` 必須無 error。這是長度/被動語態的檢查，**不是**完整 STE 驗證。 |
| 2 | 圖解 Diagrams | `diagram.md` | Mermaid 圖（結構圖、流程圖、序列圖、狀態圖擇需要者）。能用圖就不要用長文。 |
| 3 | 互動網頁 Web page | `explainer.html` | 單一檔案、離線可開、深色/淺色皆可讀、手機寬度可用；要有可操作的元件（slider、按鈕、步進）。需要的文字放在 `<script type="application/json" id="ste-data">`，其中 `ste` 欄位會被 lint 檢查（舊專案的 `id="aoi-data"` 也認得）。 |
| 4 | 解說影片 Explainer video | `storyboard.md` | 分鏡表（時間、畫面、旁白 STE 英文、繁中字幕）。實際影片/語音合成是選配，需使用者同意再做。 |

另外每一步要有 `perf.md`：本步驟量測了哪些指標、預算、實測值、機器指紋。本步驟沒有效能指標時，寫「本步驟無效能指標」並說明原因，不要留白。

## Step Protocol（每完成一個步驟必做；不要在同一個 session 連做下一步）

步驟 ID `<ID>` 用總體計畫（`docs/00-master-plan.md`）的編號，例如 `S01`；需求進度表的 `Rnn` 若自成一步也可以當 ID。

1. 開工：讀 `docs/current-status.md`（需求進度表）與 `docs/plan/<ID>-*.md`。從預設分支建立分支 `step/NN-slug`；把本步驟在進度表裡的列標為 ⏳。
2. 實作，並同時產出四種輸出與 `perf.md`。
3. 驗證：建置通過、單元/契約測試通過、`node tools/ste-lint.mjs` 無 error，以及 `AI_CONTEXT.md` 與該步驟 plan 檔「完成定義」列出的其他檢查；本步驟的效能指標已量測並與預算（若專案有定義）比對。**失敗就如實回報，不要宣稱完成。**
4. 更新 `docs/current-status.md`（需求進度表該列改 ✅，補分支／commit 與驗證），並依 `ai-handoff` skill 更新 `.ai/handoff.md` 與 `docs/changelog-ai.md`。
5. Commit（格式見下方「Commit 訊息」；該步驟 plan 檔的「Commit 範本」只當內容參考），合併到預設分支（`git merge --no-ff`），打 tag `step-NN-done`。僅限本機，**不要 push**，除非使用者要求。
6. 對使用者**提示 commit 內容**：commit hash、訊息、變更檔案摘要、效能結果、未解決事項。
7. 為下一步建立新 session（prompt 取自**下一步** plan 檔底部的「Start prompt」，並提醒新 session 回覆使用繁體中文）：Claude Code 用 `start_session` 或 `spawn_task`（工具可用時）；Codex 或工具不可用時，把 prompt 印出來讓使用者貼上。**到此為止，不要接著做下一步。**

## Git

- 調查、review、狀態詢問不授權 commit 或 push；要做就先取得使用者同意。本專案明確授權的流程是 Step Protocol 第 5 步：每步完成後在本機 commit、`merge --no-ff`、打 tag；**不包含 push**。TODO(project): 若專案有 remote 或 CI，寫出 push、PR 與合併的授權範圍；沒寫就不 push。
- Stage 時明確列出路徑，禁止 `git add .` 與 `git add -A`（`.claude/settings.json` 也設了 deny）。
- Commit 前看 `git status` 與 `git diff --staged`；有 submodule 時逐一檢查，因為它們是獨立的 repository。
- 工作目錄可能有他人或其他 session 的未提交修改。同一個檔案混有他人修改時，先確認自己的精確 diff，不整檔 stage，也不回復他人的內容。
- Commit 訊息一律寫成檔案（UTF-8）再用 `git commit -F <檔案>`；不用 `-F -`（stdin）、heredoc 或拼接多個 `-m`。原因：Bash heredoc 會壓縮連續反斜線，多行訊息經 shell 也可能被改寫，檔案不受影響。
- 不使用 `--no-verify`；force push、`reset --hard`、`clean` 需要使用者明確同意。

### Commit 訊息

本節是 commit 訊息格式的唯一權威：任何 AI 在 commit、或只是產生一段訊息給人貼上時，都照本節產出。格式是使用者的固定慣例，與上層共用規則檔（例如 `D:\ClaudeWork\CLAUDE.md`）相同；**上層共用規則檔有規定時以上層為準**（例如固定的作者身分與 commit 指令寫法），專案若有別的規定，直接改本節，不要另外在別處再寫一份。

訊息骨架（每個空行都是格式的一部分，不可增減）：

```
[作者名字]type: 一行修改說明

<正文>

測試 :
<測試說明>

Co-Authored-By: Claude <當下模型名稱>

[Test]
Build OK, Test OK
```

- **標題**：`[作者名字]type: 修改說明`。`]` 之後不留空格，冒號後一個半形空格。作者名字取 git user 的名字（例如 `HQ\Caspar.Hsu` 寫 `Caspar`）。type 用小寫英文，**只寫一個，不加括號**：`feat`、`fix`、`docs`、`refactor`、`test`、`chore`、`perf`、`build`。說明用繁體中文，一句話交代「到底改了什麼」，禁止「修正問題」「調整程式碼」這類沒有資訊量的句子。
- **正文**：與標題空一行，用散文（不用條列）。寫為什麼改與怎麼改的，不重述 diff；踩到的坑、否決的做法、實測的數字都寫進來。單行不超過 76 字元寬（中文約 38 字）。
- **`測試 :`**：`測試` 與冒號之間一個半形空格，自成一行；內容從下一行開始，用繁體中文（指令、函式名保留原文，敘述句要中文）。沒跑就照實寫沒跑，或寫實際做了什麼驗證；不可以為了填滿格式而編造結果。
- **`Co-Authored-By: Claude <模型名稱>`**：前後各空一行。模型名稱填產生這次 commit 的模型當下實際的名字，不要從文件或舊 commit 抄。**後面不接 email 與角括號**，這是刻意的選擇，不要順手加回去。
- **結尾固定兩行**：`[Test]` 與下一行的測試結果（例如 `Build OK, Test OK`；實際失敗就照實寫，如 `Build OK, Test FAIL`）。純文件、純設定的修改、沒有建置的專案也照寫，不省略。這兩行不代表本次有建置；實際驗證了什麼只看 `測試 :` 區塊。
- 一個 commit 一個意圖；格式化、重新命名不要混進功能 commit。
- TODO(project): 不要提交的路徑（例如 `build/`、真實資料、效能結果輸出）；`.gitignore` 要涵蓋。

## 文件

- 修改任何 Markdown 後，最終回應要列出「本次修改差異」。
- `docs/` 內的功能或設計文件，最前方保留修改紀錄表，新紀錄放在最上方。
- 流程圖寫進文件時保留 Mermaid 原始碼。
- 文件裡的路徑、指令與檔名必須是真的存在的；不確定就不寫。
- 設計決策寫成 `docs/adr/NNN-slug.md`（範本 `docs/adr/_template.md`），並在 `docs/decisions.md` 的索引加一行；`decisions.md` 不放 ADR 內文。
- 需求進度表（`docs/current-status.md`）與計畫檔（`docs/plan/`）的規則見 `engineering-workflow.md`「需求展開與進度表」。

## 驗證

- 驗證指令與 `VERIFIED` 的定義在 `AI_CONTEXT.md` 的「建置與測試」；步驟層級的驗證項目在 Step Protocol 第 3 步。
- 改過入口檔、skill、`.claude/` 或 `scripts/ai/` 之後，執行 `scripts/ai/check-ai-structure.ps1`（填完 `TODO(project)` 後加 `-Strict`）；改過模型路由相關檔案，再跑 `node scripts/ai/check-model-routing.mjs`。
- TODO(project): 若有額外的 gate（例如 commit 前必跑的檢查、真實資料回歸），寫在這裡，並在 `.claude/settings.json` 以 hook 實作。
