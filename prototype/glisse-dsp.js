// Glisse DSP — AudioWorklet. Voices (sub / fm / pluck), resonant filter, envelopes,
// 16-step chord sequencer, 4/4 kick, sidechain envelope. Structured like the future FM-1 firmware.
// Outputs: 0 = chord+bass (mono), 1 = kick (mono), 2 = sidechain gain (0..1)

const SR = sampleRate;
const TAU = Math.PI * 2;
const mtof = m => 440 * Math.pow(2, (m - 69) / 12);

function blep(t, dt){
  if (t < dt) { t /= dt; return t + t - t * t - 1; }
  if (t > 1 - dt) { t = (t - 1) / dt; return t * t + t + t + 1; }
  return 0;
}
function osc(w, ph, dt){
  switch (w) {
    case 0: return 2 * ph - 1 - blep(ph, dt);                                   // saw
    case 1: { let s = ph < .5 ? 1 : -1; s += blep(ph, dt); s -= blep((ph + .5) % 1, dt); return s; } // square
    case 2: return 1 - 4 * Math.abs(ph - .5);                                   // triangle
    default: return Math.sin(TAU * ph);                                         // sine
  }
}

const DEF = {engine:'sub', w1:0, w2:0, det:6, oct2:0, mix2:.5, ratio:1, idx:1, idec:.4, isus:.2, fb:0,
  bright:.6, damp:.996, atk:.01, dec:.3, sus:1, rel:.5, fenv:1, fdec:.3, wob:5, wobRate:.3, noise:0, bass:.5, level:1};

class Voice {
  constructor(){
    this.m = 60; this.target = 60; this.inc = 0;
    this.p1 = Math.random(); this.p2 = Math.random();
    this.pc = 0; this.pm = 0; this.prev = 0;
    this.buf = new Float32Array(4096); this.w = 0; this.last = 0;
  }
  setNote(m, glideS){
    this.target = m;
    if (glideS < 1) { this.m = m; this.inc = 0; }
    else this.inc = (m - this.m) / glideS;
  }
  tick(){
    if (this.inc !== 0) {
      this.m += this.inc;
      if ((this.inc > 0 && this.m >= this.target) || (this.inc < 0 && this.m <= this.target)) { this.m = this.target; this.inc = 0; }
    }
  }
  excite(bright){
    const L = Math.min(4000, SR / mtof(this.m));
    const n = Math.ceil(L) + 2, a = .12 + .88 * bright;
    let lp = 0;
    for (let j = 0; j < n; j++) {
      lp += a * ((Math.random() * 2 - 1) - lp);
      const k = (this.w - n + j + 8192) & 4095;
      this.buf[k] = this.buf[k] * .25 + lp * 1.4;
    }
  }
}

class Glisse extends AudioWorkletProcessor {
  constructor(){
    super();
    this.v = [new Voice(), new Voice(), new Voice(), new Voice()];
    this.bm = 36; this.binc = 0; this.btarget = 36; this.bp = 0; this.bs = 0;
    this.p = Object.assign({}, DEF);
    this.cut = 8000; this.Q = 1; this.bpm = 118;
    this.kick = false; this.side = false; this.bassOn = true;
    this.rh = {hold: true};
    this.active = false; this.running = false; this.step = 0; this.stepLeft = 0;
    this.e = 0; this.stage = 0; this.vel = 1; this.gateLeft = 0; this.atkS = 1; this.relS = SR * .1;
    this.fEnv = 0; this.iEnv = 0;
    this.ic1 = 0; this.ic2 = 0; this.g = .5; this.k = 1; this.cnt = 0;
    this.wobPh = 0;
    this.fadeLen = Math.round(.004 * SR); this.fadeLeft = 0; this.pending = null; this.retrig = false;
    this.kPh = 0; this.kT = 1e9; this.duck = 0; this.duckAtk = false; this.kLast = 0; this.kTail = 0;
    this.port.onmessage = e => this.msg(e.data);
  }

  msg(d){
    switch (d.t) {
      case 'sound': this.p = Object.assign({}, DEF, d.p); break;
      case 'set': Object.assign(this, d.v); if (!this.active && !this.kick && !this.side) this.running = false;
        if ((this.kick || this.side) && !this.running) this.startTransport(); break;
      case 'rhythm':
        this.rh = d.r;
        if (this.active) {
          if (this.rh.hold) this.hit(1, Infinity);
          else { this.gateLeft = 0; if (!this.running) this.startTransport(); }
        }
        break;
      case 'chord': this.chord(d); break;
      case 'stop':
        this.active = false;
        if (this.stage >= 1 && this.stage <= 3) this.gateLeft = 0;
        if (!this.kick && !this.side) this.running = false;
        break;
    }
  }

  setNotes(notes, bass, glideS){
    for (let i = 0; i < 4; i++) this.v[i].setNote(notes[i], glideS);
    this.btarget = bass;
    if (glideS < 1) { this.bm = bass; this.binc = 0; } else this.binc = (bass - this.bm) / glideS;
  }

