// S06：ImageSourceChain（幾個 IImageSource 依優先順序組合）的規則。來源是假的：只看規則，不解碼。
#include <QtTest>
#include <memory>
#include <string>
#include <vector>

#include "QtTestPrinters.h"
#include "application/image/ImageSourceChain.h"
#include "domain/image_buffer.h"

using namespace aoi;

namespace {

// 認得某些副檔名，load 回傳事先設定的結果，並記下被呼叫的次數。
class ScriptedSource final : public IImageSource {
public:
    ScriptedSource(std::string name, std::vector<std::string> suffixes, Result<ImageView> result)
        : name_(std::move(name)), suffixes_(std::move(suffixes)), result_(std::move(result)) {}

    bool canRead(std::string_view path) const override {
        for (const std::string& s : suffixes_) {
            if (path.size() >= s.size() && path.substr(path.size() - s.size()) == s) return true;
        }
        return false;
    }
    Result<ImageView> load(std::string_view) override {
        ++calls;
        return result_;
    }
    int calls = 0;

private:
    std::string name_;
    std::vector<std::string> suffixes_;
    Result<ImageView> result_;
};

ImageView imageOfWidth(int width) {
    return ImageBuffer(width, 1, PixelFormat::Gray8).view();
}

Error error(ErrorCode code, const char* message) { return makeError(code, message); }

struct Chain {
    ImageSourceChain chain;
    ScriptedSource* first = nullptr;
    ScriptedSource* second = nullptr;
    Chain(Result<ImageView> a, Result<ImageView> b, std::vector<std::string> aSuffixes = {".png", ".tif"},
          std::vector<std::string> bSuffixes = {".png"}) {
        auto sa = std::make_unique<ScriptedSource>("A", std::move(aSuffixes), std::move(a));
        auto sb = std::make_unique<ScriptedSource>("B", std::move(bSuffixes), std::move(b));
        first = sa.get();
        second = sb.get();
        chain.add(std::move(sa));
        chain.add(std::move(sb));
    }
};

}  // namespace

class TstImageChain : public QObject {
    Q_OBJECT

private slots:
    void theFirstSourceThatSucceedsWins() {
        Chain c(imageOfWidth(11), imageOfWidth(22));
        const Result<ImageView> r = c.chain.load("board.png");
        QVERIFY(r.ok());
        QCOMPARE(r.value().width, 11);
        QCOMPARE(c.first->calls, 1);
        QCOMPARE(c.second->calls, 0);
    }

    void aSourceThatCannotDecodeHandsOverToTheNext() {
        Chain c(error(ErrorCode::FormatInvalid, "A cannot decode"), imageOfWidth(22));
        const Result<ImageView> r = c.chain.load("board.png");
        QVERIFY(r.ok());
        QCOMPARE(r.value().width, 22);
        QCOMPARE(c.first->calls, 1);
        QCOMPARE(c.second->calls, 1);
    }

    // 讀不到檔案時換解碼器也沒有用：立刻回報，不問下一個。
    void fileProblemsStopTheChain_data() {
        QTest::addColumn<int>("code");
        QTest::newRow("NotFound") << int(ErrorCode::NotFound);
        QTest::newRow("IoFailed") << int(ErrorCode::IoFailed);
        QTest::newRow("Internal") << int(ErrorCode::Internal);
    }
    void fileProblemsStopTheChain() {
        QFETCH(int, code);
        Chain c(error(static_cast<ErrorCode>(code), "A says no"), imageOfWidth(22));
        const Result<ImageView> r = c.chain.load("board.png");
        QVERIFY(!r.ok());
        QCOMPARE(static_cast<int>(r.error().code), code);
        QCOMPARE(c.second->calls, 0);
    }

    // 只問認得這個檔名的來源：.tif 只有 A 認得。
    void onlySourcesThatKnowTheFileTypeAreAsked() {
        Chain c(error(ErrorCode::FormatInvalid, "A cannot decode"), imageOfWidth(22));
        const Result<ImageView> r = c.chain.load("board.tif");
        QVERIFY(!r.ok());
        QCOMPARE_CODE(r.error().code, ErrorCode::FormatInvalid);
        QCOMPARE(c.first->calls, 1);
        QCOMPARE(c.second->calls, 0);
    }

    // 沒有來源認得這個檔名：交給最後一個（後備）試一次，例如沒有副檔名、但內容是 PNG 的檔案。
    void anUnknownFileTypeGoesToTheLastSourceOnce() {
        Chain c(imageOfWidth(11), imageOfWidth(22));
        const Result<ImageView> r = c.chain.load("board.dat");
        QVERIFY(r.ok());
        QCOMPARE(r.value().width, 22);
        QCOMPARE(c.first->calls, 0);
        QCOMPARE(c.second->calls, 1);
    }

    void whenNobodyCanDecodeTheReasonsAreListedInOrder() {
        Chain c(error(ErrorCode::FormatInvalid, "A: bad header"), error(ErrorCode::FormatInvalid, "B: bad data"));
        const Result<ImageView> r = c.chain.load("board.png");
        QVERIFY(!r.ok());
        QCOMPARE_CODE(r.error().code, ErrorCode::FormatInvalid);
        const std::string& m = r.error().message;
        QVERIFY2(m.find("A: bad header") != std::string::npos && m.find("B: bad data") != std::string::npos, m.c_str());
        QVERIFY(m.find("A: bad header") < m.find("B: bad data"));
    }

    void canReadIsTrueWhenAnySourceKnowsTheFileType() {
        Chain c(imageOfWidth(1), imageOfWidth(1));
        QVERIFY(c.chain.canRead("x.tif"));
        QVERIFY(c.chain.canRead("x.png"));
        QVERIFY(!c.chain.canRead("x.dat"));
        QCOMPARE(c.first->calls + c.second->calls, 0);  // canRead 不載入
    }

    void anEmptyChainReadsNothing() {
        ImageSourceChain chain;
        chain.add(nullptr);  // 空指標被忽略
        QCOMPARE(chain.size(), std::size_t{0});
        QVERIFY(!chain.canRead("x.png"));
        const Result<ImageView> r = chain.load("x.png");
        QVERIFY(!r.ok());
        QCOMPARE_CODE(r.error().code, ErrorCode::FormatInvalid);
    }
};

QTEST_APPLESS_MAIN(TstImageChain)
#include "tst_image_chain.moc"
