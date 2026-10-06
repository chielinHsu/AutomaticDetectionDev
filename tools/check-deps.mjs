#!/usr/bin/env node
// 依賴方向檢查：掃 src/ 底下每個檔案的 #include，依 docs/architecture.md 的依賴表判斷，違規即失敗（結束碼 1）。
//
// 用法：
//   node tools/check-deps.mjs                 掃這個 repo 的 src/
//   node tools/check-deps.mjs --root <dir>    掃 <dir>/src/（測試用：fixture 目錄）
//   node tools/check-deps.mjs --self-test     對 tests/core/fixtures/deps/* 驗證「該報的有報、不該報的沒報」
//
// 規則表（RULES）必須和 docs/architecture.md 的「分層與依賴規則」一致；改一邊就要改另一邊。
// 只看 src/。tests/、AiInspTool/（qmake 專案，不在五層之內）、build/ 不掃。
//
// 限制（誠實說明）：
//  - 只看 #include 文字，不展開巨集，也不看 CMake 的 target_link_libraries（兩者一起才完整；CMake 擋連結，本工具擋標頭）。
//  - 分不清 Qt Core 與 Qt Widgets 的裸標頭名（#include <QString>）時，用下面的 QT_CORE_HEADERS 白名單：
//    只有列在白名單的才算 Core。新的 Core 標頭要加進白名單（寧可誤報、不要漏報）。
//  - #if 0 區塊裡的 #include 仍會被掃到。
import { existsSync, readFileSync, readdirSync, statSync } from 'node:fs';
import { dirname, join, posix, relative, resolve, sep } from 'node:path';
import { fileURLToPath } from 'node:url';

const REPO_ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');

// ---------------------------------------------------------------------------------------------
// 規則表。每一層（或每個 adapter）可以 include 哪幾「類」：
//   std        C++／C 標準庫
//   domain / application / presentation / app   對應的層
//   own        同一個 adapter 自己（只有 infrastructure/* 用得到）
//   qt-core    白名單裡的 Qt Core 標頭
//   qt-gui     白名單裡的 Qt Gui 標頭（QImage 等；S06 起 infrastructure/image_qt 可以用）
//   qt         任何 Qt 標頭（含 Widgets；也包含 qt-core 與 qt-gui）
//   opencv / halcon   視覺函式庫
//   other      其他第三方或平台標頭（例如 <windows.h>）
// 其他類別一律禁止。
// ---------------------------------------------------------------------------------------------
const RULES = {
  domain: { allow: ['std', 'domain'], why: 'domain only uses the C++ standard library' },
  application: { allow: ['std', 'domain', 'application'], why: 'application only depends on domain (and the standard library)' },
  'infrastructure/backend_fake': { allow: ['std', 'domain', 'application', 'own'], why: 'the fake back end uses the standard library only' },
  'infrastructure/backend_opencv': { allow: ['std', 'domain', 'application', 'own', 'opencv', 'other'], why: 'the OpenCV adapter may use OpenCV, never Qt Widgets, HALCON or another adapter' },
  'infrastructure/backend_halcon': { allow: ['std', 'domain', 'application', 'own', 'halcon', 'other'], why: 'the HALCON adapter may use HALCON, never Qt Widgets, OpenCV or another adapter' },
  'infrastructure/image_qt': { allow: ['std', 'domain', 'application', 'own', 'qt-core', 'qt-gui', 'other'], why: 'image_qt may use Qt Core and Qt Gui (QImage), never Qt Widgets or a vision library' },
  'infrastructure/storage': { allow: ['std', 'domain', 'application', 'own', 'qt-core', 'other'], why: 'storage may use Qt Core, never Qt Widgets or a vision library' },
  'infrastructure/perf': { allow: ['std', 'domain', 'application', 'own', 'qt-core', 'other'], why: 'perf may use Qt Core, never Qt Widgets or a vision library' },
  'infrastructure/threading': { allow: ['std', 'domain', 'application', 'own', 'qt-core', 'other'], why: 'threading may use Qt Core, never Qt Widgets or a vision library' },
  presentation: { allow: ['std', 'domain', 'application', 'presentation', 'qt', 'other'], why: 'presentation may use Qt, never a vision library or infrastructure' },
  app: { allow: ['std', 'domain', 'application', 'presentation', 'app', 'qt', 'other', 'infrastructure'], why: 'app assembles everything but must not use a vision library directly' },
};

