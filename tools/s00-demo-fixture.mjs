#!/usr/bin/env node
// S05：與 S00 示範頁的晶格演算法交叉驗證用的 fixture。
//
// 直接執行 docs/steps/S00-overview/explainer.html 裡「原本的」JavaScript，不是抄一份：
// 從頁面取出「tab 2: pipeline demo」那一段（rng、fit、compute…），在 Node 裡配上假的控制項執行，
// 把每一組參數（缺件、雜訊、傾斜、邊驗證邊重新擬合）的輸入點與結果寫成 JSON。
// C++ 的 tests/domain/tst_lattice_crosscheck 讀同一份 JSON：同樣的點，C++ 要算出同樣的晶格、同樣的找到／推算。
//
// 用法：
//   node tools/s00-demo-fixture.mjs           寫 tests/domain/fixtures/s00-demo-lattice.json
//   node tools/s00-demo-fixture.mjs --check   重新產生並與檔案比對；不同（S00 頁面的演算法改了、或 fixture 過期）就以 1 結束
import { readFileSync, writeFileSync, existsSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '..');
const PAGE = 'docs/steps/S00-overview/explainer.html';
const OUT = 'tests/domain/fixtures/s00-demo-lattice.json';

function fail(message) {
  console.error(`s00-demo-fixture: ${message}`);
  process.exit(1);
}

// 取出示範頁的演算法（到畫圖的程式之前為止），用假的 $() 提供控制項。
const html = readFileSync(join(ROOT, PAGE), 'utf8');
const startMarker = '/* ---------- tab 2: pipeline demo ---------- */';
const endMarker = 'const box = ';
const start = html.indexOf(startMarker);
const end = html.indexOf(endMarker, start);
if (start < 0 || end < 0) fail(`cannot find the pipeline demo code in ${PAGE}`);
const code = html.slice(start + startMarker.length, end);
const controls = { miss: { value: '0' }, noise: { value: '0' }, tilt: { value: '0' }, prog: { checked: true }, mp: { value: '5' } };
const $ = (selector) => {
  const control = controls[selector.replace('#', '')];
  if (!control) fail(`the demo code asks for an unknown control ${selector}`);
  return control;
};
const demo = new Function('$', `${code}\nreturn { compute, fit, COLS, ROWS, P, Q, O, SEEDS };`)($);

const model = (m) => ({ o: [m.o.x, m.o.y], u: [m.u.x, m.u.y], v: [m.v.x, m.v.y] });

const cases = [];
for (const miss of [0, 2, 5]) {
  for (const noise of [0, 3, 6]) {
    for (const tilt of [0, 8]) {
      for (const prog of [true, false]) {
        controls.miss.value = String(miss);
        controls.noise.value = String(noise);
        controls.tilt.value = String(tilt);
        controls.prog.checked = prog;
        const S = demo.compute();
        const present = S.pins.filter((p) => p.present).map((p) => ({ i: p.i, j: p.j, x: p.x, y: p.y }));
        const fullFit = demo.fit(present);
        if (!fullFit) fail(`the demo fit failed for miss=${miss} noise=${noise} tilt=${tilt}`);
        cases.push({
          miss, noise, tilt, prog,
          pins: S.pins.map((p) => [p.i, p.j, p.x, p.y, p.present ? 1 : 0]),
          seeds: S.seedPts.map((p) => [p.i, p.j]),
          window: [S.WX, S.WY],
          seedModel: model(S.seedModel),
          finalModel: model(S.model),
          fullFit: model(fullFit),
          // 依驗證的順序：[i, j, 找到 1／推算 0, x, y]（推算的位置是最後的模型算的）
          results: S.results.map((r) => [r.i, r.j, r.state === 'found' ? 1 : 0, r.x, r.y]),
        });
      }
    }
  }
}

const header = {
  source: PAGE,
  generator: 'tools/s00-demo-fixture.mjs',
  note: 'Produced by the original JavaScript of the S00 demo page. Numbers are full-precision doubles. Do not edit by hand: run the generator.',
  grid: { cols: demo.COLS, rows: demo.ROWS, pitchX: demo.P, pitchY: demo.Q, origin: [demo.O.x, demo.O.y] },
};
const text = `${JSON.stringify(header).slice(0, -1)},"cases":[\n${cases.map((c) => JSON.stringify(c)).join(',\n')}\n]}\n`;

const outPath = join(ROOT, OUT);
if (process.argv.includes('--check')) {
  if (!existsSync(outPath)) fail(`${OUT} does not exist: run node tools/s00-demo-fixture.mjs`);
  if (readFileSync(outPath, 'utf8') !== text) fail(`${OUT} is out of date (the S00 demo changed?): run node tools/s00-demo-fixture.mjs`);
  console.log(`s00-demo-fixture: ${OUT} matches the S00 demo (${cases.length} cases)`);
} else {
  writeFileSync(outPath, text);
  console.log(`s00-demo-fixture: wrote ${OUT} (${cases.length} cases, ${text.length} bytes)`);
}
