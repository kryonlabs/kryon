// Private headless browsers, interleaved samples, identical rendered output.
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {readFile, mkdtemp, rm, writeFile} from 'node:fs/promises';
import {tmpdir} from 'node:os';
import {join, resolve} from 'node:path';
import {pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
import {setTimeout as delay} from 'node:timers/promises';

const [before, after, receipt] = process.argv.slice(2);
assert(before && after && receipt, 'before-dir after-dir output.json required');
async function open(directory) {
  const profile = await mkdtemp(join(tmpdir(), 'canvas-performance-'));
  const env = {...process.env};
  for(const key of ['DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER','GDK_DISPLAY']) delete env[key];
  const browser = spawn(process.env.CHROMIUM || 'chromium', ['--headless','--no-sandbox','--disable-gpu','--disable-dev-shm-usage','--no-first-run','--no-default-browser-check','--allow-file-access-from-files','--remote-debugging-port=0','--user-data-dir='+profile,'about:blank'], {env,detached:true,stdio:['ignore','ignore','pipe']});
  let errors = '', socket;
  browser.stderr.on('data', d => errors=(errors+d).slice(-4000));
  const close = async () => {
    socket?.close();
    if(browser.exitCode === null && browser.signalCode === null) {
      const done = new Promise(r => browser.once('exit',r));
      process.kill(-browser.pid,'SIGTERM'); await done;
    }
    await rm(profile,{recursive:true,force:true});
  };
  try {
    let port;
    for(const end=Date.now()+20000; Date.now()<end && !port;) {
      try {port=(await readFile(join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0];}
      catch {assert.equal(browser.exitCode,null,errors); await delay(50);}
    }
    assert(port,errors);
    const pages=await(await fetch('http://127.0.0.1:'+port+'/json/list')).json();
    socket=new WebSocket(pages.find(p=>p.type==='page').webSocketDebuggerUrl);
    await new Promise((r,j)=>{socket.addEventListener('open',r,{once:true});socket.addEventListener('error',j,{once:true});});
    let sequence=0; const pending=new Map();
    socket.addEventListener('message',e=>{const m=JSON.parse(e.data); if(pending.has(m.id)){pending.get(m.id)(m);pending.delete(m.id);}});
    async function call(method,params={}) {
      const id=++sequence;
      const result=new Promise((r,j)=>{
        pending.set(id,r);
        setTimeout(()=>{if(pending.delete(id)) j(Error(method+' timed out'));},30000).unref();
      });
      socket.send(JSON.stringify({id,method,params})); const m=await result;
      assert(!m.error,JSON.stringify(m.error)); return m.result;
    }
    async function evaluate(expression) {
      const result=await call('Runtime.evaluate',{expression,returnByValue:true});
      assert(!result.exceptionDetails,JSON.stringify(result.exceptionDetails));return result.result.value;
    }
    await call('Page.navigate',{url:pathToFileURL(resolve(directory,'index.html')).href});
    let ready=false;
    for(const end=Date.now()+20000;Date.now()<end && !ready;){ready=await evaluate('!!globalThis.ready');if(!ready) await delay(50);}
    assert(ready,'Wasm initialization timed out');
    assert.equal(await evaluate('Module._Setup()'),0);
    return {evaluate,close};
  } catch(e) {await close();throw e;}
}
const clients=[];
try {
  clients.push(await open(before)); clients.push(await open(after));
  const samples=[];
  let tileOracle = 2166136261;
  for(let y=0;y<384;y++) for(let x=0;x<512;x++) {
    const pixel=y<256 ? [x%16<8?240:40,y%16<8?80:200,120,255] : [12,24,36,255];
    for(const channel of pixel) tileOracle=Math.imul(tileOracle^channel,16777619);
  }
  for(const transformed of [0,1]) {
    // Include deferred raster work with a synchronous readback inside the timer.
    const expression=`(()=>{const begin=performance.now();Module._Draw(50,${transformed});const pixels=document.querySelector('canvas').getContext('2d').getImageData(0,0,512,384).data;const end=performance.now();let hash=2166136261;for(const x of pixels) hash=Math.imul(hash^x,16777619);return {ms:end-begin,hash:hash>>>0};})()`;
    for(const client of clients) await client.evaluate(expression);
    let reference;
    for(let round=0;round<9;round++) {
      for(const version of (round%2 ? [1,0] : [0,1])) {
        const result=await clients[version].evaluate(expression);
        if(transformed===0) assert.equal(result.hash,tileOracle>>>0,'Tile board differs from the independent pixel oracle');
        reference ??= result.hash;
        assert.equal(result.hash,reference,'Rendered output changed');
        samples.push({transformed,round,version:version?'after':'before',...result});
      }
    }
  }
  const median=xs=>xs.sort((a,b)=>a-b)[Math.floor(xs.length/2)];
  const summary=[0,1].map(transformed=>{
    const times=version=>median(samples.filter(s=>s.transformed===transformed&&s.version===version).map(s=>s.ms));
    const beforeMs=times('before'),afterMs=times('after');
    return {transformed,beforeMs,afterMs,throughputGainPercent:(beforeMs/afterMs-1)*100};
  });
  const artifacts={};
  for(const directory of [before,after]) {
    artifacts[resolve(directory)]={};
    for(const file of ['index.js','index.wasm','metadata.json']) {
      artifacts[resolve(directory)][file]=createHash('sha256').update(await readFile(join(directory,file))).digest('hex');
    }
  }
  await writeFile(receipt,JSON.stringify({before:resolve(before),after:resolve(after),artifacts,framesPerSample:50,spritesPerFrame:512,samples,summary,limitations:'Warm private headless Chromium on one shared machine. Host tile drawing only; no claim about whole-game frame time.'},null,2)+'\n');
  console.log(JSON.stringify(summary));
} finally {for(const client of clients) await client.close();}
