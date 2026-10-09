# ADR-004 兩個 back end 的差異：契約一樣、準確度各有上限；哪些差異接受、哪些算 bug

- 日期：2026-10-09
- 決定者：Claude Code (Claude Opus 5.5) 在 S09 依實測提案（S09 計畫預留的編號）。界線與接受的清單是提案並採用；**兩項等使用者決定**：分數門檻要不要依 back end 分開（產品的預設）、HALCON 的 `nativeGray16` 要不要當預設。
- 狀態：採用（界線與接受的差異）；兩項待使用者決定（見「待決定」）；**HALCON 端的 S09 並排量測未做**（dongle 讀不到，使用者 2026-10-09 決定之後再測，見 §3）

## 背景

- 兩個 back end 用不同的方法找元件：OpenCV 是強度式的 NCC，由粗到細（ADR-016）；HALCON 是邊緣式的 shape model（ADR-017）。兩者實作同一組 port（`ITemplateBuilder`、`IPatternSearcher`、`ILibraryMatcher`），use case、晶格、domain 都沒有為任何一個改過。
- S07、S08 各寫了一份契約的草稿與端到端測試，S08 已看到差異：合成板子上 HALCON 的位置誤差 p95 0.48 px、OpenCV 0.11 px（強雜訊時差最多、反光時 HALCON 較好）；強雜訊時 HALCON 的角度最大約 0.9°；**真實影像 HA 上 HALCON 用一根針腳當樣本只找到少數針腳**（分數 < 0.70，S08 `perf.md` §4）；兩邊的分數不是同一把尺。
- S09 的任務：用同一組契約測試跑每個 back end（Liskov），記錄 NCC 與 shape model 的行為差異（光照、反光、缺件），並分出哪些差異可以接受、哪些是 bug。

## 決定

### 1. 界線：什麼是 bug、什麼是接受的差異

- **bug**：違反 port 標頭寫的契約、ADR-010 的座標與角度慣例，或 `tests/contract/ContractTolerances.h` 的準確度上限——也就是 `tests/contract/` 的任何一列失敗。修在那個 back end 的 adapter（不在 use case、domain，不放寬容差來遮掉）。
- **接受的差異**：兩邊都通過契約，但同一個輸入給不同的結果，原因是方法本身（看灰階還是看邊緣）。條件：記在下面的清單（現象、量到的數字、為什麼接受、使用者在哪裡看得到），而且 A/B 比較畫面看得見它。
- **容差可以因 back end 而不同**，但只能在 `ContractTolerances.h` 的 `profileOf` 裡、附依據；現在只有一項（D2）。

### 2. 接受的差異

| # | 差異 | OpenCV | HALCON | 為什麼接受 | 在哪裡看得到 |
|---|---|---|---|---|---|
| D1 | **分數不是同一把尺** | NCC（灰階的相關係數，−1..1，負的截成 0） | 邊緣方向的一致度（0..1） | 兩種方法的定義不同，不能換算；合成板子上同一個門檻 0.70 兩邊都 100% 找到 | A/B 視窗的門檻兩邊各一個；說明頁可以拖門檻 |
| D2 | **強雜訊時的角度** | 最大 0.28°（S09 契約測試，雜訊 12＋模糊 1） | 最大 0.90°（同一塊板子；48×28 的範本角落位移 0.41 px） | shape model 只用邊緣點，雜訊大的小元件邊緣點少；角落位移仍在 T6b 的 0.5 px 內。容差：HALCON 強雜訊時用「角落位移 ≤ 0.5 px」 | `tst_contract_end_to_end` 的 log；A/B 視窗的「角度差」 |
| D3 | **雜訊下的位置誤差** | p95 0.11 px（S08 T6） | p95 0.48 px（S08 T6） | 都在 T6b 的 0.5 px 內 | A/B 視窗的「位置差」 |
| D4 | 搜尋的角度範圍比範本的寬 | `InvalidArgument` | `InvalidArgument` | port 沒有規定；假 back end 照樣過濾。use case 永遠用同一組參數建範本與搜尋，看不到 | `tst_contract_searcher` 的 `theAngleRangeIsRespected` 印出各自的行為 |
| D5 | 光照不均、低對比、重模糊、大片反光、強雜訊、缺件 | 見 §3 | 見 §3 | 見 §3 | A/B 視窗；`perf_ab` |

