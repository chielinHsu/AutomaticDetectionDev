#!/usr/bin/env node
// Verifies docs/ux/tokens.json against WCAG 2.1 contrast rules.
//
// Checks:
//   1. Every pair in contrastRules meets its minimum ratio (text 4.5, graphic 3) in every theme.
//      Rules with scope "overlay" check the theme-independent overlay palette against the sample board colors.
//   2. The four state line styles differ from each other, so state never depends on color alone.
//   3. The recorded contrastResults match the values computed now (no stale numbers).
//
// Usage: node tools/check-contrast.mjs [tokens.json]   (default: docs/ux/tokens.json)
//        node tools/check-contrast.mjs --write         rewrite contrastResults, then check
// Exit code 1 when a check fails.
import { readFileSync, writeFileSync } from 'node:fs';

const args = process.argv.slice(2);
const write = args.includes('--write');
const path = args.find((a) => !a.startsWith('--')) ?? 'docs/ux/tokens.json';
const tokens = JSON.parse(readFileSync(path, 'utf8'));

function channel(v) {
  const c = v / 255;
  return c <= 0.03928 ? c / 12.92 : ((c + 0.055) / 1.055) ** 2.4;
}
function luminance(hex) {
  const m = /^#([0-9a-f]{6})$/i.exec(hex);
  if (!m) throw new Error(`Bad color "${hex}": use #RRGGBB`);
  const n = parseInt(m[1], 16);
  return 0.2126 * channel((n >> 16) & 255) + 0.7152 * channel((n >> 8) & 255) + 0.0722 * channel(n & 255);
}
function ratio(a, b) {
  const [hi, lo] = [luminance(a), luminance(b)].sort((x, y) => y - x);
  return (hi + 0.05) / (lo + 0.05);
}
function lookup(theme, dotted) {
  const root = theme === 'overlay' ? tokens.overlay : tokens.themes[theme];
  const v = dotted.split('.').reduce((o, k) => (o == null ? o : o[k]), root);
  if (typeof v !== 'string') throw new Error(`Missing token "${dotted}" in theme "${theme}"`);
  return v;
}

const computed = {};
let failures = 0;
for (const theme of [...Object.keys(tokens.themes), 'overlay']) {
  computed[theme] = [];
  for (const rule of tokens.contrastRules) {
    const applies = rule.scope === 'overlay' ? theme === 'overlay' : theme !== 'overlay';
    if (!applies) continue;
    for (const [fg, bg] of rule.pairs) {
      const r = Math.round(ratio(lookup(theme, fg), lookup(theme, bg)) * 100) / 100;
      const pass = r >= rule.min;
      computed[theme].push({ kind: rule.kind, fg, bg, min: rule.min, ratio: r, pass });
      if (!pass) {
        failures++;
        console.log(`FAIL ${theme}: ${fg} on ${bg} = ${r} (need ${rule.min}, ${rule.kind})`);
      }
    }
  }
}

// Check 2: the four state styles must differ from each other in line style and dash pattern.
const seen = new Map();
for (const name of ['sample', 'found', 'inferred', 'rejected']) {
  const s = tokens.stateStyle[name];
  const key = `${s.lineStyle}|${s.dash.join(',')}|${s.width}`;
  if (seen.has(key)) {
    failures++;
    console.log(`FAIL line style of "${name}" equals "${seen.get(key)}" (state would depend on color alone)`);
  }
  seen.set(key, name);
}
const dashed = ['inferred', 'rejected'].map((n) => tokens.stateStyle[n].dash.join(','));
if (dashed[0] === dashed[1]) {
  failures++;
  console.log('FAIL inferred and rejected use the same dash pattern');
}

// Check 3: recorded results must match (or be rewritten).
if (write) {
  tokens.contrastResults = computed;
  writeFileSync(path, `${format(tokens)}\n`);
  console.log(`Wrote contrastResults to ${path}`);
} else if (JSON.stringify(tokens.contrastResults) !== JSON.stringify(computed)) {
  failures++;
  console.log('FAIL contrastResults is stale. Run: node tools/check-contrast.mjs --write');
}

for (const theme of Object.keys(computed)) {
  const rows = computed[theme];
  const lowest = rows.reduce((m, r) => (r.ratio / r.min < m.ratio / m.min ? r : m));
  console.log(`${theme}: ${rows.length} pairs, ${rows.filter((r) => !r.pass).length} failing, tightest ${lowest.fg} on ${lowest.bg} = ${lowest.ratio} (min ${lowest.min})`);
}
console.log(`Contrast check: ${failures} failures.`);
process.exit(failures ? 1 : 0);

// JSON writer that keeps flat objects and primitive arrays on one line.
function isScalar(x) {
  return x === null || typeof x !== 'object';
}
function isFlat(v) {
  if (Array.isArray(v)) return v.every(isScalar);
  return Object.values(v).every((x) => isScalar(x) || (Array.isArray(x) && x.every(isScalar)));
}
function inlineJson(v) {
  if (isScalar(v)) return JSON.stringify(v);
  if (Array.isArray(v)) return `[${v.map(inlineJson).join(', ')}]`;
  const entries = Object.entries(v).map(([k, x]) => `${JSON.stringify(k)}: ${inlineJson(x)}`);
  return entries.length ? `{ ${entries.join(', ')} }` : '{}';
}
function format(v, indent = '') {
  if (isScalar(v)) return JSON.stringify(v);
  if (isFlat(v)) {
    const line = inlineJson(v);
    if (line.length <= 110) return line;
  }
  const next = `${indent}  `;
  const nl = '\n';
  if (Array.isArray(v)) {
    const items = v.map((x) => next + format(x, next));
    return `[${nl}${items.join(`,${nl}`)}${nl}${indent}]`;
  }
  const items = Object.entries(v).map(([k, x]) => `${next}${JSON.stringify(k)}: ${format(x, next)}`);
  return `{${nl}${items.join(`,${nl}`)}${nl}${indent}}`;
}
