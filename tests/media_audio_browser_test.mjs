// Test tooling: one private, muted headless browser with the default autoplay policy.
import {spawn} from 'node:child_process';
import {readFile, stat} from 'node:fs/promises';
import {join} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
const [url,profile]=process.argv.slice(2);
const launchedAt=Date.now();
const environment={...process.env,YUE_DESKTOP_RECOVERY:'0'};
for(const key of ['DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER','GDK_DISPLAY'])delete environment[key];
const browser=spawn(process.env.CHROMIUM||'chromium',[
  '--headless','--no-sandbox','--disable-gpu','--use-gl=disabled','--disable-dev-shm-usage',
  '--no-first-run','--no-default-browser-check','--mute-audio',
  '--remote-debugging-port=0','--user-data-dir='+profile,'about:blank'
],{env:environment,detached:true,stdio:['ignore','ignore','pipe']});
let diagnostics='',socket;
const pending=new Map();let id=0;
browser.stderr.on('data',d=>diagnostics=(diagnostics+d).slice(-2500));
try {
  const deadline=Date.now()+30000;
  let port,endpoint;
  while(Date.now()<deadline){
    try{
      const path=join(profile,'DevToolsActivePort');
      if((await stat(path)).mtimeMs>=launchedAt){[port,endpoint]=(await readFile(path,'utf8')).split('\n');break;}
    }catch{}
    if(browser.exitCode!==null||browser.signalCode!==null)throw new Error('Browser exited '+diagnostics);
    await delay(30);
  }
  if(!port)throw new Error('No private CDP endpoint '+diagnostics);
  const request=path=>fetch('http://127.0.0.1:'+port+path,{signal:AbortSignal.timeout(Math.max(1,deadline-Date.now()))});
  const version=await(await request('/json/version')).json();
  if(new URL(version.webSocketDebuggerUrl).pathname!==endpoint)throw new Error('Endpoint does not match private audio browser');
  const pages=await(await request('/json/list')).json();
  socket=new WebSocket(pages.find(p=>p.type==='page').webSocketDebuggerUrl);
  await new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(new Error('Private audio browser connection timed out')),10000);
    socket.addEventListener('open',()=>{clearTimeout(timer);resolve();},{once:true});
    socket.addEventListener('error',error=>{clearTimeout(timer);reject(error);},{once:true});
  });
  socket.addEventListener('message',event=>{
    const data=JSON.parse(event.data),item=pending.get(data.id);
    if(item){clearTimeout(item.timer);pending.delete(data.id);data.error?item.reject(new Error(JSON.stringify(data.error))):item.resolve(data);}
    if(data.method==='Runtime.exceptionThrown')diagnostics=(diagnostics+JSON.stringify(data.params)).slice(-2500);
  });
  socket.addEventListener('close',()=>{
    for(const item of pending.values()){clearTimeout(item.timer);item.reject(new Error('Private audio browser disconnected '+diagnostics));}
    pending.clear();
  });
  async function call(method,params={}){
    const key=++id;
    const result=new Promise((resolve,reject)=>{
      const timer=setTimeout(()=>{pending.delete(key);reject(new Error(method+' timed out '+diagnostics));},Math.max(1,Math.min(10000,deadline-Date.now())));
      pending.set(key,{resolve,reject,timer});
    });
    socket.send(JSON.stringify({id:key,method,params}));return result;
  }
  await call('Runtime.enable');await call('Page.enable');
  await call('Page.navigate',{url});
  let gesture=false,passed=false;
  while(Date.now()<deadline){
    const r=await call('Runtime.evaluate',{expression:'JSON.stringify({phase:globalThis.audioTestPhase,result:document.getElementById("result")?.textContent})',returnByValue:true});
    const state=JSON.parse(r.result?.result?.value||'{}');
    if(state.phase==='gesture'&&!gesture){
      await call('Input.dispatchMouseEvent',{type:'mousePressed',x:20,y:20,button:'left',clickCount:1});
      await call('Input.dispatchMouseEvent',{type:'mouseReleased',x:20,y:20,button:'left',clickCount:1});
      gesture=true;
    }
    if(state.result==='PASS'){passed=true;break;}
    if(state.result&&state.result!=='pending')throw new Error(state.result+' '+diagnostics);
    await delay(30);
  }
  if(!passed||!gesture)throw new Error('Audio browser test timed out '+diagnostics);
  console.log('actual browser: suspended before trusted gesture; independent voices, frame persistence and cleanup passed');
}finally{
  if(socket)socket.close();
  const exited=new Promise(resolve=>{if(browser.exitCode!==null||browser.signalCode!==null)resolve();else browser.once('exit',resolve);});
  try{process.kill(-browser.pid,'SIGTERM');}catch{}
  await Promise.race([exited,delay(2000)]);
  if(browser.exitCode===null&&browser.signalCode===null){
    try{process.kill(-browser.pid,'SIGKILL');}catch{}
    await Promise.race([exited,delay(2000)]);
  }
  browser.stderr.destroy();
}
