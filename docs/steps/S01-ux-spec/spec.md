# S01 — UX 規格與可點擊原型：目的、原因、效果

> 規則：英文區塊用 ASD-STE100 風格（短句、主動語態、一詞一義）。繁中摘要放在每段之後。
> 檢查：`node tools/ste-lint.mjs --strict`（只檢查句長與被動語態，不是完整 STE 驗證）。
> 完整設計在 [`docs/ux/ux-spec.md`](../../ux/ux-spec.md)；本檔只講「為什麼」。

<!--STE-->
## Purpose
This step defines how the user works with the program.
It builds a clickable prototype before any Qt code.
The prototype walks through every screen state.
<!--/STE-->

**繁中**：先定義「使用者怎麼用」，並在寫任何 Qt 程式之前做出可點擊的原型。原型會真的走過每一個畫面狀態。

<!--STE-->
## Reason
A wrong screen layout costs more to fix after coding.
A prototype shows the problems in minutes.
The team finds a bad idea while it is cheap.
<!--/STE-->

**繁中**：版面寫完才發現錯，修正成本高；原型幾分鐘就能暴露問題。這一步實際抓到的問題：疊在綠板上的框對比不足、「接受後復原」的重做順序錯誤、框到兩個元件時提示誤導、無匹配訊息蓋住板上的料件（使用者實測時發現）。

<!--STE-->
## Effect
The team agrees on the screens, the states and the keys first.
Step S02 then builds a known design.
All colors pass the contrast checks.
The prototype ran with no invalid state move.
<!--/STE-->

**繁中**：團隊先對畫面、狀態與快捷鍵取得共識，S02 照已確認的設計做。顏色對比已驗算（`tools/check-contrast.mjs`，淺色 18、深色 18、疊圖 24 組全過）；原型的整套腳本測試（124 項）沒有出現非法的狀態轉移。**這些是設計層的驗證，不是效能量測**（S01 沒有 Qt 程式）。

<!--STE-->
## Users and tasks
The user is a recipe engineer for an AOI machine.
The engineer does four tasks.
First, teach one new part. Second, scan a region with the library.
Third, edit a component type. Fourth, check a whole batch fast.
<!--/STE-->

**繁中**：使用者是 AOI 配方工程師，四件事：教一個新元件、用樣本庫掃描一塊區域、維護元件類型、快速驗證整批結果。

<!--STE-->
## Board, component and box
A board holds components. A component holds boxes.
A component is one part on the board, such as U1. It has a name, a position and an angle.
The boxes do not belong to the component. They come from its component type.
A component type is one library sample, such as part 8371.
The type 8371 has one body box, a matrix of pin boxes and a window for each pin.
A change to the type updates every component of that type.
<!--/STE-->

**繁中**：Board › Component › Box 三層。元件是板上的一個零件（有名稱、位置、角度），**框不屬於元件，而是由它的「元件類型」推導**。類型就是樣本庫的一個條目，例如料號 8371 ＝ 1 個主體框 ＋ 1 個 Pin 矩陣 ＋ 每支腳 1 個窗。改類型，同類型的所有元件一起更新。理由與否決的替代方案見 [ADR-006](../../adr/006-sample-library-and-hierarchy.md)。

<!--STE-->
## Two ways to make boxes
In the first way, the user draws a box around one part. The program finds the same parts on the board.
In the second way, the user drags any region. The program matches the region with every library sample.
Then it lists each part it knows, with all of its boxes.
A part with no library sample gets a hint. The hint says: teach a new sample.
<!--/STE-->

**繁中**：流程 A（教樣本）：框一個元件，程式在整張板子找相同的。流程 B（區域掃描）：框任意區域，程式用樣本庫辨識，列出認得的每個元件與它的全部框；庫裡沒有的元件會提示「教一個新樣本」。流程 B 是使用者從影片提出的做法，與 A 並存。