  chord(d){
    const gs = d.glide * SR;
    if (!this.active) {
      this.active = true;
      this.setNotes(d.notes, d.bass, 0);
      if (this.fadeLeft > 0) { this.fadeLeft = 0; this.pending = null; }
      if (this.rh.hold) this.hit(1, Infinity);
      else if (!this.running) this.startTransport();
      return;
    }
    if (gs >= 1) { this.setNotes(d.notes, d.bass, gs); return; }
    if (this.e > .002) {
      // A: quick fade, swap notes, restart as a fresh hit if the gate is still open
      this.pending = d;
      this.retrig = this.stage >= 1 && this.stage <= 3;
      if (this.fadeLeft <= 0) this.fadeLeft = this.fadeLen;
    } else this.setNotes(d.notes, d.bass, 0);
  }

  startTransport(){ this.running = true; this.step = 0; this.stepLeft = 0; }

  stepLen(i){
    const sw = this.rh.swing || 0, d = 60 / this.bpm / 4 * SR;
    return i % 2 === 0 ? d * (1 + sw) : d * (1 - sw);
  }

  hit(vel, gateS, atk){
    if (this.fadeLeft > 0 && this.pending) { this.setNotes(this.pending.notes, this.pending.bass, 0); this.pending = null; this.fadeLeft = 0; }
    const p = this.p;
    this.vel = vel; this.stage = 1; this.gateLeft = gateS;
    // in a rhythm, long attacks never get time to open: keep them short (slow attack only when held)
    this.atkS = Math.max(1, (atk || (this.rh.hold ? p.atk : Math.min(p.atk, .012))) * SR);
    this.relS = (this.rh.hold ? p.rel : Math.min(p.rel, .3)) * SR;
    this.fEnv = 1; this.iEnv = 1;
    if (p.engine === 'pluck') for (const v of this.v) v.excite(p.bright);
  }

  onStep(off){
    const i = this.step, r = this.rh;
    const beat = i % 4 === 0;
    if (beat && this.kick) { this.kTail = this.kLast; this.kT = 0; this.kPh = 0; }
    if (beat && (this.kick || this.side)) this.duckAtk = true;
    let hv = 0;
    if (this.active && !r.hold && r.steps[i]) {
      const s = r.steps[i];
      let len = 0; for (let k = 0; k < s.len; k++) len += this.stepLen((i + k) % 16);
      this.hit(s.v, len * r.gate, r.atk);
      hv = s.v;
    }
    this.port.postMessage({t:'step', i, time: currentTime + off / SR, v: hv, kick: beat && this.kick});
    if (!this.active && !this.kick && !this.side) this.running = false;
  }

