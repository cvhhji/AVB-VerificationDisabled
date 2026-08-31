#!/usr/bin/env node
// Reproduce the patcher's ADRP+ADD string-reference scan without modifying ABL.
import fs from "node:fs";

const path = process.argv[2];
if (!path) throw new Error("usage: node tests/diagnose_adrl.mjs <abl.elf>");
const data = fs.readFileSync(path);
const u32 = off => data.readUInt32LE(off);
const rd = ins => ins & 31;
const rn = ins => (ins >>> 5) & 31;
const isAdrp = ins => (ins & 0x9f000000) === 0x90000000;
const isAdd = ins => (ins & 0xff000000) === 0x91000000;
const isStp = ins => (ins & 0xff000000) === 0xa9000000;
const isSub = ins => (ins & 0xff800000) === 0xd1000000;

function adrlTarget(off) {
  const adrp = u32(off), add = u32(off + 4);
  if (!isAdrp(adrp) || !isAdd(add) || rd(adrp) !== rn(add)) return null;
  let imm = (((adrp >>> 5) & 0x7ffff) << 2) | ((adrp >>> 29) & 3);
  if (imm & (1 << 20)) imm -= 1 << 21;
  const page = (off & ~0xfff) + imm * 4096;
  const addImm = (add >>> 10) & 0xfff;
  return page + addImm * (((add >>> 22) & 1) ? 4096 : 1);
}

function functionStart(from) {
  const limit = Math.max(0, from - 4096) & ~3;
  for (let off = from - 4; off >= limit; off -= 4) {
    const ins = u32(off);
    if ((isStp(ins) && rn(ins) === 31) ||
        (isSub(ins) && rd(ins) === 31 && rn(ins) === 31)) return off;
  }
  return null;
}

const needles = ["AVB0", "vbmeta", "VerifiedBoot", "androidboot.vbmeta"];
const funcs = new Map();
for (const needle of needles) {
  const bytes = Buffer.from(needle);
  let stringOff = -1;
  while ((stringOff = data.indexOf(bytes, stringOff + 1)) >= 0) {
    const refs = [];
    for (let off = 0; off + 8 <= data.length; off += 4) {
      if (adrlTarget(off) === stringOff) refs.push(off);
    }
    console.log(`${needle} string=0x${stringOff.toString(16)} refs=${refs.length}`);
    for (const ref of refs) {
      const start = functionStart(ref);
      console.log(`  ref=0x${ref.toString(16)} func=${start === null ? "none" : `0x${start.toString(16)}`}`);
      if (start !== null) {
        const evidence = funcs.get(start) ?? [];
        evidence.push({needle, stringOff, ref});
        funcs.set(start, evidence);
      }
    }
  }
}

console.log(`unique candidate functions=${funcs.size}`);
for (const [func, evidence] of funcs) {
  console.log(`func=0x${func.toString(16)} evidence=${evidence.map(x => `${x.needle}@0x${x.ref.toString(16)}`).join(",")}`);
}