const KNOWN_TOP_LEVEL = new Set(['domain', 'application', 'infrastructure', 'presentation', 'app']);

const STD_HEADERS = new Set(`algorithm any array atomic barrier bit bitset charconv chrono codecvt compare complex concepts condition_variable
coroutine deque exception execution expected filesystem flat_map flat_set format forward_list fstream functional future generator initializer_list
iomanip ios iosfwd iostream istream iterator latch limits list locale map mdspan memory memory_resource mutex new numbers numeric optional ostream
print queue random ranges ratio regex scoped_allocator semaphore set shared_mutex source_location span spanstream sstream stack stacktrace
stdexcept stdfloat stop_token streambuf string string_view strstream syncstream system_error thread tuple type_traits typeindex typeinfo
unordered_map unordered_set utility valarray variant vector version
cassert cctype cerrno cfenv cfloat cinttypes climits clocale cmath csetjmp csignal cstdarg cstddef cstdint cstdio cstdlib cstring ctime cuchar cwchar cwctype
assert.h ctype.h errno.h fenv.h float.h inttypes.h limits.h locale.h math.h setjmp.h signal.h stdarg.h stddef.h stdint.h stdio.h stdlib.h string.h time.h wchar.h wctype.h`
  .split(/\s+/).filter(Boolean));

// 已知屬於 Qt Gui 模組的標頭（裸名稱；S06）。QtGui/ 前綴的一律算 Gui。不在兩份白名單裡的 Q 開頭標頭一律算 qt（寧可誤報）。
const QT_GUI_HEADERS = new Set(`QImage QImageReader QImageWriter QImageIOHandler QColor QColorSpace QRgb QPixmap QBitmap QPainter QPainterPath
QPen QBrush QTransform QPolygon QPolygonF QRegion QFont QFontMetrics QFontMetricsF QGuiApplication QIcon QKeySequence QPalette`.split(/\s+/).filter(Boolean));

// 已知屬於 Qt Core 模組的標頭（裸名稱）。QtCore/ 前綴的一律算 Core。
const QT_CORE_HEADERS = new Set(`QObject QCoreApplication QString QStringList QStringView QByteArray QByteArrayView QChar QLatin1StringView QList QMap QHash QSet
QVector QVariant QDateTime QDate QTime QElapsedTimer QTimer QThread QThreadPool QRunnable QMutex QMutexLocker QReadWriteLock QWaitCondition QSemaphore
QAtomicInt QAtomicPointer QFile QDir QFileInfo QSaveFile QTemporaryFile QTemporaryDir QIODevice QTextStream QDataStream QBuffer QUrl QUuid QSettings
QStandardPaths QJsonDocument QJsonObject QJsonArray QJsonValue QJsonParseError QLocale QTranslator QSharedPointer QWeakPointer QScopedPointer
QMetaObject QMetaType QPointer QRect QRectF QPoint QPointF QSize QSizeF QLine QLineF QMargins QMarginsF QCommandLineParser QCommandLineOption
QProcess QRegularExpression QCryptographicHash QRandomGenerator QSysInfo QLoggingCategory QDebug QtGlobal QtNumeric QtLogging QAnyStringView QMimeData QEvent
QTextCodec QCborValue QPromise QFuture QFutureWatcher QtConcurrent QDeadlineTimer QOverload`.split(/\s+/).filter(Boolean));

// ---------------------------------------------------------------------------------------------

function walk(dir, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (/\.(h|hpp|hh|hxx|inl|c|cc|cpp|cxx|mm)$/i.test(name)) out.push(p);
  }
  return out;
}

const toPosix = (p) => p.split(sep).join('/');

