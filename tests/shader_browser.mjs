// Own headless Chromium only. Pixel assertions execute inside the WASM host.
import {spawn} from 'node:child_process';
import {readFile} from 'node:fs/promises';
import {setTimeout as delay} from 'node:timers/promises';
const [url,profile]=process.argv.slice(2);
const env={...process.env,HARMONY_DESKTOP_RECOVERY:'0',YUE_DESKTOP_RECOVERY:'0'};
for(const key of ['DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER'])delete env[key];
const browser=spawn(process.env.CHROMIUM||'chromium',['--headless','--no-sandbox','--enable-unsafe-swiftshader','--enable-webgl','--use-gl=angle','--use-angle=swiftshader','--disable-dev-shm-usage','--no-first-run','--remote-debugging-port=0','--user-data-dir='+profile,'about:blank'],{env,detached:true,stdio:['ignore','ignore','pipe']});
let errors='',socket;
browser.stderr.on('data',d=>{errors=(errors+d).slice(-3000)});
try{
 let port;
 const deadline=Date.now()+45000;
 while(Date.now()<deadline){try{[port]=(await readFile(profile+'/DevToolsActivePort','utf8')).split('\n');break}catch{}await delay(50)}
 if(!port)throw Error('Private Chromium unavailable: '+errors);
 const pages=await(await fetch('http://127.0.0.1:'+port+'/json/list')).json();
 socket=new WebSocket(pages.find(p=>p.type==='page').webSocketDebuggerUrl);
 await new Promise((resolve,reject)=>{socket.addEventListener('open',resolve,{once:true});socket.addEventListener('error',reject,{once:true})});
 const pending=new Map();let id=0,diagnostics='';
 socket.addEventListener('message',e=>{const m=JSON.parse(e.data);if(m.method==='Runtime.consoleAPICalled'||m.method==='Runtime.exceptionThrown')diagnostics=(diagnostics+JSON.stringify(m.params)).slice(-4000);const p=pending.get(m.id);if(p){clearTimeout(p.timer);pending.delete(m.id);m.error?p.reject(Error(JSON.stringify(m.error))):p.resolve(m)}});
 const command=(method,params={})=>new Promise((resolve,reject)=>{const n=++id;const timer=setTimeout(()=>{pending.delete(n);reject(Error(method+' timed out'))},10000);pending.set(n,{resolve,reject,timer});socket.send(JSON.stringify({id:n,method,params}))});
 await command('Runtime.enable');await command('Page.enable');await command('Page.navigate',{url});
 let passed=false;
 while(Date.now()<deadline){const m=await command('Runtime.evaluate',{expression:'document.getElementById("result")?.textContent',returnByValue:true});const v=m.result?.result?.value;if(v==='PASS'){passed=true;break}if(v?.startsWith('FAIL'))throw Error(v+' '+diagnostics);await delay(50)}
 if(!passed)throw Error('Shader WASM timed out: '+diagnostics+' '+errors);
 console.log('Raylib WebAssembly shader host pixels/lifecycle: PASS');
}finally{
 socket?.close();
 try{process.kill(-browser.pid,'SIGTERM')}catch{}
 await Promise.race([new Promise(r=>browser.exitCode!==null?r():browser.once('exit',r)),delay(2000)]);
 if(browser.exitCode===null&&browser.signalCode===null){try{process.kill(-browser.pid,'SIGKILL')}catch{}}
}
