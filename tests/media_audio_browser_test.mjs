// Test tooling: one private, muted headless browser with the default autoplay policy.
import {spawn} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {join} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
const [url,profile]=process.argv.slice(2);
const browser=spawn(process.env.CHROMIUM||'chromium',[
  '--headless','--no-sandbox','--disable-gpu','--disable-dev-shm-usage',
  '--no-first-run','--no-default-browser-check','--mute-audio',
  '--remote-debugging-port=0','--user-data-dir='+profile,'about:blank'
],{stdio:['ignore','ignore','pipe']});
let diagnostics='',socket;
browser.stderr.on('data',d=>diagnostics=(diagnostics+d).slice(-2500));
try {
  const deadline=Date.now()+30000;
  let port;
  while(Date.now()<deadline){
    try{port=(await readFile(join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0];break;}catch{}
    if(browser.exitCode!==null) throw new Error('Browser exited '+diagnostics);
    await delay(30);
  }
  if(!port) throw new Error('No private CDP endpoint '+diagnostics);
  const pages=await(await fetch('http://127.0.0.1:'+port+'/json/list')).json();
  socket=new WebSocket(pages.find(p=>p.type==='page').webSocketDebuggerUrl);
  await new Promise((resolve,reject)=>{socket.addEventListener('open',resolve,{once:true});socket.addEventListener('error',reject,{once:true});});
  let id=0;const pending=new Map();
  socket.addEventListener('message',event=>{
    const data=JSON.parse(event.data);
    if(pending.has(data.id)){pending.get(data.id)(data);pending.delete(data.id);}
    if(data.method==='Runtime.exceptionThrown')diagnostics=(diagnostics+JSON.stringify(data.params)).slice(-2500);
  });
  async function call(method,params={}){const key=++id;const p=new Promise(r=>pending.set(key,r));socket.send(JSON.stringify({id:key,method,params}));return p;}
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
  await call('Browser.close');
}finally{
  if(socket)socket.close();
  if(browser.exitCode===null&&browser.signalCode===null)browser.kill('SIGTERM');
  await new Promise(r=>{if(browser.exitCode!==null||browser.signalCode!==null)r();else browser.once('exit',r);});
}
