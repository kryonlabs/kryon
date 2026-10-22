// Drive only the private headless process created here; no desktop connection.
import {spawn} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {join} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
const [html,profile]=process.argv.slice(2);
const dpi = process.env.CANVAS_TEST_DPI || '1';
const gpu = process.env.CANVAS_TEST_MESH_GPU === '1';
const rendering = gpu ? ['--enable-unsafe-swiftshader','--enable-webgl','--use-gl=angle','--use-angle=swiftshader'] : ['--disable-gpu','--disable-webgl'];
const browser=spawn(process.env.CHROMIUM||'chromium',['--headless','--no-sandbox',...rendering,'--disable-dev-shm-usage','--no-first-run','--no-default-browser-check','--force-device-scale-factor='+dpi,'--remote-debugging-port=0','--user-data-dir='+profile,'about:blank'],{detached:true,stdio:['ignore','ignore','pipe']});
let errors='';
browser.stderr.on('data',data=>{errors=(errors+data).slice(-8000);});
let socket;
try {
  const deadline=Date.now()+25000;
  let port;
  while(Date.now()<deadline) {
    try {port=(await readFile(join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0];break;} catch {}
    if(browser.exitCode!==null) throw new Error('Chromium exited: '+errors);
    await delay(50);
  }
  if(!port) throw new Error('Chromium debug endpoint unavailable: '+errors);
  const pages=await(await fetch('http://127.0.0.1:'+port+'/json/list')).json();
  const page=pages.find(page=>page.type==='page');
  socket=new WebSocket(page.webSocketDebuggerUrl);
  await new Promise((resolve,reject)=>{socket.addEventListener('open',resolve,{once:true});socket.addEventListener('error',reject,{once:true});});
  const pending=new Map(); let sequence=0; let pageDiagnostics="";
  socket.addEventListener('message',event=>{const message=JSON.parse(event.data);if(message.method==='Runtime.consoleAPICalled'||message.method==='Runtime.exceptionThrown'){pageDiagnostics=(pageDiagnostics+JSON.stringify(message.params||{})).slice(-4000);}if(pending.has(message.id)){pending.get(message.id)(message);pending.delete(message.id);}});
  async function command(method,params={}) {
    const id=++sequence;
    const result=new Promise(resolve=>pending.set(id,resolve));
    socket.send(JSON.stringify({id,method,params}));return result;
  }
  await command('Runtime.enable');
  await command('Page.enable');
  if(!gpu) await command('Page.addScriptToEvaluateOnNewDocument',{source:`
    for(const prototype of [HTMLCanvasElement.prototype,OffscreenCanvas.prototype]) {
      const original=prototype.getContext;
      prototype.getContext=function(kind,...args) {
        if(kind==='webgl'||kind==='webgl2'||kind==='experimental-webgl') return null;
        return original.call(this,kind,...args);
      };
    }
  `});
  await command('Page.navigate',{url:html});
  let result='pending';
  while(Date.now()<deadline) {
    const response=await command('Runtime.evaluate',{expression:'document.getElementById("result")?.textContent',returnByValue:true});
    result=response.result?.result?.value;
    if(result==='PASS') {
      const measured=await command('Runtime.evaluate',{expression:'globalThis.canvasTestMeshDraws || 0',returnByValue:true});
      const draws=measured.result?.result?.value || 0;
      if(gpu ? draws < 1 : draws !== 0) throw new Error('Unexpected accelerated mesh count: '+draws+' gpu='+gpu);
      console.log('Canvas2D browser: PASS ('+(gpu?'accelerated':'software')+' meshes)');break;
    }
    if(result && result!=='pending') throw new Error(result+' '+pageDiagnostics);
    await delay(50);
  }
  if(result!=='PASS') {
    const state=await command('Runtime.evaluate',{expression:'JSON.stringify({module:typeof Module,cap:globalThis.__kryonCanvas,result:document.getElementById("result")?.textContent})',returnByValue:true});
    pageDiagnostics+=' '+String(state.result?.result?.value);
    throw new Error('Canvas test timed out: '+result+' '+errors+' '+pageDiagnostics);
  }
} finally {
  if(socket) socket.close();
  try {process.kill(-browser.pid,'SIGTERM');} catch {}
  await new Promise(resolve=>{if(browser.exitCode!==null || browser.signalCode!==null) resolve();else browser.once('exit',resolve);});
}
