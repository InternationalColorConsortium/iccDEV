#!/usr/bin/env node
// regression.js — Testing/CreateAllProfiles.sh parity gate
//
// Runs the upstream Testing/CreateAllProfiles.sh through MEMFS-backed
// shims of the 16 published WASM tools, then asserts the produced
// .icc count matches the native-Linux baseline.
//
// Expected baseline (CreateAllProfiles.sh in isolation): 207 .icc
// Source of truth: native-linux iccDEV/Build/Tools at the same SHA.
//
// Exit 0 = parity. Non-zero = drift; prints per-dir distribution diff.

const fs = require('fs');
const path = require('path');
const os = require('os');
const cp = require('child_process');

const EXPECTED = parseInt(process.env.ICCDEV_REGRESSION_EXPECTED || '207', 10);
const PKG_ROOT = __dirname;
const TESTING = path.join(PKG_ROOT, 'Testing');

if (!fs.existsSync(TESTING)) {
  console.error('regression.js: Testing/ not present in package; skipping');
  process.exit(0);
}
if (!fs.existsSync(path.join(TESTING, 'CreateAllProfiles.sh'))) {
  console.error('regression.js: Testing/CreateAllProfiles.sh missing; skipping');
  process.exit(0);
}

// 16 published tool dirs (mirrors stage.sh / test_all.js)
const TOOLS = [
  'iccDumpProfile', 'iccToXml', 'iccFromXml', 'iccToJson', 'iccFromJson',
  'iccRoundTrip', 'iccFromCube', 'iccApplyNamedCmm', 'iccApplyProfiles',
  'iccApplySearch', 'iccApplyToLink', 'iccTiffDump', 'iccJpegDump',
  'iccPngDump', 'iccSpecSepToTiff', 'iccV5DspObsToV4Dsp'
];

function dirCase(tool) {
  // iccDumpProfile -> IccDumpProfile
  return 'I' + tool.substring(1, 0) + tool.charAt(0).toUpperCase() + tool.substring(1);
}
// simpler: just uppercase first char of "icc..." -> "Icc..."
function pkgDirOf(tool) {
  return tool.charAt(0).toUpperCase() + tool.substring(1);
}

function findToolJs(tool) {
  const candidates = [
    path.join(PKG_ROOT, pkgDirOf(tool), tool + '.js'),
    path.join(PKG_ROOT, pkgDirOf(tool).toUpperCase(), tool + '.js'),
  ];
  for (const c of candidates) if (fs.existsSync(c)) return c;
  // Fallback: scan
  for (const e of fs.readdirSync(PKG_ROOT)) {
    const p = path.join(PKG_ROOT, e, tool + '.js');
    if (fs.existsSync(p)) return p;
  }
  return null;
}

const work = fs.mkdtempSync(path.join(os.tmpdir(), 'iccdev-regression-'));
const shimDir = path.join(work, 'shim');
fs.mkdirSync(shimDir, { recursive: true });

