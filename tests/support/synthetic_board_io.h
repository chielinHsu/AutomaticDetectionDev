#pragma once

#include <QString>

#include "synthetic_board.h"

// 把合成板子存成檔案（測試用，Qt）：<dir>/<name>.png（影像）與 <dir>/<name>.json（標準答案）。
// 給 S07 以後要「從檔案讀」的測試、效能量測與人工檢查用。PNG 是無損的：讀回來的像素與記憶體裡的相同。
namespace aoi::test {

// 成功回傳空字串；失敗回傳原因。
QString saveSyntheticBoard(const SyntheticBoard& board, const QString& dir, const QString& name);

}  // namespace aoi::test
