// A private headless browser and loopback server; no owner browser or display.
import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {readFile,writeFile,mkdir,mkdtemp,rm} from 'node:fs/promises';
import {createServer} from 'node:http';
import {join,resolve,extname,basename,dirname} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
const [html,mode='keyboard',bundleFile]=process.argv.slice(2);
const downloaded=bundleFile||join(dirname(html),'zib_canvas_probe-web.zib');
const root=dirname(html),profile=await mkdtemp(join(root,'browser-profile-'));
const server=createServer(async(req,res)=>{
  try {
    const name=decodeURIComponent(new URL(req.url,'http://localhost').pathname);
    if(name.includes('..'))throw Error('invalid path');
    const data=await readFile(mode==='url'&&name==='/'+basename(downloaded)?downloaded:join(root,name));
    res.setHeader('Content-Type',extname(name)==='.wasm'?'application/wasm':extname(name)==='.js'?'text/javascript':'text/html');
    res.end(data);
  }catch{res.writeHead(404).end();}
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const query=mode==='url'?'?bundle='+encodeURIComponent(basename(downloaded)):'';
const browser=spawn(process.env.CHROMIUM||'chromium',['--headless','--no-sandbox','--disable-gpu','--disable-dev-shm-usage','--no-first-run','--no-default-browser-check','--remote-debugging-port=0','--user-data-dir='+profile,'http://127.0.0.1:'+server.address().port+'/'+basename(html)+query],{detached:true,stdio:['ignore','ignore','pipe']});
let errors='',socket,diagnostics='';browser.stderr.on('data',v=>errors=(errors+v).slice(-2000));
try {
  const deadline=Date.now()+45000;
  let port;
  while(Date.now()<deadline){
    try {port=(await readFile(join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0];break;}catch{}
    if(browser.exitCode!==null)throw Error(errors);
    await delay(50);
  }
  assert(port,'Chromium debug endpoint unavailable: '+errors);
  const pages=await(await fetch('http://127.0.0.1:'+port+'/json/list')).json();
  socket=new WebSocket(pages.find(p=>p.type==='page').webSocketDebuggerUrl);
  await new Promise((resolve,reject)=>{socket.addEventListener('open',resolve,{once:true});socket.addEventListener('error',reject,{once:true});});
  let seq=0;const pending=new Map();
  socket.addEventListener('message',e=>{
    const m=JSON.parse(e.data);
    if(m.method==='Runtime.consoleAPICalled')diagnostics+=m.params.args.map(a=>a.value||a.description||'').join(' ')+'\n';
    if(m.method==='Runtime.exceptionThrown')diagnostics+=JSON.stringify(m.params)+'\n';
    if(pending.has(m.id)){pending.get(m.id)(m);pending.delete(m.id);}
  });
  async function call(method,params={}){
    const id=++seq;const response=new Promise((resolve,reject)=>{pending.set(id,resolve);setTimeout(()=>{if(pending.delete(id))reject(Error(method+' timed out '+diagnostics));},15000).unref();});
    socket.send(JSON.stringify({id,method,params}));
    const result=await response;assert(!result.error,JSON.stringify(result));return result.result;
  }
  async function evaluate(expression){const r=await call('Runtime.evaluate',{expression,returnByValue:true});assert(!r.exceptionDetails,JSON.stringify(r));return r.result.value;}
  await call('Runtime.enable');
  await call('Emulation.setDeviceMetricsOverride',{width:960,height:600,deviceScaleFactor:1,mobile:false});
  if(mode==='gallery') {
    while(Date.now()<deadline&&!await evaluate('!!document.querySelector("#zib-open")'))await delay(100);
    const document=await call('DOM.getDocument');
    const picker=await call('DOM.querySelector',{nodeId:document.root.nodeId,selector:'#zib-open'});
    const entries=JSON.parse(await readFile(bundleFile,'utf8'));
    const shots=join(dirname(bundleFile),'screenshots-browser');
    await mkdir(shots,{recursive:true});
    for(const entry of entries) {
      await call('DOM.setFileInputFiles',{nodeId:picker.nodeId,files:[resolve(dirname(bundleFile),entry.bundle)]});
      await delay(600);
      const state=await evaluate(`(()=>{const c=document.querySelector('canvas');const p=c.getContext('2d').getImageData(0,0,c.width,c.height).data;let ink=0;for(let i=0;i<p.length;i+=4)if(p[i]+p[i+1]+p[i+2]<500)ink++;return {status:document.querySelector('#zib-controls span').textContent,ink};})()`);
      assert.equal(state.status,'',entry.id+': '+state.status+' '+diagnostics);
      assert(state.ink>10,'Empty browser example: '+entry.id);
      assert(!/portable execution failed|ABORT|RuntimeError/.test(diagnostics),entry.id+': '+diagnostics);
      const screenshot=await call('Page.captureScreenshot',{format:'png'});
      await writeFile(join(shots,entry.id.replaceAll('/','-')+'.png'),Buffer.from(screenshot.data,'base64'));
      console.log('Browser rendered '+entry.id+': '+entry.name);
    }
    console.log('All '+entries.length+' full gallery ZIBs rendered through the browser VM');
  } else {
  if(mode==='file') {
    while(Date.now()<deadline&&!await evaluate('!!document.querySelector("#zib-open")'))await delay(100);
    const document=await call('DOM.getDocument');
    const picker=await call('DOM.querySelector',{nodeId:document.root.nodeId,selector:'#zib-open'});
    await call('DOM.setFileInputFiles',{nodeId:picker.nodeId,files:[downloaded]});
  }
  let ready=false;
  while(Date.now()<deadline){
    ready=await evaluate('(()=>{const c=document.querySelector("canvas");if(!c||c.width!==960)return false;const p=c.getContext("2d").getImageData(550,130,1,1).data;return p[0]===230&&p[1]===30&&p[2]===40;})()');
    if(ready)break;
    assert(!/portable execution failed|ABORT|FAIL/.test(diagnostics),diagnostics);
    await delay(100);
  }
  assert(ready,'ZIB image asset did not render: '+diagnostics+' '+errors);
  assert(await evaluate('(()=>{const c=document.querySelector("canvas").getContext("2d");const p=c.getImageData(10,10,1,1).data;if(p[0]!==248||p[1]!==250||p[2]!==252)return false;return [...c.getImageData(40,128,300,40).data].filter((v,i)=>i%4!==3&&v<100).length>30;})()'),'ZIB background or text did not render');
  const textPixels='Array.from(document.querySelector("canvas").getContext("2d").getImageData(40,128,300,40).data).join(",")';
  const before=await evaluate(textPixels);
  const position=await evaluate('(()=>{const r=document.querySelector("canvas").getBoundingClientRect();return {x:r.x+100,y:r.y+70};})()');
  await call('Input.dispatchMouseEvent',{type:'mouseMoved',...position});
  await call('Input.dispatchMouseEvent',{type:'mousePressed',...position,button:'left',buttons:1,clickCount:1});
  await delay(150);
  await call('Input.dispatchMouseEvent',{type:'mouseReleased',...position,button:'left',buttons:0,clickCount:1});
  await delay(400);
  const clicked=await evaluate(textPixels);
  assert.notEqual(clicked,before,'ZIB click did not update retained app state');
  // Resize the same instance; its click state must survive.
  await call('Emulation.setDeviceMetricsOverride',{width:1000,height:660,deviceScaleFactor:1,mobile:false});
  await delay(300);
  assert.equal(await evaluate(textPixels),clicked,'Resize discarded the ZIB app state');
  if(mode==='keyboard') {
    const document=await call('DOM.getDocument');
    const picker=await call('DOM.querySelector',{nodeId:document.root.nodeId,selector:'#zib-open'});
    assert(picker.nodeId,'The generic player has no ZIB file picker');
    await call('DOM.setFileInputFiles',{nodeId:picker.nodeId,files:[join(dirname(downloaded),'invalid.zib')]});
    await delay(400);
    assert.match(await evaluate('document.querySelector("#zib-controls span").textContent'),/invalid|capability/);
    assert.equal(await evaluate(textPixels),clicked,'Invalid file disturbed the running app');
    await evaluate('Module.FS.writeFile("/tmp/kryon-zib/private.png",Module.FS.readFile("/tmp/kryon-zib/assets/red.png"))');
    await call('DOM.setFileInputFiles',{nodeId:picker.nodeId,files:[downloaded]});
    await delay(600);
    assert.equal(await evaluate(textPixels),before,'Opening the downloaded ZIB did not create a fresh instance');
    await call('Input.dispatchMouseEvent',{type:'mousePressed',...position,button:'left',buttons:1,clickCount:1});
    await delay(150);
    await call('Input.dispatchMouseEvent',{type:'mouseReleased',...position,button:'left',buttons:0,clickCount:1});
    await delay(300);
  }
  if(mode==='unicode')await call('Input.dispatchKeyEvent',{type:'char',key:'é',text:'é',unmodifiedText:'é'});
  else await call('Input.dispatchKeyEvent',{type:'keyDown',key:'a',code:'KeyA',windowsVirtualKeyCode:65,nativeVirtualKeyCode:65,text:'a'});
  while(Date.now()<deadline&&!diagnostics.includes('zib interaction passed'))await delay(100);
  assert(diagnostics.includes('zib interaction passed'),diagnostics);
  assert(!/interaction failed|portable execution failed|ABORT/.test(diagnostics),diagnostics);
  console.log('ZIB Canvas '+mode+': image, text, click state, resize and input passed');
  }
} finally {
  socket?.close();try {process.kill(-browser.pid,'SIGTERM');}catch {}
  await new Promise(resolve=>browser.exitCode!==null||browser.signalCode!==null?resolve():browser.once('exit',resolve));
  await new Promise(resolve=>server.close(resolve));await rm(profile,{recursive:true,force:true,maxRetries:10,retryDelay:100});
}