// Generate one PATH-resolvable shim per tool
const NODE_BIN = process.execPath;
for (const tool of TOOLS) {
  const js = findToolJs(tool);
  if (!js) {
    console.error(`regression.js: tool module not found: ${tool}`);
    process.exit(2);
  }
  const shim = `#!${NODE_BIN}
const path=require('path'),fs=require('fs');
const factory=require(${JSON.stringify(js)});
async function main(){
  const argv=process.argv.slice(1);
  const m=await factory({noInitialRun:true,print:s=>process.stdout.write(s+'\\n'),printErr:s=>process.stderr.write(s+'\\n')});
  const FS=m.FS, cwd=process.cwd();
  function mirrorIn(host,depth){
    if(depth>5)return;
    let st;try{st=fs.statSync(host);}catch{return;}
    if(st.isDirectory()){
      const rel=host==cwd?'/':path.relative(cwd,host).split(path.sep).join('/');
      const memdir='/'+rel.replace(/^\\//,'');
      try{FS.mkdir(memdir);}catch{}
      for(const e of fs.readdirSync(host)){
        if(e.startsWith('.'))continue;
        mirrorIn(path.join(host,e),depth+1);
      }
    } else if(st.isFile() && st.size<64*1024*1024){
      const rel=path.relative(cwd,host).split(path.sep).join('/');
      try{FS.writeFile('/'+rel,fs.readFileSync(host));}catch{}
    }
  }
  function ensureParent(p){
    const parts=p.split('/').filter(Boolean); let cur='';
    for(let i=0;i<parts.length-1;i++){cur+='/'+parts[i];try{FS.mkdir(cur);}catch{}}
  }
  mirrorIn(cwd,0);
  const absOut=argv.slice(1).filter(a=>a.startsWith('/'));
  for(const a of absOut){ensureParent(a);}
  function snap(dir,o){try{for(const e of FS.readdir(dir)){if(e=='.'||e=='..')continue;const p=dir=='/'?'/'+e:dir+'/'+e;let s;try{s=FS.stat(p);}catch{continue;}if(FS.isDir(s.mode))snap(p,o);else o[p]=s.mtime;}}catch{}}
  const before={};snap('/',before);
  try{m.callMain(argv.slice(1));}catch(e){if(e&&e.name!='ExitStatus')throw e;}
  const after={};snap('/',after);
  for(const p of Object.keys(after)){
    if(before[p]==after[p])continue;
    let buf;try{buf=FS.readFile(p);}catch{continue;}
    let host;
    if(absOut.includes(p)){host=p;}
    else{const rel=p.replace(/^\\//,'');host=path.join(cwd,rel);}
    try{fs.mkdirSync(path.dirname(host),{recursive:true});fs.writeFileSync(host,Buffer.from(buf));}catch{}
  }
}
main().catch(e=>{console.error(e);process.exit(1);});
`;
  const shimPath = path.join(shimDir, tool);
  fs.writeFileSync(shimPath, shim);
  fs.chmodSync(shimPath, 0o755);
}

// Copy Testing/ into a writable work dir
const testingWork = path.join(work, 'Testing');
cp.execFileSync('cp', ['-a', TESTING + '/.', testingWork + '/']);
// Strip CRLF from any *.sh (defensive — Windows checkouts)
cp.execFileSync('bash', ['-c',
  `find ${JSON.stringify(testingWork)} -name '*.sh' -exec sed -i 's/\\r$//' {} +`]);

console.log('iccDEV WASM Regression: Testing/CreateAllProfiles.sh');
console.log(`  pkg root:  ${PKG_ROOT}`);
console.log(`  work dir:  ${work}`);
console.log(`  expected:  ${EXPECTED} .icc files`);
console.log('');

const env = Object.assign({}, process.env, {
  PATH: shimDir + path.delimiter + (process.env.PATH || '/usr/bin:/bin'),
});
const logPath = path.join(work, 'create.log');
const logFd = fs.openSync(logPath, 'w');
const child = cp.spawnSync('sh', ['CreateAllProfiles.sh'], {
  cwd: testingWork, env, stdio: ['ignore', logFd, logFd],
});
fs.closeSync(logFd);

// Count produced ICCs
const counts = {};
let total = 0;
function walk(dir) {
  for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
    const p = path.join(dir, e.name);
    if (e.isDirectory()) walk(p);
    else if (e.isFile() && e.name.endsWith('.icc')) {
      const rel = path.relative(testingWork, path.dirname(p)) || '.';
      counts[rel] = (counts[rel] || 0) + 1;
      total++;
    }
  }
}
walk(testingWork);

console.log(`Result: ICC files generated = ${total}  (expected ${EXPECTED})`);
console.log('Per-dir distribution:');
const dirs = Object.keys(counts).sort((a, b) => counts[b] - counts[a]);
for (const d of dirs) {
  console.log(`  ${String(counts[d]).padStart(4)}  ${d}`);
}

if (child.status !== 0) {
  console.error(`\nCreateAllProfiles.sh exited ${child.status}; tail of log:`);
  const tail = fs.readFileSync(logPath, 'utf8').split('\n').slice(-20).join('\n');
  console.error(tail);
}

if (total !== EXPECTED) {
  console.error(`\nFAIL: parity mismatch (got ${total}, expected ${EXPECTED})`);
  process.exit(1);
}
console.log('\nPASS: WASM CreateAllProfiles.sh produced expected ICC count');
process.exit(0);
