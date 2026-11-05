// Drive only the private headless process created here; no desktop connection.
import {spawn} from 'node:child_process';
import {readFile,stat} from 'node:fs/promises';
import {join} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
const [html,profile]=process.argv.slice(2);
const dpi = process.env.CANVAS_TEST_DPI || '1';
const gpu = process.env.CANVAS_TEST_MESH_GPU === '1';
const rendering = gpu ? ['--enable-unsafe-swiftshader','--enable-webgl','--use-gl=angle','--use-angle=swiftshader'] : ['--disable-gpu','--use-gl=disabled','--disable-webgl'];
const launchedAt=Date.now();
const env={...process.env,YUE_DESKTOP_RECOVERY:'0'};
for(const name of ['DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER']) delete env[name];
const browser=spawn(process.env.CHROMIUM||'chromium',['--headless','--no-sandbox',...rendering,'--disable-dev-shm-usage','--no-first-run','--no-default-browser-check','--force-device-scale-factor='+dpi,'--remote-debugging-port=0','--user-data-dir='+profile,'about:blank'],{env,detached:true,stdio:['ignore','ignore','pipe']});
let errors='';
browser.stderr.on('data',data=>{errors=(errors+data).slice(-8000);});
let socket;
try {
  const deadline=Date.now()+25000;
  let port,endpoint;
  while(Date.now()<deadline) {
    try {
      const path=join(profile,'DevToolsActivePort');
      if((await stat(path)).mtimeMs>=launchedAt) {
        [port,endpoint]=(await readFile(path,'utf8')).split('\n');break;
      }
    } catch {}
    if(browser.exitCode!==null) throw new Error('Chromium exited: '+errors);
    await delay(50);
  }
  if(!port) throw new Error('Chromium debug endpoint unavailable: '+errors);
  const request=path=>fetch('http://127.0.0.1:'+port+path,{signal:AbortSignal.timeout(Math.max(1,deadline-Date.now()))});
  const version=await(await request('/json/version')).json();
  if(new URL(version.webSocketDebuggerUrl).pathname!==endpoint) throw new Error('Chromium endpoint does not match this private profile');
  const pages=await(await request('/json/list')).json();
  const page=pages.find(page=>page.type==='page');
  socket=new WebSocket(page.webSocketDebuggerUrl);
  await new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(new Error('Chromium WebSocket connection timed out: '+errors)),Math.max(1,Math.min(10000,deadline-Date.now())));
    socket.addEventListener('open',()=>{clearTimeout(timer);resolve();},{once:true});
    socket.addEventListener('error',error=>{clearTimeout(timer);reject(error);},{once:true});
  });
  const pending=new Map(); let sequence=0; let pageDiagnostics="";
  socket.addEventListener('message',event=>{const message=JSON.parse(event.data);if(message.method==='Runtime.consoleAPICalled'||message.method==='Runtime.exceptionThrown'){pageDiagnostics=(pageDiagnostics+JSON.stringify(message.params||{})).slice(-4000);}if(pending.has(message.id)){const request=pending.get(message.id);clearTimeout(request.timer);pending.delete(message.id);if(message.error)request.reject(new Error(JSON.stringify(message.error)));else request.resolve(message);}});
  socket.addEventListener('close',()=>{for(const request of pending.values()){clearTimeout(request.timer);request.reject(new Error('Chromium connection closed: '+errors));}pending.clear();});
  async function command(method,params={}) {
    const id=++sequence;
    const result=new Promise((resolve,reject)=>{
      const timer=setTimeout(()=>{pending.delete(id);reject(new Error(method+' timed out: '+pageDiagnostics+' '+errors));},Math.max(1,Math.min(10000,deadline-Date.now())));
      pending.set(id,{resolve,reject,timer});
    });
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
  const exited=new Promise(resolve=>{if(browser.exitCode!==null || browser.signalCode!==null) resolve();else browser.once('exit',resolve);});
  await Promise.race([exited,delay(3000)]);
  if(browser.exitCode===null && browser.signalCode===null) {
    try {process.kill(-browser.pid,'SIGKILL');} catch {}
    await Promise.race([exited,delay(2000)]);
  }
  browser.stderr.destroy();
}
