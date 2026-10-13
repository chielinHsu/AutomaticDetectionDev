# S10 — 解說影片分鏡（60 秒：我們怎麼證明程式夠快）

實際影片與語音合成是選配，需使用者同意。分鏡本身永遠要交付。
旁白：STE 風格英文（句子 ≤ 20 字）。字幕：繁中。

| # | 秒 | 畫面 | 旁白（STE 英文） | 繁中字幕 |
|---|---|---|---|---|
| 1 | 0–8 | 兩個終端機並排：同一個程式、同一台機器，I1 的 p95 一次 14 ms、一次 21 ms | A fast program is a program that we can prove is fast. But the same program gives a different number each time. | 快，要能證明。可是同一個程式每次量出來的數字都不一樣。 |
| 2 | 8–20 | 錄屏：`.\scripts\dev\perf.ps1`；畫面上一個個量測跑過（perf_matrix 的格子、perf_e2e 的 T5），最後印出 PASS | One command runs every measurement. It writes one record with the machine, the load and the clock speed. | 一個指令跑完所有量測，寫成一份紀錄，連機器、負載、時脈都記下來。 |
| 3 | 20–34 | `diagram.md` 第 3 節的決策流程圖逐步亮起：預算 → 已知 → 基準線的範圍 → 一半以上變慢 | The gate compares each number with its budget and with the range of the baseline. A known slow item is amber. If half of the numbers are slower, the machine was slower, not the program. | 閘門把每個數字和預算、和基準線的範圍比。已知的超標是琥珀色。一半以上都變慢，是機器慢了，不是程式。 |
| 4 | 34–46 | 錄屏：`perf.ps1 -InjectDelayMs 60 -Filter perf_matrix`；報表的判定變成 FAIL，回歸的清單列出矩陣的格子 | We make the program slower on purpose. The gate fails and shows which metric is slower. | 故意讓程式慢 60 ms，閘門失敗，而且說出是哪個指標。 |
| 5 | 46–56 | `explainer.html`：拖「回歸的門檻」滑桿，判定從 FAIL 變 PASS；篩選 back end，矩陣的顏色條（範本、種子、晶格、驗證） | The report is interactive. Move the limit and see the verdict change. Filter by back end and see where the time goes. | 報表可以互動：拖門檻看判定怎麼變；篩 back end，看時間花在哪個階段。 |
| 6 | 56–60 | `perf.md` 的表：T5 的數字對 300 ms 的預算、雜訊的範圍 | The numbers of this step are in perf.md. The next step makes the slowest part faster. | 數字在 perf.md。下一步，把最慢的那一塊變快。 |

## 製作備註

- 色彩語言與 UI 一致：樣本＝藍色實線、找到＝綠色實線、推算＝琥珀色虛線、拒絕＝紅色點線。報表裡：通過＝綠、失敗＝紅、已知＝琥珀、無法判定＝灰；階段的顏色：範本＝灰、種子＝藍、晶格＝琥珀、驗證＝綠。不只靠顏色，判定也有文字。
- 第 4 段的注入延遲是 harness 的功能（`AOI_PERF_INJECT_DELAY_MS`），不是改產品的程式；錄的時候要讓觀眾看到結果檔上的 `injectedDelayMs`，說明這種結果不能當基準線。
- 第 1 段的兩個數字取自 `docs/steps/S02-ui-shell/perf.md`（I1 13 次的範圍 14.1–20.7 ms）。