<!--STE-->
## Matrix and linked windows
The user edits a component type in the type editor.
A pin group is a matrix. The user sets rows and columns. The program makes all the boxes at once.
A window links to a parent box. It keeps its size as a ratio of the parent.
When the parent changes, the window follows.
The user can edit one group or all boxes of the same kind. Every component of the type updates in one step.
<!--/STE-->

**繁中**：Pin 群組是矩陣，設列數與欄數就一次建立整排框；檢測窗連結到父框，大小用父框的比例表示，父框一改窗跟著變；編輯範圍可選「這個群組」或「此類型中相同種類的全部框」。原型驗證：Pin 寬 9 → 12，窗寬 12.6 → 16.8；列數改 12 得 24 支 Pin 與 24 個窗；儲存後板上 3 顆 IC 一起更新並可復原。

<!--STE-->
## Library and board copies
The library holds the master of each component type.
The board keeps its own copy of every type it uses.
A change to the library does not change the board by itself.
The user presses Update from the library to take the new version.
The user presses Add to the library to share a board copy.
An old program never changes without an action from the user.
<!--/STE-->

**繁中**：使用者決定（TRI 的 Merge 語意）：樣本庫是母版，板子保存自己用到的類型的**複本**。改庫不會偷偷改到舊板子；要同步只能按〔從樣本庫更新〕或〔存入樣本庫〕，兩者都能復原。刪除庫條目不影響已有的板子。

<!--STE-->
## Scan scope
A scan compares the region with library samples.
A big library makes the scan slow.
So the program first tries the types that the board already uses.
It widens to the whole library only when some parts match nothing.
The status bar shows how many samples it compared.
The user can also choose the whole library, or board types only.
<!--/STE-->

**繁中**（回應使用者問「樣本庫很多不是要比很久」）：預設先只比板子已有的類型，區域裡還有東西沒比到才擴大到整個庫；狀態列顯示「比對 N 個樣本」。原型的耗時是公式模擬，**只有「比對幾個樣本」是真的邏輯**；預篩選、模型快取、平行比對都還沒有量測（ADR-006 §7，S07 量）。

<!--STE-->
## Units
The library stores sizes in micrometers.
Each board image carries a scale in micrometers per pixel.
Then one library sample works on every image and every camera.
The prototype uses pixels to stay simple.
<!--/STE-->

**繁中**：使用者決定：樣本庫存 µm，板子影像帶 µm／像素的校正值，庫才能跨影像、跨相機使用。原型為了簡化用像素，**那是簡化，不是決定**。

<!--STE-->
## Reference documents
We checked the design against three documents from TRI.
They show a library for each package, a body box, pin boxes and linked windows.
They also show a name index for part numbers and a CAD record with five fields.
The five fields are name, X, Y, angle and type. The prototype shows the same five fields.
<!--/STE-->

**繁中**：使用者提供三份 TRI 文件（TR7500 Model-Library、元件庫缺陷類型表、TR7700 軟體手冊）。對照出：每種封裝一個庫、主體框、Pin 框、以 Link 連結的檢測窗、料號／別名的名稱索引、五欄位的 CAD 記錄（名稱、X、Y、角度、類型）。採用與延後的清單見 ADR-006。**TRI 沒有「用影像辨識類型」的流程**（它靠 CAD 的型號欄位），流程 B 是本專案的新增。

<!--STE-->
## State language
Each box state has its own color and its own line style.
Sample is a thick blue solid line. Found is a green solid line.
Inferred is an amber dashed line. Rejected is a red dotted line.
A component draws all its boxes in the style of its own state.
The user can read the state in gray scale.
Color alone never carries the meaning.
<!--/STE-->

**繁中**：Sample＝藍實線粗、Found＝綠實線、Inferred＝琥珀虛線、Rejected＝紅點線。狀態屬於元件，元件的所有框用同一個狀態的樣式畫；框的種類用寬度區分（主體、Pin、窗）。不只靠顏色，線型也不同（色盲友善）；原型的灰階開關可驗證，`check-contrast` 也會檢查線型不重複。