### 3. 光照、反光、缺件：S09 的並排量測

`perf_ab`：12 種 5 MP 合成板子，use case 端到端，兩邊的門檻都是 0.70（數字在 `docs/steps/S09-contract-ab/perf.md` §2）。**HALCON 那一邊沒有量**：2026-10-09 12:39 起讀不到 dongle（錯誤 2300），使用者決定 dongle 相關的部分記下來、等之後再測。下表的 HALCON 欄是 S08 已經量過的同類情境（`docs/steps/S08-halcon/perf.md` 的 T6 逐板），S09 的新情境（重模糊、40% 反光、低對比、光照不均、雜訊 25）HALCON 的欄是「未測」。

| 情境 | OpenCV（S09） | HALCON | 讀法 |
|---|---|---|---|
| 光照不均（左邊 0.35 倍） | 100%；位置 p95 0.021 px；分數 0.997 | 未測 | NCC 每個視窗自己正規化，幾乎不受影響；shape model 看邊緣方向，預期也不受影響，但左邊的邊緣對比只剩 0.35 倍，可能低於它的對比門檻——要量 |
| 低對比（主體與底差 12 灰階） | 100%；0.116 px；分數 0.922 | 未測 | 同上，對比門檻是 shape model 的風險 |
| 重模糊（半徑 4） | 100%；0.028 px；0.995 | 未測 | 模糊讓邊緣變寬變弱，shape model 的邊緣點變少 |
| 雜訊 12、模糊 2 | 100%；0.047 px（S08 T6：最大 0.069） | S08：100%，位置最大 **1.36 px** | 雜訊是 HALCON 最弱的地方（D2、D3） |
| 雜訊 25 | 100%；0.095 px；分數 0.946 | 未測 | |
| 10% 反光 | 100%；位置 p95 **0.490 px**；最低分 0.842 | S08：100%，位置最大 **0.142 px**（OpenCV 0.503） | **反光是 OpenCV 最弱的地方**：亮斑把相關的峰拉偏約 0.5 px；shape model 只看邊緣，亮斑的影響小。兩邊都在 T6b 的 0.5 px 內（OpenCV 剛好在邊上） |
| 40% 反光 | 100%；0.490 px；分數中位數 **0.839** | 未測 | 分數下降 0.16：門檻設高（例如 0.85）時 OpenCV 會開始漏；這是 D1「分數不是同一把尺」最容易被看見的情況 |
| 10% 缺件 | 100%（754／754）；缺件都被推算；0.015 px | S08（5% 缺件）：100%，0.100 px | 缺件由晶格推算，兩邊相同（晶格在 domain，不在 back end） |
| 傾斜 3° | 100%；0.026 px | S08：100%，0.138 px | |
| 12 bit | 100%；0.016 px；峰值記憶體多約 5 MB | S08：100%，0.100 px（轉 8 bit；`nativeGray16` 見「待決定」） | |

**目前的結論（只到這裡為止）**：在合成板子上，兩邊都 100% 找到；差別在位置的穩定度——**雜訊時 OpenCV 好、反光時 HALCON 好**（S08 的數字），都在 T6b 內，所以接受（D3）。光照不均、低對比、重模糊這三種 shape model 可能比較吃虧的情況，HALCON 那一邊要等 dongle 才能下結論；量到之後若召回率低於 T6a（99%）或位置超過 T6b（0.5 px），就不是「接受的差異」，要回到 adapter 的參數（`HalconSearchOptions` 的對比門檻）處理，並在這裡記錄。

### 4. 契約測試找到、已經修掉的

