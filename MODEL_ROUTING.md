# 模型路由規則（Claude Code 專用）

目的：依任務的難度與風險，決定用 Haiku、Sonnet 還是 Opus，在正確性與額度之間取平衡。`CLAUDE.md` 每個 session 都匯入本檔。

來源：使用者 2026-10-08 提供的 ChatGPT 討論（<https://chatgpt.com/share/6ac74783-b388-83ee-b335-1f3092f9ee36>）與其中的設定 prompt。規則已改寫成本專案的版本：判準改成可檢查的條件、例子換成本專案的模組、與 `engineering-workflow.md` 已有的規則（驗證、回報）不重複。

## 1. 原則與限制

- 模型一律用別名 `haiku`、`sonnet`、`opus`，不寫版本號，模型升級時規則不必改。
- **主 session 的模型不會因為本檔、hook 或 subagent 而改變。** 做法是把子任務委派給指定模型的 subagent。不要說「已切換模型」；要換主 session 的模型是使用者的操作，建議換時說出理由，由使用者決定。
- **hook 只注入提示，不強制。** Claude 仍要依實際任務重新評估，不可盲從 hook 的關鍵字分類。
- 沒把握時選 Sonnet。檔案多本身不是選 Opus 的理由。
- 不要沒有理由地降級：Haiku 省下的額度，抵不過一次改錯的重工。

## 2. 分級與對應的 subagent

| 等級 | 模型 | subagent（`.claude/agents/`） | 本專案的例子 | 不要用在 |
|---|---|---|---|---|
| 1 | Haiku | `fast-worker`（唯讀） | 找檔案、符號、呼叫點；整理 `build/qmake-*/test-logs/` 的失敗與 Log；彙整 `git diff --stat`；跑 `check-deps`、`ste-lint` 並摘要；簡單分析 | 設計、port 契約、需要寫入 repo 的工作 |
| 2（預設） | Sonnet | `developer` | 一般 C++、Qt（Presenter、Widgets 的功能與版面）、Python、網頁、UI 修改、`copy.json` 文案、單元與整合測試、`.pro` 加檔、一般除錯、步驟文件、已知架構內的重構 | 下面第 3 級的任何一項 |
| 3 | Opus | `architect`（唯讀，輸出設計與修改計畫） | 見第 3 節 | 查檔案、改文案這類不需要推理的工作 |

## 3. 判斷順序

由上而下，第一個符合的就停：

1. **Opus**：符合任一項。
   - 系統架構：改分層、port 契約、依賴表，或要寫新的 ADR（`docs/architecture.md`、`aoi_deps.pri`、`check-deps` 的 `RULES`）。
   - 並行、取消、可重入、物件生命週期、影像所有權（`JobRunner`、ADR-014）、座標與角度慣例（ADR-010）。
   - 晶格接受規則（ADR-013）、back end 的搜尋演算法與授權（ADR-016、ADR-017）、建置系統（ADR-018）。
   - 加密（AES）、金鑰管理、授權、不可逆的資料操作（刪除、遷移、覆寫使用者的檔案）。
   - 效能瓶頸分析（先量測，S11 規則）、CUDA 的複雜除錯（本專案目前沒有 CUDA，沿用你的分級）。
   - 跨模組重構（跨三層以上而且要先設計）；無法重現或根因不明的 bug。
2. **Haiku**：必須同時符合：唯讀或機械式、範圍已經明確、不影響設計判斷、做錯了很容易發現。
3. 其餘一律 **Sonnet**。

做到一半發現任務比預期難或風險更高，就重新分級；不要用原來的等級硬做完。

## 4. 升級

符合任一項就升到 Opus（委派 `architect`，或建議使用者換主 session 的模型）：

- 同一個問題兩次有效嘗試仍沒解決。「有效」＝依證據改了做法；重複同一個做法不算。
- 修正需要改變架構或 port。
- 測試失敗暴露的是系統層的問題，不是這次修改本身的錯。
- 發現安全或資料完整性的風險。
- 無法建立可靠的根因（`engineering-workflow.md` 第 4 節：證據不足，不當成已確認）。

升級時把已知的發現一起交過去：失敗的指令與輸出、已排除的假設、讀過的檔案。不要讓新的模型從頭來過。`architect` 只輸出分析與修改計畫，實作交給 `developer`（或主 session）。

## 5. 委派（`Agent` 工具）

