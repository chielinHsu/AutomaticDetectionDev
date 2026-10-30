# ADR-042 HALCON 建好樣本後在訊息列建議用 `M` 點端子的遮罩當比對區：back end 的特性旗標、application 的規則、Idle 的卡片

- 日期：2026-10-29
- 決定者：使用者（R07 收尾時決定做這件後續）；Claude Code (Claude Fable 5.1) 提出做法與實作
- 狀態：採用（補充 ADR-028；R10）

## 背景

R07.8 在使用者的真實影像（`20261014_4677_AdvancePin_LeftTopRegion_Proj\FovImages\0_0_HA.tif`，交錯排列的端子陣列、
右下角一個長槽；樣本中心 (477, 684)、100×100）量測比對區對兩個 back end 的影響（數字在 R07 `plan.md` 的「R07.8 的量測」）：

| 範本 | HALCON 真端子（≥ 0.70） | HALCON 鋸齒 | OpenCV 真端子 | OpenCV 鋸齒 |
|---|---|---|---|---|
| 整個主體框（使用者的設定） | 855／1068 | 最高 0.70 | 1081／1112 | 最高 0.71 |
| `M` 點端子自動擷取的遮罩（外擴 0） | **1059／1087**，離格 0 | 最高 **0.48** | 1191／1209，約 100 個離格誤報 | 4 |

HALCON 的 shape model 是邊緣：整框時樣本裡鄰居焊墊的邊也進了模型，長槽鋸齒的邊剛好拼得出 0.70；只留端子的遮罩之後
鋸齒掉到 0.48、召回從 80% 到 97%。OpenCV 的 NCC 用遮罩反而多約 100 個不在晶格上的誤報，所以 OpenCV 維持整框。
使用者現在不知道有這個差別：建完樣本就按 F5。ADR-028 的 `M` 工具已經存在，缺的是「什麼時候該用它」的提示。

現況（從程式碼確認）：`ComponentType::match` 預設 ＝ 整個主體框、灰階（`MatchSettings::isDefault`）；訊息列（ADR-008）
同一時間一種內容：進度列 > 卡片（`AutoRoiPresenter` 經 `IMessageView::showCard`，只在 Preview、NoMatch、Error）> 提醒卡片
（`BackendPresenter` 經 `IBackendView::showNotice`：HALCON 的 runtime 授權提醒，按〔關閉〕才收）> 提示。操作步驟 dock
（ADR-027）的規則在 application 的 `StepGuide`，Presenter 只在變了時交給 View。

## 決定

1. **哪個 back end 建議遮罩是 back end 自己的特性**：`Backend` 多 `traits`（`BackendTraits::suggestsMatchMask`，預設 false），
   由 adapter 的工廠填（`makeHalconBackend` 填 true；OpenCV、假 back end 不填）。規則與畫面只看旗標，不比對 `"halcon"`
   之類的字串；之後有別的 back end 也適用遮罩時只改它的工廠。`SearchEngine` 帶著 `Backend`，所以特性跟著 engine 走。
2. **規則在 application**（ADR-011 的做法）：`application/interaction/MatchAdvice`。`pending(session, traits)`：旗標是 true、
   狀態是 `Idle`、板上每個樣本的類型 `match.isDefault()`、而且沒按過〔關閉〕 → 列出（依樣本在板上的順序）。
   `dismiss(samples)` 記類型 id（同類型一起不再提醒；再框一個新樣本是新類型，會再提醒）；`reset()` 開新影像時清掉。
   類型只改了特徵（邊緣）也算處理過（使用者動過比對區的設定）。`tst_match_advice` 9 項逐條驗證。
3. **觸發時機 ＝ 回到 `Idle` 而且條件成立**：建完樣本（`ReleaseSample`）、切到 HALCON（`useSearchEngine` 之後重新發布）、
   接受、捨棄、取消之後都算。不選「按 F5 之前才提醒」：那時使用者已經決定要搜尋，提醒會變成打斷；不選「只在切到 HALCON 時」：
   使用者多半先選好 back end 再框樣本。同一個樣本只提醒一次：按〔關閉〕記住它；設了比對區自然不再符合條件。
4. **顯示用訊息列的卡片**（`IMessageView::showCard`，`AutoRoiPresenter` 擁有；`MessageTexts::matchAdviceCard`）：標題
   「{back end}：建議用遮罩當比對區」、說明原因（整框會把相鄰的焊墊與長槽的鋸齒也當成端子，實測約 0.70）、做法列出還沒設定的
   樣本數，按鈕〔用 M 點端子〕（選取第一個還沒設定的樣本、切到 `M` 工具，卡片留著）與〔關閉〕；`Esc` 在沒有選取時也關掉。
   **不擋搜尋**：F5 照舊，搜尋中進度列蓋過它，回到 `Idle` 再評估。特性經 `ISearchEngineSink::useSearchEngine` 的第三個參數交給
   `AutoRoiPresenter`（啟動時 `AppPresenters` 用 `setBackendTraits` 交一次），和錯誤卡片用的名稱同一條路。
5. **不自動擷取**：樣本的中心不一定在端子上（使用者的樣本裡上緣與左下角是鄰居焊墊的邊），點哪裡是使用者的判斷；
   自動擷取的外擴仍是 `M` 工具的預設 4 px（R07.8 的量測用 0，差別在 `plan.md`），由選取資訊表單調。

## 否決的替代方案

- **用提醒卡片（`showNotice`）**：語意最像（「back end 的提醒」），但 `MessageBar` 只有一個提醒的位置、歸 `BackendPresenter`，
  HALCON 的 runtime 授權提醒會和它互相覆蓋；要支援多個提醒就得改 `MessageBar` 與兩個 Presenter 的擁有權。
- **只改提示列的那一句**（`ui.sample.hintIdle` 加一段）：一行放不下原因，吐司 4 秒會蓋掉，而且 OpenCV 時那一句要不一樣。
- **放在操作步驟 dock 的「搜尋」那一步底下**：dock 可以關掉、預設寬度窄，而且它是「做到哪一步」不是「該怎麼做比較好」。
- **在 Presenter 裡 `if (backendId == "halcon")`**：違反開放封閉（新 back end 要改 Presenter），也和 `BackendLabels` 只放顯示名稱的做法衝突。
- **按 F5 時先問要不要先設比對區**：對話框打斷流程，違反「不擋搜尋」。
- **自動在樣本中心擷取遮罩**：見決定 5。

## 後果

- `Backend` 多一個欄位、`ISearchEngineSink::useSearchEngine` 多一個參數（實作者：`AutoRoiPresenter`、`tst_presenter` 的 `RecordingSink`）。
- `tst_presenter` 的測試台多一個 back end `maskhint`（「MaskHint (test)」，和 fake 同一個假世界與探針，只差旗標），註冊在 `other` 之前，
  所以 `fallback()`（可用、不是目前的最後一個）仍是 `other`；可用的 back end 從 4 個變 5 個（`withoutANoticeThereIsNoCardAndNoMark`）。
- Idle 有卡片時提示列被蓋住（訊息列的規則），所以卡片的說明寫明「不改也可以直接按 F5」；按〔關閉〕或 `Esc` 提示回來。
- OpenCV 不提醒：之後若 OpenCV 的遮罩改好了（例如 ADR-028 的邊緣模式配矩形），只要在它的工廠把旗標打開。
- 文案多 4 個 key（`ui.match.advice.*`）；`tst_messages` 檢查兩種語言都有。

## 驗證（2026-10-29）

見 R10 的 `changelog.md`（`test.ps1` 預設建置、`check-deps`、`ste-lint`、實際執行程式的結果）。