- **假 back end 不檢查角度範圍**：收到 NaN 或顛倒的範圍時「成功、找不到」，OpenCV 與 HALCON 回 `InvalidArgument`。改假 back end（範本建立器與搜尋器都檢查），和另外兩個一致。這是 Liskov 的差異：同一個錯誤的輸入，use case 會把「找不到」當成樣本的問題給建議，方向錯。

### 5. 沒有解決、記下來的

- **真實影像 HA**（S08）：HALCON 用一根針腳當樣本只找到少數針腳（分數 < 0.70），OpenCV 找到大部分（也框到焊墊的邊緣）。推測：樣本框裡有周圍焊墊的輪廓（shape model 要求這些邊緣也在），或 'auto' 的對比門檻。S09 沒有重量：使用者的真實影像不在原本的位置（`D:\Benano\20260922_*` 已不存在；`D:\Benano\20261007_4677_AdvancePin_Proj\` 有新的影像，是不是同一批要使用者確認）。有了影像之後，用 A/B 視窗在 HA 上看「只有 OpenCV」的那些元件，HALCON 的分數是多少。

## 待決定（使用者）

1. **分數門檻要不要依 back end 分開**（產品的預設，D1）。現在兩個 back end 都用 0.70（`InspectionSession::Config::defaultThreshold`）。A/B 視窗已經可以分開設、看差異；要不要把「每個 back end 一個預設門檻」做進主視窗（會動到 application 的設定），等使用者看過真實影像的 A/B 再決定。
2. **HALCON 的 `nativeGray16` 要不要當預設**（S08 留給 S09）。S08 的實測：5 MP 12 bit 的種子搜尋，轉成 8 bit（預設）中位數 47.9–71.4 ms，直接用 16 bit（`uint2`，零複製）26.9–37.2 ms，841 個元件都找到；`tst_halcon_backend` 的 `gray8IsSearchedWithoutCopiesAndGray16WithOne` 也確認準確度相同。**建議改成預設**，但 HALCON 的對比門檻（'auto'）在 16 bit 上用的是 16 bit 的灰階值，真實的 12 bit 影像沒有驗證過；`HalconSearchOptions` 的預設值在交接單的「不要動」清單裡，所以 S09 沒有改。

## 證據

- 契約測試：`tests/contract/` 三個執行檔，每個 back end 一列（數字見 `docs/steps/S09-contract-ab/perf.md` §1）。
- 變異測試（4 個，`docs/steps/S09-contract-ab/spec.md`）：拿掉 OpenCV 的視窗過濾、假 back end 的排序反過來、假 back end 的角度檢查拿掉、OpenCV 的角度正負號反過來——都在對的那個 back end 的那一列失敗。
- S07 `perf.md`、S08 `perf.md` 的並排（T2、T4、T6）。

## 否決的替代方案

- **把兩邊的分數換算成同一把尺**（例如對 HALCON 的分數做查表）：兩個定義測的東西不同（灰階的形狀、邊緣的方向），沒有一個換算在所有情況都對；換算錯了比分開更難察覺。
- **一個容差給兩個 back end（取寬的那個）**：OpenCV 的角度在強雜訊時好 3 倍，用 HALCON 的上限會讓 OpenCV 的退步看不出來。
- **假 back end 也跑準確度的測試**：它不看影像，結果來自 FakeGrid；跑了只會永遠 SKIPPED 或永遠失敗。它的世界由 `tst_fake_backend` 驗證。
- **差異只寫在文件，不做 A/B 畫面**：S09 計畫要求「差異看得見」；真實影像上的差異（HA）只有在畫面上一個一個看才知道原因。

## 後果

- 新的 back end 只要在組裝根註冊，契約測試自動多一列；要不同的準確度上限，就在 `profileOf` 加一項並在這份 ADR 寫理由。
- 契約測試在每一種建置都有：只有一個 back end 的建置（`-NoOpenCv`、`-NoHalcon`）裡，缺的那個是 SKIPPED（附原因），不是消失。
- HALCON 讀不到授權時（錯誤 2300，S09 開發途中又發生），HALCON 的列全部 SKIPPED；`scripts/dev/test.ps1` 最後列出「有跳過項目的測試」——跳過不是通過。
