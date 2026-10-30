# R08 目前狀態

- 日期：2026-10-29
- 目前入口：Socket 區域 CAD 註冊，R 選任意區域放開直接產生 CAD，F5 全圖；不需要範本、不呼叫 NCC。
- R08.9–R08.11 完成：Socket 水平端點結構辨識、左右兩個共享 CAD profile、Body＋Window、統一尺寸、單框 copy-on-write、人工覆寫保留、JSON v3 存讀。
- 專項 19 passed／0 failed／0 skipped；真實 ROI 24 個，全圖 686 個、2 類；局部先估計後沿用 47×26 px。沒有完整人工標準答案，不宣稱辨識率。
- Release 與 Debug（無 OpenCV／HALCON）建置成功；執行包移除 Qt 開發 PATH 後 --help exit 0、示範存活 3 秒，ZIP CRC 通過。
- 完整回歸初跑 66/67；新增 R07 一維弱候選測資只有五個強種子，晶格選成 240 px，沒有要驗的缺格。補足連續種子並斷言 60 px 後單項 3/0/0，未改 R07 演算法。最終全量 67/67；7 組既有 skip、2 個 XFAIL，見 build/r08-region/regression-final.log。
- 整體 PARTIALLY VERIFIED：既有 skip／XFAIL 及真實圖未標註保留。
- 交付：build/SocketTeaching-RegionCAD-20261029.zip；操作 docs/steps/R08-socket-teaching/region-workflow.md；ADR-036。
- R08.8 由另一聊天負責，保留其表格與拖移修復，細項狀態不代改。
- 舊亮塊／NCC API 保留相容測試，舊執行包不含目前區域流程。
- 不 stage／commit／push；保留 main 上其他 session 的工作。
- 使用者已授權下一步 R09：PCB 多類元件 OBB 與實例分割，從標註／訓練開始。
