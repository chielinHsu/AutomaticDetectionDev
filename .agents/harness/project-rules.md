# 專案規則

本檔放「這個專案的慣例」，可依專案調整。流程在 `engineering-workflow.md`，專案事實在 `AI_CONTEXT.md`。
本檔的 SOLID、四種輸出、Step Protocol、Commit 規則原本在 `CLAUDE.md`，2026-10-05 搬進來（見 `docs/adr/000-ai-agent-structure.md`）。文字照搬，只有 Step Protocol 第 1、4、5、7 步調整：檔名改為 `current-status.md`、第 4 步加入交接單與 changelog、第 5 步的 commit 格式改為下方「Commit 訊息」（原本是 Conventional Commits）、第 7 步註明 `spawn_task` 是 Claude 專用。

## 程式碼風格

- 沿用周邊程式碼的命名、縮排、註解密度與慣用寫法；不做無關的重構或格式化。
- 新增依賴前，先確認既有依賴解決不了。
- 換行：文字檔 LF；`*.bat`、`*.cmd`、`*.ps1` 在 checkout 時為 CRLF（見 `.gitattributes`）。
- 其餘風格規則（命名、縮排、註解格式）尚未定義；S02 建立專案骨架時若定義了，寫在這裡。

### SOLID 檢查清單（每個 PR / commit 前自問）

- **S** 這個類別只有一個變更理由嗎？超過 300 行要在 commit 說明。
- **O** 新增功能是「新增類別 + 註冊」，還是「修改既有 use case」？後者要有理由。
- **L** 新的 `IPatternSearcher` 實作能通過同一組契約測試（S09）嗎？
- **I** 介面 ≤ 5 個方法？UI 只看到它需要的最小介面？
- **D** use case 只依賴 port，具體 adapter 只在 `app/` 組裝？

## 每一步都必須交付四種輸出（Karpathy 的 4 種輸出形式）

目的：讓讀者清楚知道這個設計的**目的（Purpose）、原因（Reason）、效果（Effect）**。放在 `docs/steps/Sxx-<slug>/`，範本在 `docs/steps/_template/`。

| # | 輸出 | 檔案 | 要求 |
|---|---|---|---|
| 1 | 寫作 Writing | `spec.md` | 用 ASD-STE100 風格英文寫 Purpose / Reason / Effect（放在 `<!--STE-->` … `<!--/STE-->` 之間），每段後附繁中摘要。短句（描述句 ≤ 25 字、程序句 ≤ 20 字）、主動語態、一詞一義。執行 `node tools/ste-lint.mjs` 必須無 error。這是長度/被動語態的檢查，**不是**完整 STE 驗證。 |
| 2 | 圖解 Diagrams | `diagram.md` | Mermaid 圖（結構圖、流程圖、序列圖、狀態圖擇需要者）。能用圖就不要用長文。 |
| 3 | 互動網頁 Web page | `explainer.html` | 單一檔案、離線可開、深色/淺色皆可讀、手機寬度可用；要有可操作的元件（slider、按鈕、步進）。需要的文字放在 `<script type="application/json" id="aoi-data">`，其中 `ste` 欄位會被 lint 檢查。 |
| 4 | 解說影片 Explainer video | `storyboard.md` | 分鏡表（時間、畫面、旁白 STE 英文、繁中字幕）。實際影片/語音合成是選配，需使用者同意再做。 |

另外每一步要有 `perf.md`：本步驟量測了哪些指標、預算、實測值、機器指紋。

## Step Protocol（每完成一個步驟必做；不要在同一個 session 連做下一步）

1. 開工：讀 `docs/current-status.md` 與 `docs/plan/Sxx-*.md`。在 `main` 上建分支 `step/NN-slug`。
2. 實作，並同時產出四種輸出與 `perf.md`。
3. 驗證：建置通過、單元/契約測試通過、`node tools/check-deps.mjs`（S03 起）、`node tools/ste-lint.mjs` 皆無 error；本步驟的效能指標已量測並與 `perf/budgets.json` 比對。**失敗就如實回報，不要宣稱完成。**
4. 更新 `docs/current-status.md`，並依 `ai-handoff` skill 更新 `.ai/handoff.md` 與 `docs/changelog-ai.md`。
5. Commit（格式見下方「Commit 訊息」；該步驟 plan 檔的「Commit 範本」只當內容參考），合併到 `main`（`git merge --no-ff`），打 tag `step-NN-done`。僅限本機，**不要 push**，除非使用者要求。
6. 對使用者**提示 commit 內容**：commit hash、訊息、變更檔案摘要、效能結果、未解決事項。
7. 呼叫 `spawn_task` 為下一步建立新 session（prompt 取自**下一步** plan 檔底部的「Start prompt」）。若無法呼叫，改為把 prompt 印出來讓使用者貼上。**到此為止，不要接著做下一步。**（`spawn_task` 是 Claude Code 的工具；Codex 一律走「印出 prompt」。）