- **主動評估，只在值得時委派**：獨立的子任務、能平行做的事、會產生大量輸出的事（大範圍搜尋、整理很長的 log）、需要較強推理的分析。小任務自己做；不要為了「分級」而開 agent，每個 agent 的額度都算在使用者身上。
- **委派有固定成本**（2026-10-08 實測，第 8 節）：每個 subagent 啟動時都載入 `CLAUDE.md` 與它匯入的文件，約 48K–68K tokens；只回一句話的委派，Sonnet 約 US$0.17、Opus 約 US$0.24（API 計價，不等於訂閱的額度）。所以「交給 Haiku 比較省」只對大量讀取的工作成立。
- **主 session 已經是建議的模型時，自己做**，不要委派給同一個模型。
- **怎麼委派**：`subagent_type` 填 `fast-worker`、`developer` 或 `architect`；prompt 寫清楚目標、已排除的事、要讀的檔案、要回報什麼。subagent 看不到這段對話。
- **subagent 的回報是未驗證的**：重要的結論自己查證後才寫進回報；subagent 說「測試通過」不能當 `VERIFIED`（`AGENTS.md` 不可省略的第 2 條）。
- **subagent 一樣受 `AGENTS.md` 的三條規則限制**：繁體中文回報、不虛構驗證、不 commit 與 push。三個 subagent 的定義檔已寫入這些規則。
- 同一個任務不要同時自己做又交給 subagent。

## 6. 回報

有委派或升級時，在最終回報的 Decision Review 加一行：任務的分級、用了哪個 subagent 與模型、理由。瑣碎的動作不必寫。

## 7. Hook

`.claude/settings.json` 的 `UserPromptSubmit` 在每次送出需求時執行 `.claude/hooks/model_router.mjs`：

- 用關鍵字（中英文）做初步分類，把建議的等級、subagent 與命中的原因，經 `additionalContext` 交給 Claude。內容會寫明：這只是提示、要重新評估、主 session 的模型沒有切換。
- 不呼叫外部 API。任何例外都以結束碼 0、不輸出收場，**不會擋住你的需求**。
- 太短的文字、斜線指令（`/…`）不處理。環境變數 `AOI_MODEL_ROUTER=off` 可以關掉。
- 限制：關鍵字比對不理解程式碼。貼上很長的文字時容易誤判（例如文字裡提到「架構」不代表任務是架構）。
- 為什麼用 Node 不用 Python：這台機器的 `python.exe` 只是 Store 捷徑（`AI_CONTEXT.md`），Node 是本專案已有的工具鏈（`tools/*.mjs`）。

## 8. 驗證

`node scripts/ai/check-model-routing.mjs`（改過本檔、subagent、hook 或 `settings.json` 之後執行）檢查：

1. 三個 subagent 的定義檔存在，`name` 與檔名一致，`model` 分別是 `haiku`、`sonnet`、`opus` 別名（不是版本號）。
2. `settings.json` 是合法 JSON、hook 已登錄且指向存在的檔案，`permissions` 與 `git HEAD` 的版本相比沒有少任何一項。
3. hook 對一組範例需求給出預期的分類；空輸入、壞掉的輸入、太短的輸入、`AOI_MODEL_ROUTER=off` 都以結束碼 0、不輸出收場。
4. `CLAUDE.md` 匯入本檔。

腳本**驗證不了**的事（Claude Code 真的載入了 hook 與 subagent、委派後的實際模型與回報）要開一個新的 session 看。互動式：`/agents` 列出三個 subagent；送出需求後 context 有路由提示，委派時的 `Agent` 呼叫出現對應的 subagent。也可以用 headless 模式，從紀錄直接讀出實際的模型：

```powershell
claude -p "<要求它照抄路由提示、列出 subagent_type、委派三個 subagent 各回一句話>" --model haiku --max-turns 6 --output-format stream-json --verbose > run.jsonl
```

看 `system/init` 的 `agents`（有三個 subagent），以及每則 assistant 訊息的 `model` 與 `parent_tool_use_id`（subagent 實際用的模型）。session 啟動時 `.claude/agents/` 還不存在的話，新增的 subagent 要重新開 session 才看得到。

### 2026-10-08 實測（Claude Code 2.1.292，headless）

- hook 有執行：新 session 的 context 有路由提示，內容和 hook 的輸出相同。互動式（桌面 app 的 Code 分頁）也有：在進行中的 session 改完 `settings.json` 之後，下一則使用者訊息的 context 就有路由提示，不必重新開 session。
- `system/init` 的 `agents` 有 `fast-worker`、`developer`、`architect`；三個同時委派，回報都回到主 session。
- 實際模型：`fast-worker` → `claude-haiku-4-5-20251001`、`developer` → `claude-sonnet-5-5`、`architect` → `claude-opus-5-5`。**`haiku` 別名在這個版本對應 Haiku 4.5，不是 Haiku 5.5。**要用 5.5 只能寫完整的 model ID（違反第 1 節的別名原則）或等 Claude Code 更新別名；目前照別名。
- 一次測試（主 session 用 Haiku、三個 subagent 各回一句話）共 US$0.59（API 計價）；`developer` 沒有列 `tools`（繼承全部工具），啟動的 context 比只列 5 個工具的 `architect` 大（68K 對 48K tokens；`fast-worker` 的數字和 Haiku 主 session 合在一起，分不出來）。

Codex 沒有對應機制，不讀本檔。