<!--STE-->
## Overlay colors
Boxes on the image use one fixed bright palette in both themes.
The image does not change with the theme.
Most boards are dark.
The light theme colors have a contrast of only 1.05 to 1.6 on a dark green board.
The bright colors pass 3 to 1 on green, blue and black boards.
<!--/STE-->

**繁中**：畫布上的框在兩個主題都用同一組亮色（`tokens.json` 的 `overlay`），因為影像不隨主題改變、多數板子偏暗。淺色主題的描邊色疊在深綠板上只有 1.05–1.6:1，全靠外框撐著。板子顏色是合成的參考色，**不是實測**。**使用者已確認採用**（2026-10-05）。

<!--STE-->
## Messages
Messages never cover the image.
The message bar sits under the canvas.
It holds hints, progress, no match, errors and toasts.
The canvas keeps its size in every state.
The user sees every part while a message stays on the screen.
<!--/STE-->

**繁中**：使用者實測原型時指出無匹配訊息蓋住板上的料件。新規則：所有要閱讀的訊息放在畫布**下方的訊息列**，不蓋在影像上；訊息列變高時畫布大小不變。原型已驗證各種無匹配原因、錯誤、進度、吐司的方塊都不與畫布相交。

<!--STE-->
## Feedback time
The program answers a click in 50 ms or less.
After 200 ms of waiting, it shows a busy sign.
After 1 s, it shows progress and a Cancel button.
Cancel returns the screen to idle in 100 ms or less.
The program drops the old result when it arrives.
The status bar always shows the back end, the time and the count.
<!--/STE-->

**繁中**：0.1 s／1 s／10 s 是人感受延遲的三個界線。取消時 UI 立刻回到 Idle，worker 不必立刻停（單次比對呼叫無法中途中斷），舊結果回來時丟棄。

<!--STE-->
## Undo
Every edit is a command.
A proposal and a type draft each have their own undo list.
Accept and discard are commands too.
Undo after accept returns the user to the proposal.
The user can always go back.
<!--/STE-->

**繁中**：三個範圍（提案、類型編輯草稿、文件）；接受、捨棄、儲存類型也是 Command；接受後復原回到 Preview。理由與否決的替代方案見 [ADR-002](../../adr/002-undo-model.md)；使用者已確認採用「回到 Preview」。

<!--STE-->
## Toolkit decision
The program uses Qt Widgets with QGraphicsView.
The user chose this option after we compared it with QML.
ADR-001 records the facts and the unproven points.
S02 measures the unproven points.
The presenters see only small view interfaces.
A later step can replace the view layer.
<!--/STE-->

**繁中**：使用者選了 Widgets + QGraphicsView（[ADR-001](../../adr/001-ui-toolkit.md)）。「5 000 框夠快」是假設，S02 用 U1、I1 驗證；沒有證據不推翻。Presenter 只看窄介面，所以日後換 View 技術不必改 Presenter。

<!--STE-->
## Interfaces for S04
S04 builds the presenters from the interface names in this step.
Each interface has five methods or fewer.
A view sees only the small interface it needs.
Zoom and pan stay inside the canvas.
They do not pass through a presenter.
<!--/STE-->

**繁中**：S04 照 `ux-spec.md` 第 12 節的介面名單實作（View 端 8 個、Controller 端 10 個，每個介面 ≤ 5 個方法）。縮放與平移是畫布自己的檢視狀態，不經過 Presenter，因為 I1 的幀時間不能多一層間接。

<!--STE-->
## Budgets
The user confirmed the UI budgets U1 to U4 and I1 to I6.
The numbers stay as proposed values until S02 measures them.
Three notes define the terms.
U3 covers threshold changes only.
I4 measures the time to idle on screen.
U4 splits the decode time from the paint time.
<!--/STE-->

**繁中**：使用者確認 U1–U4、I1–I6 的數字並接受三點定義：U3 只適用用快取重新過濾的參數（分數門檻）；I4 量「UI 回到 Idle」；U4 拆成解碼與首次繪製。數字仍是**提議值**，S02 校準。區域掃描的耗時隨樣本庫大小 N 成長，新預算等 S07 實測再提。

