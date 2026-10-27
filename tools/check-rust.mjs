#!/usr/bin/env node
// Rust crate 的規則檢查（ADR-031；scripts/dev/test.ps1 的 rust_manifest）：
//   1. rust/aoi_domain_rs/Cargo.toml 沒有任何依賴（[dependencies]、[dev-dependencies]、[build-dependencies]、
//      [target.*.dependencies]、[workspace.dependencies] 都要是空的）：domain 只用標準庫，對應 check-deps 對 C++ 的規則。
//   2. 自我測試：有依賴的 fixture（tests/core/fixtures/rust/Cargo-with-dependency.toml）一定要被抓到。
//   3. rustc 實際要求的系統程式庫（--print native-static-libs）和 AiInspTool/qmake/aoi_rust.pri 的 AOI_RUST_SYSLIBS 相同
//      （換 Rust 版本時可能會變；少了就連結失敗，多了沒事但要知道）。/defaultlib:msvcrt 是 CRT 的選擇，另外處理（aoi_target.pri）。
//
// 用法：node tools/check-rust.mjs [--manifest <Cargo.toml>] [--no-native-libs]
//   --manifest  只檢查這一份的依賴（不做 2、3），有依賴時結束碼 1
// 結束碼：0 通過、1 違規、2 執行有問題（找不到 cargo 等）。
import { spawnSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import { homedir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const crateManifest = join(root, 'rust', 'aoi_domain_rs', 'Cargo.toml');
const violationFixture = join(root, 'tests', 'core', 'fixtures', 'rust', 'Cargo-with-dependency.toml');
const rustPri = join(root, 'AiInspTool', 'qmake', 'aoi_rust.pri');

// 依賴表的段落：[dependencies]、[dev-dependencies]、[build-dependencies]、[target.<cfg>.dependencies]、
// [workspace.dependencies]，以及 [dependencies.<名稱>] 這種一個依賴一段的寫法。
const DEPENDENCY_SECTION = /^\[\s*(?:target\.[^\]]+\.|workspace\.)?(?:dev-|build-)?dependencies(?:\.[^\]]+)?\s*\]$/;

export function findDependencies(text) {
    const found = [];
    let section = null;
    for (const [index, raw] of text.split(/\r?\n/).entries()) {
        const line = raw.replace(/#.*$/, '').trim();
        if (!line) continue;
        if (line.startsWith('[')) {
            section = DEPENDENCY_SECTION.test(line) ? line : null;
            // [dependencies.foo]：這一段本身就是一個依賴。
            if (section && /dependencies\.[^\]]+\]$/.test(line)) found.push(`line ${index + 1}: ${line}`);
            continue;
        }
        if (section && /^[A-Za-z0-9_.\-"']+\s*=/.test(line)) found.push(`line ${index + 1}: ${section} ${line}`);
    }
    return found;
}

function checkManifest(path) {
    const deps = findDependencies(readFileSync(path, 'utf8'));
    for (const d of deps) console.log(`  ${path}: ${d}`);
    return deps;
}

function findCargo() {
    if (process.env.AOI_CARGO && existsSync(process.env.AOI_CARGO)) return process.env.AOI_CARGO;
    if (process.env.CARGO_HOME && existsSync(join(process.env.CARGO_HOME, 'bin', 'cargo.exe'))) return join(process.env.CARGO_HOME, 'bin', 'cargo.exe');
    const rustup = join(homedir(), '.cargo', 'bin', process.platform === 'win32' ? 'cargo.exe' : 'cargo');
    return existsSync(rustup) ? rustup : 'cargo';
}

function runCargoForNativeLibs() {
    const args = ['rustc', '--manifest-path', crateManifest, '--target-dir', join(root, 'build', 'cargo-checks'), '--release', '--lib',
        '--crate-type', 'staticlib', '--locked', '--offline', '--', '--print', 'native-static-libs'];
    const cargo = findCargo();
    // cargo 把訊息寫到 stderr，而且會重播快取的編譯訊息：沒有重新編譯時也印得出這一行。
    const r = spawnSync(cargo, args, { encoding: 'utf8' });
    if (r.error) throw r.error;
    const text = `${r.stdout}${r.stderr}`;
    if (r.status !== 0) throw new Error(`cargo rustc failed (exit ${r.status}):\n${text}`);
    return text;
}

function checkNativeLibs() {
    const text = runCargoForNativeLibs();
    const line = text.split(/\r?\n/).find((l) => l.includes('native-static-libs:'));
    if (!line) throw new Error(`rustc did not print native-static-libs:\n${text}`);
    const fromRustc = line.split('native-static-libs:')[1].trim().split(/\s+/).filter((t) => !/^\/defaultlib:msvcrt$/i.test(t));
    const pri = readFileSync(rustPri, 'utf8').split(/\r?\n/).find((l) => /^AOI_RUST_SYSLIBS\s*=/.test(l));
    if (!pri) throw new Error(`AOI_RUST_SYSLIBS not found in ${rustPri}`);
    const fromPri = pri.split('=')[1].trim().split(/\s+/);
    const same = fromRustc.length === fromPri.length && fromRustc.every((t, k) => t.toLowerCase() === fromPri[k].toLowerCase());
    console.log(`  rustc native-static-libs: ${fromRustc.join(' ')}`);
    console.log(`  aoi_rust.pri AOI_RUST_SYSLIBS: ${fromPri.join(' ')}`);
    return same;
}

async function main() {
    const argv = process.argv.slice(2);
    const at = argv.indexOf('--manifest');
    if (at >= 0) {
        const deps = checkManifest(resolve(argv[at + 1]));
        console.log(deps.length ? `FAIL: ${deps.length} dependencies (the crate must use only the Rust standard library)` : 'OK: no dependencies');
        return deps.length ? 1 : 0;
    }
    let failed = false;
    const deps = checkManifest(crateManifest);
    console.log(deps.length ? `FAIL: ${crateManifest} has ${deps.length} dependencies (ADR-031: standard library only)` : `OK: ${crateManifest} has no dependencies`);
    failed ||= deps.length > 0;

    const caught = checkManifest(violationFixture);
    console.log(caught.length >= 2 ? `OK: self-test caught ${caught.length} dependencies in the fixture` : 'FAIL: self-test did not catch the fixture\'s dependencies');
    failed ||= caught.length < 2;
    const clean = findDependencies('[package]\nname = "x"\n[dependencies]\n# none\n[features]\ndefault = []\n');
    if (clean.length) { console.log('FAIL: self-test flagged a manifest without dependencies'); failed = true; }

    if (!argv.includes('--no-native-libs')) {
        const same = checkNativeLibs();
        console.log(same ? 'OK: AOI_RUST_SYSLIBS matches rustc' : 'FAIL: update AOI_RUST_SYSLIBS in AiInspTool/qmake/aoi_rust.pri to the rustc list');
        failed ||= !same;
    }
    return failed ? 1 : 0;
}

main().then((code) => process.exit(code), (e) => { console.error(e.message ?? e); process.exit(2); });
