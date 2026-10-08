// 每個 Qt Test 執行檔都編進這個檔（AiInspTool/qmake/aoi_test.pri）：在 main 之前把測試需要的環境設好，
// 讓 Qt Creator 直接執行測試時，和 scripts/dev/test.ps1 得到一樣的環境（CMake 時代由 CTest 的 ENVIRONMENT 設定）。
// 已經有設定的環境變數不覆寫：例如要在真的視窗上量測時，設 QT_QPA_PLATFORM=windows 再執行。
//   QT_QPA_PLATFORM=offscreen        不開視窗、不搶焦點
//   QT_QPA_FONTDIR=%WINDIR%/Fonts    offscreen 平台不會自己找到字型（Qt 不附字型），文字量測與截圖會是空的
// 放在程式這邊（不放在靜態程式庫）：沒有被參考的目的檔會被連結器丟掉，這裡的初始化就不會執行。
#include <QByteArray>
#include <QtGlobal>

namespace {

void setIfUnset(const char* name, const QByteArray& value) {
    if (!qEnvironmentVariableIsSet(name)) qputenv(name, value);
}

const bool kTestEnvironmentReady = [] {
    setIfUnset("QT_QPA_PLATFORM", "offscreen");
    const QByteArray windows = qgetenv("WINDIR");
    if (!windows.isEmpty()) setIfUnset("QT_QPA_FONTDIR", windows + "/Fonts");
    return true;
}();

}  // namespace