## Git

- 調查、review、狀態詢問不授權 commit 或 push；要做就先取得使用者同意。本專案明確授權的流程是 Step Protocol 第 5 步：每步完成後在本機 commit、`merge --no-ff`、打 tag；**不包含 push**。
- Stage 時明確列出路徑，禁止 `git add .` 與 `git add -A`（`.claude/settings.json` 也設了 deny）。
- Commit 前看 `git status` 與 `git diff --staged`。
- 工作目錄可能有他人或其他 session 的未提交修改。同一個檔案混有他人修改時，先確認自己的精確 diff，不整檔 stage，也不回復他人的內容。
- Commit 訊息一律寫成檔案（UTF-8）再用 `git commit -F <檔案>`；不用 `-F -`（stdin）、heredoc 或拼接多個 `-m`。原因：Bash heredoc 會壓縮連續反斜線，多行訊息經 shell 也可能被改寫，檔案不受影響。
- 不使用 `--no-verify`；force push、`reset --hard`、`clean` 需要使用者明確同意。

### Commit 訊息

本節是 commit 訊息格式的唯一權威：任何 AI 在 commit、或只是產生一段訊息給人貼上時，都照本節產出。格式沿用使用者在其他專案的慣例（AlgoParamFineTune 的 `.clauderules`）。各步驟 plan 檔的「Commit 範本」只當內容參考（改了什麼、含哪些項目），格式與語言一律以本節為準，內容要改寫成繁體中文。

訊息骨架（每個空行都是格式的一部分，不可增減）：

```
[作者名字]屬性: 一行修改說明

<正文>

測試 :
<測試說明>

Co-Authored-By: Claude <當下模型名稱>

[Test]
Build OK, Test OK
```

- **標題**：`[作者名字]屬性: 修改說明`。`]` 之後不留空格，冒號後一個半形空格。作者名字取 git user 的名字（本機是 `HQ\Caspar.Hsu`，寫 `Caspar`）。屬性用小寫英文，**只寫一個，不加括號**：`feat`、`fix`、`docs`、`refactor`、`test`、`chore`、`perf`、`build`。說明用繁體中文，一句話交代「到底改了什麼」，禁止「修正問題」「調整程式碼」這類沒有資訊量的句子。
- **正文**：與標題空一行。寫為什麼改與怎麼改的，不重述 diff；踩到的坑、否決的做法、實測的數字都寫進來。單行不超過 76 字元寬（中文約 38 字）。
- **`測試 :`**：`測試` 與冒號之間一個半形空格，自成一行；內容從下一行開始，用繁體中文（指令、函式名保留原文，敘述句要中文）。沒跑就照實寫沒跑，或寫實際做了什麼驗證；不可以為了填滿格式而編造結果。
- **`Co-Authored-By: Claude <模型名稱>`**：前後各空一行。模型名稱填產生這次 commit 的模型當下實際的名字，不要從文件或舊 commit 抄。**後面不接 email 與角括號**，這是刻意的選擇，不要順手加回去。
- **結尾固定兩行**：`[Test]` 與 `Build OK, Test OK`，純文件、純設定的修改也照寫，不省略、不改字。這兩行是固定結尾，不代表本次有建置；實際驗證了什麼只看 `測試 :` 區塊。
- 一個 commit 一個意圖；格式化、重新命名不要混進功能 commit。
- 不要提交：`build/`、真實產線影像（`/data/`）、`perf/results/`。

## 文件

- 修改任何 Markdown 後，最終回應要列出「本次修改差異」。
- `docs/` 內的功能或設計文件，最前方保留修改紀錄表，新紀錄放在最上方。
- 流程圖寫進文件時保留 Mermaid 原始碼。
- 文件裡的路徑、指令與檔名必須是真的存在的；不確定就不寫。
- 設計決策寫成 `docs/adr/NNN-slug.md`，並在 `docs/decisions.md` 的索引加一行。

## 驗證

- 驗證指令與 `VERIFIED` 的定義在 `AI_CONTEXT.md` 的「建置與測試」；步驟層級的驗證項目在 Step Protocol 第 3 步。
- 目前沒有額外的 hook 或 gate。
