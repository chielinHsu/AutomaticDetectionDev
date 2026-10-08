# 合成板子。AOI_SYNTHETIC_REFERENCE_HASH 是「不傾斜的參考板子」的像素雜湊：改了產生器，這個值就會變，
# 確定是刻意的修改之後再更新它（測試的 log 會印出新的值）。它只證明「同樣的輸入給同樣的輸出」，不證明輸出是對的
# （對不對由同一個測試裡的像素與標準答案檢查負責）。

TARGET = tst_synthetic_board
include(infra_tests.pri)
SOURCES += $$AOI_ROOT/tests/infra/tst_synthetic_board.cpp
DEFINES += AOI_SYNTHETIC_REFERENCE_HASH=\\\"447862f70216e47b\\\"
