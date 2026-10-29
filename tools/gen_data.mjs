// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Obsimo (OSR Records)
import fs from 'fs';
const html = fs.readFileSync(new URL('../prototype/osr1-prototype.html', import.meta.url), 'utf8');
const s = html.slice(html.indexOf('<script>') + 8, html.lastIndexOf('</script>'));
const pick = (a, b) => s.slice(s.indexOf(a), s.indexOf(b));
const FAMILIES = new Function(pick('const FAMILIES', '// 16 steps') + 'return FAMILIES;')();
const RHYTHMS = new Function(pick('// 16 steps', 'RHYTHMS.forEach') + 'return RHYTHMS;')();
const GRIDS = new Function(pick('const GRIDS', 'const parseChords') + 'return GRIDS;')();
const DEF = {engine:'sub', w1:0, w2:0, det:6, oct2:0, mix2:.5, ratio:1, idx:1, idec:.4, isus:.2, fb:0,
  bright:.6, damp:.996, atk:.01, dec:.3, sus:1, rel:.5, fenv:1, fdec:.3, chorus:.5, wob:5, wobRate:.3, noise:0, bass:.45, level:1};
const f = x => { let t = (+x).toString(); if (!t.includes('.') && !t.includes('e')) t += '.0'; return t + 'f'; };
const esc = n => n.replace(/[éèê]/g,'E').replace(/[ô]/g,'O').replace(/[àâ]/g,'A').replace(/[ç]/g,'C').replace(/'/g,"'").toUpperCase();
let out = `/* SPDX-License-Identifier: GPL-3.0-only\n * Copyright (C) 2026 Obsimo (OSR Records) */\n/* OSR-1 data: generated from the browser prototype (gen_data.mjs). Do not edit by hand. */\n#pragma once\n`;
out += `static const char *const FAMILY_NAME[${FAMILIES.length}] = {${FAMILIES.map(x=>`"${esc(x.name)}"`).join(', ')}};\n`;
out += `#define NFAMILY ${FAMILIES.length}\n#define PER_FAMILY 8\n`;
out += `static const char *const SOUND_NAME[${FAMILIES.length*8}] = {\n` + FAMILIES.flatMap(x=>x.sounds).map(x=>`    "${esc(x.name)}"`).join(',\n') + '\n};\n';
const E = {sub:'ENGINE_SUB', fm:'ENGINE_FM', pluck:'ENGINE_PLUCK'};
out += `static const sound_t SOUNDS[${FAMILIES.length*8}] = {\n`;
for (const x of FAMILIES.flatMap(x=>x.sounds)) {
  const p = Object.assign({}, DEF, x);
  out += `    {${E[p.engine]}, ${p.w1}, ${p.w2}, ${f(p.det)}, ${f(p.oct2)}, ${f(p.mix2)}, ${f(p.ratio)}, ${f(p.idx)}, ${f(p.idec)}, ${f(p.isus)}, ${f(p.fb)}, ${f(p.bright)}, ${f(p.damp)}, ${f(p.atk)}, ${f(p.dec)}, ${f(p.sus)}, ${f(p.rel)}, ${f(p.fenv)}, ${f(p.fdec)}, ${f(p.chorus)}, ${f(p.wob)}, ${f(p.wobRate)}, ${f(p.noise)}, ${f(p.bass)}, ${f(p.level)}},   /* ${p.name} */\n`;
}
out += '};\n';
out += `#define NRHYTHM ${RHYTHMS.length}\nstatic const char *const RHYTHM_NAME[NRHYTHM] = {${RHYTHMS.map(r=>`"${esc(r.name)}"`).join(', ')}};\n`;
out += `/* 16 steps: X accent, x soft, - tie, . rest ("" = held) */\nstatic const struct { const char *s; float gate, swing, atk; } RHYTHM_DEF[NRHYTHM] = {\n`;
for (const r of RHYTHMS) out += `    {"${r.hold ? '' : r.s}", ${f(r.gate||0)}, ${f(r.swing||0)}, ${f(r.atk||0)}},\n`;
out += '};\n';
out += `#define NGRID ${GRIDS.length}\n/* chords: semitones above the tonic + quality (M maj, V dominant-major, m, D dom7, d dim, s sus4, S sus2, a aug); 8 white keys then 5 black */\n`;
out += `static const struct { const char *name; uint8_t rich; const char *white, *black; } GRID_DEF[NGRID] = {\n`;
for (const g of GRIDS) out += `    {"${esc(g.n)}", ${g.r}, "${g.w}", "${g.b}"},\n`;
out += '};\n';
fs.writeFileSync(new URL('../data.h', import.meta.url), out);
console.log('ok', FAMILIES.length*8, 'sounds', RHYTHMS.length, 'rhythms', GRIDS.length, 'grids');