  process(inputs, outputs){
    const out = outputs[0][0], kout = outputs[1][0], dout = outputs[2][0];
    const n = out.length, p = this.p;
    const dC = Math.exp(-1 / (Math.max(.005, p.dec) * SR * .3));
    const rC = Math.exp(-1 / (Math.max(.004, this.relS / SR) * SR * .25));
    const fC = Math.exp(-1 / (Math.max(.01, p.fdec) * SR * .33));
    const iC = Math.exp(-1 / (Math.max(.01, p.idec) * SR * .33));
    const duckC = Math.exp(-1 / (.09 * SR));
    const duckA = 1 - Math.exp(-1 / (.004 * SR));
    const tailC = Math.exp(-1 / (.003 * SR));
    const wobInc = TAU * p.wobRate / SR;
    const eng = p.engine === 'fm' ? 1 : p.engine === 'pluck' ? 2 : 0;
    const lvl = p.level * (eng === 1 ? .16 : eng === 2 ? .3 : .11);
    const kdamp = 1 / this.Q;

    for (let s = 0; s < n; s++) {
      // transport
      if (this.running) {
        if (this.stepLeft <= 0) { this.onStep(s); this.stepLeft += this.stepLen(this.step); this.step = (this.step + 1) % 16; }
        this.stepLeft--;
      }

      // amp envelope
      const susL = p.sus * this.vel;
      switch (this.stage) {
        case 1: this.e += this.vel / this.atkS; if (this.e >= this.vel) { this.e = this.vel; this.stage = 2; } break;
        case 2: this.e = susL + (this.e - susL) * dC; if (Math.abs(this.e - susL) < 1e-4) this.stage = 3; break;
        case 3: this.e = susL; break;
        case 4: this.e *= rC; if (this.e < 1e-5) { this.e = 0; this.stage = 0; } break;
      }
      if (this.stage >= 1 && this.stage <= 3) { if (--this.gateLeft <= 0) this.stage = 4; }
      this.fEnv *= fC; this.iEnv *= iC;

      // transition fade
      let fade = 1;
      if (this.fadeLeft > 0) {
        fade = this.fadeLeft / this.fadeLen;
        if (--this.fadeLeft === 0 && this.pending) {
          const d = this.pending; this.pending = null;
          this.setNotes(d.notes, d.bass, 0);
          if (this.retrig) { this.e = 0; this.hit(this.vel, Math.max(this.gateLeft, SR * .04)); }
          else { this.e = 0; this.stage = 0; }
          fade = 0;
        }
      }

      // voices
      const wob = p.wob / 100 * Math.sin(this.wobPh);
      this.wobPh += wobInc; if (this.wobPh > TAU) this.wobPh -= TAU;
      let sum = 0;
      for (let vi = 0; vi < 4; vi++) {
        const v = this.v[vi]; v.tick();
        const m = v.m + wob;
        if (eng === 0) {
          const f1 = mtof(m - p.det / 200), f2 = mtof(m + p.oct2 + p.det / 200);
          const d1 = f1 / SR, d2 = f2 / SR;
          sum += osc(p.w1, v.p1, d1) + p.mix2 * osc(p.w2, v.p2, d2);
          v.p1 += d1; if (v.p1 >= 1) v.p1 -= 1;
          v.p2 += d2; if (v.p2 >= 1) v.p2 -= 1;
        } else if (eng === 1) {
          const fc = mtof(m), idx = p.idx * (p.isus + (1 - p.isus) * this.iEnv);
          const mod = Math.sin(TAU * v.pm + p.fb * v.prev) * idx;
          v.prev = mod;
          sum += Math.sin(TAU * v.pc + mod);
          v.pc += fc / SR; if (v.pc >= 1) v.pc -= 1;
          v.pm += fc * p.ratio / SR; if (v.pm >= 1) v.pm -= 1;
        } else {
          const L = Math.min(4000, SR / mtof(m));
          const rp = v.w - L, i0 = Math.floor(rp), fr = rp - i0;
          const a = v.buf[(i0 + 8192) & 4095], b = v.buf[(i0 + 8193) & 4095];
          const x = a + (b - a) * fr;
          const y = p.damp * .5 * (x + v.last);
          v.last = x;
          v.buf[v.w & 4095] = y; v.w = (v.w + 1) & 4095;
          sum += x;
        }
      }
      sum += p.noise * (Math.random() * 2 - 1) * 4;

      // filter (TPT state-variable lowpass), coefficient refreshed every 8 samples
      if ((this.cnt++ & 7) === 0) {
        const fc = Math.min(SR * .45, this.cut * Math.pow(2, p.fenv * this.fEnv * this.vel));
        this.g = Math.tan(Math.PI * fc / SR);
      }
      const g = this.g, a1 = 1 / (1 + g * (g + kdamp)), a2 = g * a1, a3 = g * a2;
      const v3 = sum - this.ic2;
      const v1 = a1 * this.ic1 + a2 * v3;
      const v2 = this.ic2 + a2 * this.ic1 + a3 * v3;
      this.ic1 = 2 * v1 - this.ic1; this.ic2 = 2 * v2 - this.ic2;

      // bass
      if (this.binc !== 0) {
        this.bm += this.binc;
        if ((this.binc > 0 && this.bm >= this.btarget) || (this.binc < 0 && this.bm <= this.btarget)) { this.bm = this.btarget; this.binc = 0; }
      }
      const bf = mtof(this.bm) / SR;
      const bass = this.bassOn ? (osc(2, this.bp, bf) * .55 + Math.sin(TAU * this.bs) * .8) * p.bass : 0;
      this.bp += bf; if (this.bp >= 1) this.bp -= 1;
      this.bs += bf * .5; if (this.bs >= 1) this.bs -= 1;

      const y = (v2 * lvl + bass * .2) * this.e * fade;
      out[s] = Math.tanh(y * 1.2) / 1.2;

      // kick
      let kv = 0;
      if (this.kT < SR * .3) {
        const t = this.kT / SR;
        // round kick: softer, lower pitch sweep, 1 ms fade-in, gentle saturation, no noise click
        // short, dry, tight: quick small pitch drop, ~150 ms body
        const f = 62 + 95 * Math.exp(-t * 55);
        this.kPh += f / SR;
        const amp = Math.min(1, t / .0008) * Math.exp(-t * 19);
        kv = Math.tanh(Math.sin(TAU * this.kPh) * amp * 1.6) / Math.tanh(1.6);
        this.kT++;
      }
      // the previous kick's tail fades out in 3 ms instead of being cut (no click on retrigger)
      this.kTail *= tailC;
      this.kLast = kv;
      kout[s] = (kv + this.kTail) * .5;

      // sidechain
      // sidechain: 4 ms dip, smooth release
      if (this.duckAtk) { this.duck += (1 - this.duck) * duckA; if (this.duck > .985) this.duckAtk = false; }
      else this.duck *= duckC;
      const dd = Math.min(1, this.duck * 1.1), shaped = dd * dd * (3 - 2 * dd);
      dout[s] = this.side ? 1 - .8 * shaped : 1;
    }
    return true;
  }
}

registerProcessor('glisse', Glisse);
