#pragma once

#include <QImage>

#include "domain/image_view.h"

// infrastructure/image_qt：Qt Core ＋ Qt Gui（QImage），不用 Qt Widgets、不用視覺函式庫（docs/architecture.md）。
// QImage 與 ImageView 之間的轉換（S06，ADR-014）。ImageView → QImage 的「顯示用」版本在 presentation 的 ImageItem::wrap
// （presentation 不可以 include infrastructure）；兩邊的格式對照表要一致，tests/infra/tst_image_qt 會比對。
namespace aoi::infra {

// QImage → ImageView。
//  - 零複製（view 的 owner 持有一份 QImage，隱式共享只加參照計數）：
//      Grayscale8 → Gray8、Grayscale16 → Gray16、RGB32／ARGB32 → Bgrx32、RGB888 → Rgb24、BGR888 → Bgr24。
//  - 其他格式先轉一次（每次呼叫 countImageCopy）：索引色與單色的色表是「0..255 依序的灰階」（索引 ＝ 灰階值，
//    Qt 的 isGrayscale 的定義）→ Grayscale8，其餘 → RGB32（包括只有 16 階灰的色表、16 bit 彩色：換成 8 bit，有損；
//    預先乘過 alpha 的格式：還原成不乘 alpha 的顏色）。
//    已知的例外：16 階灰的色表的 BMP，OpenCV 給 Gray8、這裡給 Bgrx32（像素值相同），見 tst_image_sources_agree。
//  - 之後別人改了原本的 QImage，Qt 會讓改的那一方先分離（detach），view 看到的像素不變。
//  - 空影像回傳無效的 view。
ImageView fromQImage(const QImage& image, double umPerPixel = 0.0);

// ImageView → QImage：零複製、唯讀（有人寫入時 Qt 先深複製）。QImage（與它的複本）活著，view 的 owner 就活著。
// 五種格式都對得上：Gray8 → Grayscale8、Gray16 → Grayscale16、Rgb24 → RGB888、Bgr24 → BGR888、Bgrx32 → RGB32。
// 無效的 view 回傳空的 QImage。
QImage wrapAsQImage(const ImageView& view);

// 正規格式（IImageSource 的契約）：Gray8、Gray16、Bgrx32 原樣；其餘（Rgb24、Bgr24）轉成 Bgrx32，算一次複製。
// 影像來源在交出 view 之前呼叫它，所以所有來源對同一個檔案給出同樣的格式。
QImage toCanonicalQImage(QImage image);

}  // namespace aoi::infra
