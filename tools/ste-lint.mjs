#!/usr/bin/env node
// STE-style lint: checks sentence length and simple passive-voice patterns.
// It is NOT a full ASD-STE100 checker (no dictionary, no grammar parser).
//
// Scanned text:
//   *.md   -> text between <!--STE--> and <!--/STE-->
//   *.html -> every string under a key named "ste" in <script type="application/json" id="ste-data">
//             (the older id "aoi-data" is accepted too)
//
// Usage: node tools/ste-lint.mjs [files...]   (default: all of docs/, except _template folders and _template.md files)
// Run it from any folder: the default docs/ is found relative to this file.
// Exit code 1 when an error is found. Add --strict to fail on warnings too.
import { existsSync, readFileSync, readdirSync, statSync } from 'node:fs';
import { dirname, join, extname } from 'node:path';
import { fileURLToPath } from 'node:url';

const DOCS_DIR = join(dirname(fileURLToPath(import.meta.url)), '..', 'docs');

const MAX_ERROR = 25; // words per sentence: hard limit (descriptive text)
const MAX_WARN = 20; // words per sentence: procedure limit
const PASSIVE = /\b(is|are|was|were|be|been|being)\s+(\w+ed|made|done|given|shown|found|built|written|taken|set|run|kept|sent|used)\b/i;

const args = process.argv.slice(2);
const strict = args.includes('--strict');
const files = args.filter((a) => !a.startsWith('--'));

function walk(dir, out = []) {
  for (const name of readdirSync(dir)) {
    if (name === 'node_modules' || name === '_template' || name === '_template.md') continue;
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (['.md', '.html'].includes(extname(p))) out.push(p);
  }
  return out;
}

function collectKey(node, key, out) {
  if (Array.isArray(node)) node.forEach((n) => collectKey(n, key, out));
  else if (node && typeof node === 'object') {
    for (const [k, v] of Object.entries(node)) {
      if (k === key) collectStrings(v, out);
      else collectKey(v, key, out);
    }
  }
}
function collectStrings(v, out) {
  if (typeof v === 'string') out.push(v);
  else if (Array.isArray(v)) v.forEach((x) => collectStrings(x, out));
  else if (v && typeof v === 'object') Object.values(v).forEach((x) => collectStrings(x, out));
}

function extract(path) {
  const text = readFileSync(path, 'utf8');
  const blocks = [];
  if (extname(path) === '.md') {
    for (const m of text.matchAll(/<!--STE-->([\s\S]*?)<!--\/STE-->/g)) blocks.push(m[1]);
  } else {
    const m = text.match(/<script type="application\/json" id="(?:ste|aoi)-data">([\s\S]*?)<\/script>/);
    if (m) collectKey(JSON.parse(m[1]), 'ste', blocks);
  }
  return blocks;
}

function sentences(block) {
  return block
    .split(/\r?\n/)
    .filter((l) => !/^\s*#/.test(l)) // skip headings
    .map((l) => l.replace(/^\s*[-*]\s+/, '').replace(/^\s*\d+\.\s+/, ''))
    .join(' ')
    .replace(/`[^`]*`/g, 'X')
    .replace(/\[([^\]]*)\]\([^)]*\)/g, '$1')
    .replace(/\*\*/g, '')
    .replace(/\b(e\.g|i\.e|etc|vs)\./gi, '$1')
    .replace(/(\d)\.(\d)/g, '$1,$2')
    .split(/(?<=[.!?])\s+/)
    .map((s) => s.trim())
    .filter(Boolean);
}

let errors = 0;
let warnings = 0;
let checked = 0;
for (const f of files.length ? files : existsSync(DOCS_DIR) ? walk(DOCS_DIR) : []) {
  for (const block of extract(f)) {
    for (const s of sentences(block)) {
      checked++;
      const words = (s.match(/[A-Za-z0-9][\w'/-]*/g) || []).length;
      const short = s.length > 90 ? `${s.slice(0, 87)}...` : s;
      if (words > MAX_ERROR) {
        errors++;
        console.log(`ERROR ${f}: ${words} words (max ${MAX_ERROR}): ${short}`);
      } else if (words > MAX_WARN) {
        warnings++;
        console.log(`warn  ${f}: ${words} words (procedure max ${MAX_WARN}): ${short}`);
      }
      if (PASSIVE.test(s)) {
        warnings++;
        console.log(`warn  ${f}: possible passive voice: ${short}`);
      }
    }
  }
}
console.log(`STE lint: ${checked} sentences, ${errors} errors, ${warnings} warnings.`);
process.exit(errors || (strict && warnings) ? 1 : 0);