## SOLID 對照

| 原則 | 本步驟如何落實 | 如何驗證 |
|---|---|---|
| S | 每個 Presenter 一個責任（`AutoRoiPresenter`、`ComponentTreePresenter`、`ParamsPresenter`、`LibraryPresenter`、`TypeEditorPresenter`、`HistoryPresenter`）；縮放平移留在畫布；`BoxExpander` 只做幾何 | S04 的類別大小與單元測試；> 300 行要說明 |
| O | 新的框種類（Pad、Align…）＝新增 `BoxGroup` 種類與表單；新工具＝新增類別＋註冊 | S02、S04 的程式審查 |
| L | 所有 `ILibraryMatcher` 與 `IPatternSearcher` 都回傳同樣的候選＋分數＋原因（`ux-spec.md` 第 12.1 節）；Presenter 才能不分 back end | S09 契約測試 |
| I | 18 個介面各 ≤ 5 個方法；View 只看到自己需要的介面 | 本步驟：方法數逐一核對（見 `ux-spec.md` 第 12 節）；S04：編譯期 |
| D | Presenter 依賴介面與 use case，不認識 `QWidget`；具體 View 在 `app/` 組裝 | `tools/check-deps.mjs`（S03 起） |

## 如何驗證

- [x] 建置通過：**不適用（尚未建立）**，S01 沒有程式
- [x] 測試通過：**不適用（尚未建立）**。原型有頁內腳本測試，但它是瀏覽器 DOM，不是產品測試
- [x] `node tools/ste-lint.mjs --strict` 無 error
- [x] `node tools/check-contrast.mjs`：淺色 18、深色 18、疊圖 24 組全過；負向測試（壞的 token）會失敗
- [x] `perf.md`：U/I 預算已確認；全部**未量測**（沒有程式）
- [x] `diagram.md` 的 12 張 Mermaid 圖用 mermaid 11 的解析器逐一解析，語法都合法（只驗語法，沒有渲染）；解析器對壞語法會失敗（已驗）
- [x] 原型互動驗證，對**最終版**整套重跑：124 項斷言全過，另有比對範圍的專屬測試約 20 項（其中一項因我的測試時序寫錯而重寫），主控台無錯誤。涵蓋：流程 A、B；接受／捨棄／儲存類型與三個範圍的復原；取消與忙碌 200 ms、進度 1 s 的出現時機；9 種無匹配原因（其中 onlyOne、lowContrast 沒有單獨斷言）；三種錯誤；類型編輯器的矩陣、相依窗、同類型範圍、主體框不可刪、有修改時 Esc 不丟棄；把手 24 px 點擊區（距中心 10 px 命中、13 px 沒命中）；CAD 清單；訊息不蓋在影像上；庫與板子的複本語意（改庫不影響板子、從樣本庫更新、存入樣本庫、刪庫不影響板子，皆可復原）；預覽時〔接受〕〔接受並存入樣本庫…〕〔捨棄〕按鈕（只用滑鼠走完整個流程）；比對範圍（板子類型先比、找不到再擴大、32 個樣本的庫上比對 1 個 vs 32 個樣本）；淺／深色、灰階、繁中／英文、樣式實驗室；沒有非法的狀態轉移
- 如實說明：真實的滑鼠、鍵盤、滾輪抽查是在第一版原型做的（拖曳樣本框、S 鍵、滾輪縮放）。新版以合成事件測試，並用截圖目視檢查了編輯器、掃描結果與訊息列。開發過程中有多項斷言失敗，絕大多數是測試腳本自己的錯（期望值寫錯、順序錯、沒先選取群組），改正後重測；真的缺陷有 2 個：接受後復原再重做的順序，以及畫面快取鍵漏看群組內容（編輯樣本庫後「與樣本庫不同」的提示不會出現），都已修正
