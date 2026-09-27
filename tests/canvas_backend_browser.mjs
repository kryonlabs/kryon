// Drive only the private headless process created here; no desktop connection.
import {spawn} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {join} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
const [html,profile]=process.argv.slice(2);
const browser=spawn(process.env.CHROMIUM||'chromium',['--headless','--no-sandbox','--disable-gpu','--disable-dev-shm-usage','--no-first-run','--no-default-browser-check','--remote-debugging-port=0','--user-data-dir='+profile,html],{detached:true,stdio:['ignore','ignore','pipe']});
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
  let result='pending';
  while(Date.now()<deadline) {
    const response=await command('Runtime.evaluate',{expression:'document.getElementById("result")?.textContent',returnByValue:true});
    result=response.result?.result?.value;
    if(result==='PASS') {console.log('Canvas2D browser: PASS');break;}
    if(result && result!=='pending') throw new Error(result+' '+pageDiagnostics);
    await delay(50);
  }
  if(result!=='PASS') {
    const state=await command('Runtime.evaluate',{expression:'JSON.stringify({module:typeof Module,cap:globalThis.__kryCanvas,result:document.getElementById("result")?.textContent})',returnByValue:true});
    pageDiagnostics+=' '+String(state.result?.result?.value);
    throw new Error('Canvas test timed out: '+result+' '+errors+' '+pageDiagnostics);
  }
} finally {
  if(socket) socket.close();
  try {process.kill(-browser.pid,'SIGTERM');} catch {}
  await new Promise(resolve=>{if(browser.exitCode!==null || browser.signalCode!==null) resolve();else browser.once('exit',resolve);});
}