// 把註解換成空白（保留換行，行號不變），避免註解裡的 #include 被當成真的。
function stripComments(text) {
  return text
    .replace(/\/\*[\s\S]*?\*\//g, (m) => m.replace(/[^\n]/g, ' '))
    .replace(/\/\/[^\n]*/g, (m) => ' '.repeat(m.length));
}

// 檔案屬於哪一層（或哪個 adapter）。srcRel：相對於 src/ 的 posix 路徑。
function layerOf(srcRel) {
  const parts = srcRel.split('/');
  const top = parts[0];
  if (!KNOWN_TOP_LEVEL.has(top)) return { error: `unknown layer '${top}' (src/ may only contain: ${[...KNOWN_TOP_LEVEL].join(', ')})` };
  if (top !== 'infrastructure') return { layer: top };
  if (parts.length < 3) return { error: 'files under src/infrastructure/ must live in an adapter folder' };
  const adapter = `infrastructure/${parts[1]}`;
  if (!RULES[adapter]) return { error: `unknown adapter '${adapter}' (add a rule to tools/check-deps.mjs and docs/architecture.md)` };
  return { layer: adapter };
}

// 一個 include 屬於哪一類。回傳 { kind, layer?, adapter?, detail }。
function classify(includeText, isQuoted, fileDir, srcRoot) {
  if (isQuoted) {
    // 先找相對於這個檔案的，再找相對於 src/ 的（專案慣例：從 src/ 起算，例如 "domain/geometry.h"）。
    const candidates = [resolve(fileDir, includeText), resolve(srcRoot, includeText)];
    for (const c of candidates) {
      if (existsSync(c) && statSync(c).isFile()) {
        const rel = toPosix(relative(srcRoot, c));
        if (rel.startsWith('..')) return { kind: 'local', detail: includeText };
        const l = layerOf(rel);
        if (l.error) return { kind: 'unresolvable', detail: `${includeText} → ${l.error}` };
        return { kind: 'project', layer: l.layer, detail: includeText };
      }
    }
    if (/^(moc_.*\.cpp|ui_.*\.h|qrc_.*\.cpp)$/.test(posix.basename(includeText))) return { kind: 'local', detail: includeText };
    return { kind: 'unresolvable', detail: includeText };
  }
  const first = includeText.split('/')[0];
  if (/^opencv2?$/i.test(first)) return { kind: 'opencv', detail: includeText };
  if (/^halcon/i.test(first) || /^hdevengine/i.test(first)) return { kind: 'halcon', detail: includeText };
  if (/^Qt[A-Z]/.test(first) && includeText.includes('/')) {
    const kind = first === 'QtCore' ? 'qt-core' : first === 'QtGui' ? 'qt-gui' : 'qt';
    return { kind, detail: includeText };  // <QtWidgets/QWidget> 這種有模組前綴的寫法
  }
  if (/^Q[A-Za-z0-9_]*$/.test(includeText)) {
    const kind = QT_CORE_HEADERS.has(includeText) ? 'qt-core' : QT_GUI_HEADERS.has(includeText) ? 'qt-gui' : 'qt';
    return { kind, detail: includeText };
  }
  if (/^qt[a-z]*\//.test(includeText) || /^q[a-z0-9_]+\.h$/.test(includeText)) return { kind: 'qt', detail: includeText };
  if (STD_HEADERS.has(includeText)) return { kind: 'std', detail: includeText };
  return { kind: 'other', detail: includeText };
}

// 規則：layerName 這一層可不可以 include 這個類別？回傳 null（可以）或違規說明。
function judge(layerName, inc) {
  const rule = RULES[layerName];
  const allow = new Set(rule.allow);
  const isAdapter = layerName.startsWith('infrastructure/');
  switch (inc.kind) {
    case 'local':
    case 'std':
      return null;
    case 'unresolvable':
      return `cannot resolve the include "${inc.detail}"`;
    case 'project': {
      if (inc.layer === layerName) return null;                                  // 自己的檔案
      if (isAdapter && inc.layer.startsWith('infrastructure/')) {
        return `${layerName} must not include another adapter (${inc.layer})`;
      }
      const target = inc.layer.startsWith('infrastructure/') ? 'infrastructure' : inc.layer;
      if (allow.has(target)) return null;
      return `${layerName} must not include ${target} ("${inc.detail}"): ${rule.why}`;
    }
    case 'qt-core':
      return allow.has('qt-core') || allow.has('qt') ? null : `${layerName} must not include Qt (<${inc.detail}>): ${rule.why}`;
    case 'qt-gui':
      return allow.has('qt-gui') || allow.has('qt') ? null : `${layerName} must not include Qt Gui (<${inc.detail}>): ${rule.why}`;
    case 'qt':
      return allow.has('qt') ? null : `${layerName} must not include Qt Widgets/Gui or an unknown Qt header (<${inc.detail}>): ${rule.why}`;
    case 'opencv':
      return allow.has('opencv') ? null : `${layerName} must not include OpenCV (<${inc.detail}>): ${rule.why}`;
    case 'halcon':
      return allow.has('halcon') ? null : `${layerName} must not include HALCON (<${inc.detail}>): ${rule.why}`;
    case 'other':
      return allow.has('other') ? null : `${layerName} must not include <${inc.detail}> (not the standard library): ${rule.why}`;
    default:
      return `unclassified include "${inc.detail}"`;
  }
}

function scan(root) {
  const srcRoot = join(root, 'src');
  const files = walk(srcRoot);
  const violations = [];
  for (const file of files) {
    const srcRel = toPosix(relative(srcRoot, file));
    const where = layerOf(srcRel);
    if (where.error) {
      violations.push({ file: `src/${srcRel}`, line: 1, includes: '', rule: where.error });
      continue;
    }
    const lines = stripComments(readFileSync(file, 'utf8')).split('\n');
    lines.forEach((text, idx) => {
      const m = /^\s*#\s*include\s*(?:<([^>]+)>|"([^"]+)")/.exec(text);
      if (!m) return;
      const quoted = m[2] !== undefined;
      const target = quoted ? m[2] : m[1];
      const inc = classify(target, quoted, dirname(file), srcRoot);
      const problem = judge(where.layer, inc);
      if (problem) violations.push({ file: `src/${srcRel}`, line: idx + 1, includes: target, rule: problem });
    });
  }
  return { files: files.length, violations };
}

function report(result, label = '') {
  for (const v of result.violations) {
    console.error(`${v.file}:${v.line}: ${v.includes ? `#include ${v.includes} — ` : ''}${v.rule}`);
  }
  console.log(`check-deps${label}: ${result.files} files scanned, ${result.violations.length} violation(s)`);
}

// --self-test：fixtures 各有 src/ 與 expect.json（{ "violations": [{ "file", "line", "includes" }] }），
// 比對「集合完全相同」：該報的沒報、不該報的卻報了，都算失敗。
function selfTest() {
  const base = join(REPO_ROOT, 'tests', 'core', 'fixtures', 'deps');
  if (!existsSync(base)) {
    console.error(`check-deps --self-test: fixture folder not found: ${base}`);
    return 1;
  }
  let failures = 0;
  let cases = 0;
  for (const name of readdirSync(base).sort()) {
    const dir = join(base, name);
    if (!statSync(dir).isDirectory()) continue;
    cases++;
    const expected = JSON.parse(readFileSync(join(dir, 'expect.json'), 'utf8')).violations;
    const actual = scan(dir).violations;
    const key = (v) => `${v.file}:${v.line}:${v.includes}`;
    const want = new Set(expected.map(key));
    const got = new Set(actual.map(key));
    const missing = [...want].filter((k) => !got.has(k));
    const extra = [...got].filter((k) => !want.has(k));
    if (missing.length || extra.length) {
      failures++;
      console.error(`FAIL fixture '${name}'`);
      missing.forEach((k) => console.error(`  not reported (should be): ${k}`));
      extra.forEach((k) => console.error(`  reported (should not be): ${k}`));
    } else {
      console.log(`ok   fixture '${name}': ${expected.length} expected violation(s) reported exactly`);
    }
  }
  if (cases === 0) {
    console.error('check-deps --self-test: no fixtures found');
    return 1;
  }
  console.log(`check-deps --self-test: ${cases} fixture(s), ${failures} failure(s)`);
  return failures ? 1 : 0;
}

const args = process.argv.slice(2);
if (args.includes('--self-test')) process.exit(selfTest());
const rootIdx = args.indexOf('--root');
const root = rootIdx >= 0 ? resolve(args[rootIdx + 1]) : REPO_ROOT;
const result = scan(root);
report(result);
process.exit(result.violations.length ? 1 : 0);
