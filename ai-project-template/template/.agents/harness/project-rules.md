# 專案規則

本檔放「這個專案的慣例」，可依專案調整。流程在 `engineering-workflow.md`，專案事實在 `AI_CONTEXT.md`。標示 `TODO(project)` 的地方，建立專案時填寫或刪除。

## 程式碼風格

- 沿用周邊程式碼的命名、縮排、註解密度與慣用寫法；不做無關的重構或格式化。
- 新增依賴前，先確認既有依賴解決不了。
- TODO(project): 編碼與換行（例如 UTF-8 BOM、LF 或 CRLF）、縮排、命名規則、namespace 或套件結構。
- TODO(project): 註解語言與格式（例如 Doxygen、JSDoc）。

## Git

- 調查、review、狀態詢問不授權 commit 或 push；要做就先取得使用者同意。專案若在這裡明確寫了授權的流程（例如「每步完成後在本機 commit」），依該流程，且授權範圍不得擴大（例如沒寫 push 就不 push）。
- Stage 時明確列出路徑，禁止 `git add .` 與 `git add -A`（`.claude/settings.json` 也設了 deny）。
- Commit 前看 `git status` 與 `git diff --staged`；有 submodule 時逐一檢查，因為它們是獨立的 repository。
- 工作目錄可能有他人或其他 session 的未提交修改。同一個檔案混有他人修改時，先確認自己的精確 diff，不整檔 stage，也不回復他人的內容。
- Commit 訊息一律寫成檔案（UTF-8）再用 `git commit -F <檔案>`；不用 `-F -`（stdin）、heredoc 或拼接多個 `-m`。原因：Bash heredoc 會壓縮連續反斜線（UNC 路徑會壞），多行訊息經 shell 也可能被改寫，檔案不受影響。
- 不使用 `--no-verify`；force push、`reset --hard`、`clean` 需要使用者明確同意。
- 訊息格式見下方「Commit 訊息」。

### Commit 訊息

本節是 commit 訊息格式的唯一權威：任何 AI 在 commit、或只是產生一段訊息給人貼上時，都照本節產出。格式是使用者的固定慣例（出處：AlgoParamFineTune 的 `.clauderules`）。專案若有別的規定，直接改本節，不要另外在別處再寫一份。

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

- **標題**：`[作者名字]屬性: 修改說明`。`]` 之後不留空格，冒號後一個半形空格。作者名字取 git user 的名字（例如 `HQ\Caspar.Hsu` 寫 `Caspar`）。屬性用小寫英文，**只寫一個，不加括號**：`feat`、`fix`、`docs`、`refactor`、`test`、`chore`、`perf`、`build`。說明用繁體中文，一句話交代「到底改了什麼」，禁止「修正問題」「調整程式碼」這類沒有資訊量的句子。
- **正文**：與標題空一行。寫為什麼改與怎麼改的，不重述 diff；踩到的坑、否決的做法、實測的數字都寫進來。單行不超過 76 字元寬（中文約 38 字）。
- **`測試 :`**：`測試` 與冒號之間一個半形空格，自成一行；內容從下一行開始，用繁體中文（指令、函式名保留原文，敘述句要中文）。沒跑就照實寫沒跑，或寫實際做了什麼驗證；不可以為了填滿格式而編造結果。
- **`Co-Authored-By: Claude <模型名稱>`**：前後各空一行。模型名稱填產生這次 commit 的模型當下實際的名字，不要從文件或舊 commit 抄。**後面不接 email 與角括號**，這是刻意的選擇，不要順手加回去。
- **結尾固定兩行**：`[Test]` 與 `Build OK, Test OK`，純文件、純設定的修改、沒有建置的專案也照寫，不省略、不改字。這兩行是固定結尾，不代表本次有建置；實際驗證了什麼只看 `測試 :` 區塊。

## 文件

- 修改任何 Markdown 後，最終回應要列出「本次修改差異」。
- `docs/` 內的功能或設計文件，最前方保留修改紀錄表，新紀錄放在最上方。
- 流程圖寫進文件時保留 Mermaid 原始碼。
- 文件裡的路徑、指令與檔名必須是真的存在的；不確定就不寫。

## 驗證

- 驗證指令與 `VERIFIED` 的定義在 `AI_CONTEXT.md` 的「建置與測試」。
- TODO(project): 若有額外的 gate（例如 commit 前必跑的檢查），寫在這裡，並在 `.claude/settings.json` 以 hook 實作。
