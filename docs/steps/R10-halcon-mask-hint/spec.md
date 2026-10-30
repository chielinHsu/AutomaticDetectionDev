# R10 — 比對區的建議（HALCON 建好樣本後建議用 `M` 點端子的遮罩）：目的、原因、效果

> 規則：英文區塊用 ASD-STE100 風格（短句、主動語態、一詞一義）。繁中摘要放在每段之後。
> 檢查：`node tools/ste-lint.mjs`。設計決策：[ADR-042](../../adr/042-match-mask-advice.md)；計畫：[R10](../../plans/completed/R10-halcon-mask-hint/plan.md)。

<!--STE-->
## Purpose
This step adds one advice card to the message bar. The card appears when three conditions are true. The back end says that a mask gives a better template. The board has a sample without a match region. The tool is idle. The card tells the user to click a pin with the M tool. The user can still press F5 to search.
<!--/STE-->

**繁中**：訊息列多一張建議卡片。三個條件同時成立才出現：back end 說遮罩的範本比較好（目前只有 HALCON）、板上有還沒設定比對區的樣本、程式在 Idle。卡片告訴使用者用 `M` 工具點一下端子；照樣可以按 F5 搜尋。

<!--STE-->
## Reason
HALCON builds a shape model from edges. A template from the whole body box also contains the edges of the pads next to the pin. On the real image, the teeth along a slot then get a score of 0.70. A mask from the M tool keeps the pin only. The teeth then get 0.48 and the recall goes from 80 percent to 97 percent. OpenCV gets about 100 false matches with the same mask. So only HALCON gets the advice. The user does not know this difference. The rule lives in the application layer. Each back end sets its own flag. The presenter only shows the card.
<!--/STE-->

**繁中**：HALCON 的 shape model 是邊緣。整個主體框建的範本連鄰居焊墊的邊也進去，真實影像上長槽的鋸齒就拿到 0.70；`M` 點端子的遮罩只留端子，鋸齒掉到 0.48、召回從 80% 到 97%（R07.8 的量測）。OpenCV 用同樣的遮罩反而多約 100 個誤報，所以只有 HALCON 要提醒。使用者原本不知道這個差別。規則放 application 層，每個 back end 自己填旗標（`Backend::traits`），Presenter 只負責顯示。

<!--STE-->
## Effect
The user sees the advice at a useful moment. The card shows after a new sample, after a switch to HALCON, and after an accept or a discard. The card names the back end, gives the reason, and has two buttons. The first button selects the sample and starts the M tool. The second button closes the card for these samples. A new sample shows the card again. A sample with a match region does not show the card. The search is never blocked.
<!--/STE-->

**繁中**：使用者在有用的時機看到建議：建完樣本、切到 HALCON、接受或捨棄之後。卡片寫出 back end 的名稱與原因，兩個按鈕：〔用 M 點端子〕選取樣本並切到 `M` 工具；〔關閉〕之後這些樣本不再提醒。再框一個新樣本會再提醒；設了比對區的樣本不提醒。搜尋從不被擋住。

## SOLID 對照

| 原則 | 本步驟如何落實 | 如何驗證 |
|---|---|---|
| S | `MatchAdvice` 只管「該提醒誰」；`matchAdviceCard` 只管文字；`AutoRoiPresenter` 只在 Idle 把結果變成卡片 | `tst_match_advice`（沒有 Qt）、`tst_messages`、`tst_presenter` |
| O | 新的 back end 要不要建議遮罩，只改它的工廠（`Backend::traits`），規則與 Presenter 不改 | 測試台的 `maskhint` back end 只多一個旗標就有卡片 |
| L | `ISearchEngineSink::useSearchEngine` 多一個參數，兩個實作（`AutoRoiPresenter`、測試的 `RecordingSink`）行為不變 | `tst_presenter` 的切換測試照舊通過 |
| I | `BackendTraits` 只有一個旗標；Presenter 只拿到 traits，不拿整個 `Backend` | 編譯：`AutoRoiPresenter` 不 include `SearchEngine.h` |
| D | 規則只依賴 `InspectionSession` 與 `BackendTraits`，不認識 HALCON | `check-deps` 0 violations |

## 如何驗證

- [x] 建置通過（`scripts/dev/test.ps1`，Release＋OpenCV＋HALCON＋Rust）
- [x] 測試通過：`tst_match_advice`（9 項）、`tst_messages`、`tst_presenter`（多 2 項）、其餘全部
- [x] `node tools/ste-lint.mjs` 無 error
- [x] `perf.md`：本步驟無效能指標（原因在那裡）
