#pragma once

#include <QMetaType>

#include "domain/image_view.h"

// aoi::ImageView 在 Qt 的訊號與槽裡傳遞（S06：MainWindow::imageOpened → AutoRoiPresenter::onImageOpened）。
// QSignalSpy 與佇列連線要認得這個型別；宣告必須在任何 moc 產生的程式用到它之前，
// 所以每個在訊號、槽或 Q_OBJECT 類別裡用到 ImageView 的標頭都 include 這個檔，不直接 include domain/image_view.h。
Q_DECLARE_METATYPE(aoi::ImageView)
