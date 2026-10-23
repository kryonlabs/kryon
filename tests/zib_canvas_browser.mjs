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
    const data=await readFile((mode==='url'||mode==='hosted')&&name==='/'+basename(downloaded)?downloaded:join(root,name));
    res.setHeader('Content-Type',extname(name)==='.wasm'?'application/wasm':extname(name)==='.js'?'text/javascript':'text/html');
    res.end(data);
  }catch{res.writeHead(404).end();}
});
await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
const query=mode==='url'||mode==='hosted'?'?bundle='+encodeURIComponent(basename(downloaded)):'';
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
  // Slow runners may take longer than any fixed delay to swap an app.
  async function settle(expression,accept){const end=Date.now()+15000;for(;;){const value=await evaluate(expression);if(accept(value)||Date.now()>end)return value;await delay(100);}}
  await call('Runtime.enable');
  await call('Emulation.setDeviceMetricsOverride',{width:960,height:600,deviceScaleFactor:1,mobile:false});
  if(mode==='services') {
    while(Date.now()<deadline&&!await evaluate('!!document.querySelector("#zib-open")'))await delay(100);
    const document=await call('DOM.getDocument');
    const picker=await call('DOM.querySelector',{nodeId:document.root.nodeId,selector:'#zib-open'});
    await call('DOM.setFileInputFiles',{nodeId:picker.nodeId,files:[resolve(bundleFile)]});
    while(Date.now()<deadline&&!diagnostics.includes('ZIB media, HTTP and clock passed')) {
      assert(!/ZIB service failure|portable execution failed|ABORT|RuntimeError/.test(diagnostics),diagnostics);
      await delay(100);
    }
    assert(diagnostics.includes('ZIB media, HTTP and clock passed'),diagnostics+' '+errors);
    console.log('Browser ZIB audio, asynchronous HTTP and clock passed');
  } else if(mode==='hosted') {
    const imageReady='(()=>{const c=document.querySelector("canvas");if(!c)return false;const p=c.getContext("2d").getImageData(550,130,1,1).data;return p[0]===230&&p[1]===30&&p[2]===40;})()';
    const textPixels='Array.from(document.querySelector("canvas").getContext("2d").getImageData(40,128,300,40).data).join(",")';
    async function ready(){const end=Date.now()+30000;while(Date.now()<end){if(await evaluate(imageReady))return;await delay(100);}throw Error('Hosted app did not render: '+diagnostics+' '+await evaluate('document.querySelector("#zib-controls span").textContent'));}
    await ready();
    const original=await evaluate(textPixels);
    await evaluate(`(() => {
      window.documentFlushes = 0;
      const backend = Module.FS.filesystems.IDBFS;
      const sync = backend.syncfs;
      backend.syncfs = function(mount, populate, callback) {
        if (!populate) documentFlushes++;
        return sync.call(this, mount, populate, callback);
      };
    })()`);
    const position=await evaluate('(()=>{const r=document.querySelector("canvas").getBoundingClientRect();return {x:r.x+100,y:r.y+70};})()');
    await call('Input.dispatchMouseEvent',{type:'mouseMoved',...position});
    await call('Input.dispatchMouseEvent',{type:'mousePressed',...position,button:'left',buttons:1,clickCount:1});
    await delay(150);
    await call('Input.dispatchMouseEvent',{type:'mouseReleased',...position,button:'left',buttons:0,clickCount:1});
    await delay(1600);
    const clicked=await evaluate(textPixels);
    assert.notEqual(clicked,original,'Hosted button did not update app state');
    const flushes=await evaluate('documentFlushes');
    assert(flushes>0&&flushes<=3,'One interaction flushed the whole browser database for individual documents: '+flushes);
    assert(await evaluate(`Array.from({length:8},(_,index)=>Module.FS.readFile('/kryon-apps/portable.probe/batch/'+(index+1),{encoding:'utf8'})).every(value=>value==='saved')`),'A batched document was not written');
    await call('Page.reload');
    await delay(500);
    await ready();
    assert.equal(await evaluate(textPixels),clicked,'Browser reload discarded the app document');
    assert(await evaluate(`Array.from({length:8},(_,index)=>Module.FS.readFile('/kryon-apps/portable.probe/batch/'+(index+1),{encoding:'utf8'})).every(value=>value==='saved')`),'Reload discarded a batched document');
    const document=await call('DOM.getDocument');
    const picker=await call('DOM.querySelector',{nodeId:document.root.nodeId,selector:'#zib-open'});
    await call('DOM.setFileInputFiles',{nodeId:picker.nodeId,files:[resolve(downloaded)]});
    await delay(700);
    assert.equal(await evaluate(textPixels),clicked,'Replacing the same hosted app lost its document');
    await call('DOM.setFileInputFiles',{nodeId:picker.nodeId,files:[join(dirname(downloaded),'invalid.zib')]});
    await delay(400);
    assert.equal(await evaluate(textPixels),clicked,'Invalid replacement discarded the running hosted app');
    await call('DOM.setFileInputFiles',{nodeId:picker.nodeId,files:[join(dirname(downloaded),'hosted_other.zib')]});
    await delay(700);
    assert.equal(await evaluate(textPixels),original,'A different app read the first app document');
    await call('DOM.setFileInputFiles',{nodeId:picker.nodeId,files:[resolve(downloaded)]});
    await delay(700);
    assert.equal(await evaluate(textPixels),clicked,'Switching back lost the first app document');
    assert(!/ABORT|RuntimeError/.test(diagnostics),diagnostics);
    for (const [x, marker, minimum] of [[100, 'Hosted swipe step', 3], [580, 'Hosted joystick step', 3]]) {
      const count = () => diagnostics.split(marker).length - 1;
      const before = count();
      await call('Input.dispatchMouseEvent', {type: 'mousePressed', x, y: 360, button: 'left', buttons: 1, clickCount: 1});
      await delay(1200);
      const during = count();
      await call('Input.dispatchMouseEvent', {type: 'mouseReleased', x, y: 360, button: 'left', buttons: 0, clickCount: 1});
      assert(during - before >= minimum, marker + ' did not hold/repeat: ' + diagnostics);
      await delay(250);
      assert.equal(count(), during, marker + ' continued after release');
    }
    {
      // The direction pad at 640,300 (160 square): its up arm is above the
      // centre 720,380, the right arm beside it, the hub in the middle.
      const origin = await evaluate('(()=>{const r=document.querySelector("canvas").getBoundingClientRect();return {x:r.x,y:r.y};})()');
      const at = (x, y) => ({x: origin.x + x, y: origin.y + y});
      const count = marker => diagnostics.split(marker).length - 1;
      const pads = () => ['left', 'up', 'right', 'down'].reduce((sum, arm) => sum + count('Hosted pad step ' + arm), 0);
      const up = count('Hosted pad step up'), right = count('Hosted pad step right');
      await call('Input.dispatchMouseEvent', {type: 'mousePressed', ...at(720, 325), button: 'left', buttons: 1, clickCount: 1});
      await delay(1200);
      assert(count('Hosted pad step up') - up >= 3, 'Hosted pad did not hold/repeat up: ' + diagnostics);
      await call('Input.dispatchMouseEvent', {type: 'mouseMoved', ...at(775, 380), button: 'left', buttons: 1});
      await delay(400);
      assert(count('Hosted pad step right') > right, 'Hosted pad did not turn when the press slid onto the right arm');
      await call('Input.dispatchMouseEvent', {type: 'mouseMoved', ...at(722, 378), button: 'left', buttons: 1});
      await delay(150);
      const onHub = pads();
      await delay(400);
      assert.equal(pads(), onHub, 'Hosted pad stepped while the press rested on its hub');
      await call('Input.dispatchMouseEvent', {type: 'mouseReleased', ...at(722, 378), button: 'left', buttons: 0, clickCount: 1});
      const released = pads();
      await delay(250);
      assert.equal(pads(), released, 'Hosted pad continued after release');
      const down = count('Hosted pad step down');
      await call('Input.dispatchMouseEvent', {type: 'mousePressed', ...at(720, 440), button: 'left', buttons: 1, clickCount: 1});
      await call('Input.dispatchMouseEvent', {type: 'mouseReleased', ...at(720, 440), button: 'left', buttons: 0, clickCount: 1});
      for (let end = Date.now() + 2000; count('Hosted pad step down') === down && Date.now() < end;) await delay(20);
      assert.equal(count('Hosted pad step down'), down + 1, 'A tap on the down arm did not step exactly once: ' + diagnostics);
    }
    {
      // Dragging on the swipe area reports which way it leans while held.
      const origin = await evaluate('(()=>{const r=document.querySelector("canvas").getBoundingClientRect();return {x:r.x,y:r.y};})()');
      const leans = () => diagnostics.split('Hosted swipe leans right').length - 1;
      const before = leans();
      await call('Input.dispatchMouseEvent', {type: 'mousePressed', x: origin.x + 80, y: origin.y + 380, button: 'left', buttons: 1, clickCount: 1});
      for (const dx of [20, 40, 60, 90]) {
        await call('Input.dispatchMouseEvent', {type: 'mouseMoved', x: origin.x + 80 + dx, y: origin.y + 380, button: 'left', buttons: 1});
        await delay(60);
      }
      for (let end = Date.now() + 2000; leans() === before && Date.now() < end;) await delay(20);
      await call('Input.dispatchMouseEvent', {type: 'mouseReleased', x: origin.x + 170, y: origin.y + 380, button: 'left', buttons: 0, clickCount: 1});
      assert(leans() > before, 'A drag to the right did not report its lean: ' + diagnostics);
      await delay(300);
    }
    const beforeKey = diagnostics.split('Hosted key step').length;
    for (let at = 0; at < 5; at++) {
      await call('Input.dispatchKeyEvent', {type: 'keyDown', key: 'ArrowLeft', code: 'ArrowLeft', windowsVirtualKeyCode: 37, autoRepeat: at > 0});
      await delay(100);
    }
    await call('Input.dispatchKeyEvent', {type: 'keyUp', key: 'ArrowLeft', code: 'ArrowLeft', windowsVirtualKeyCode: 37});
    assert.equal(diagnostics.split('Hosted key step').length - beforeKey, 5, 'Hosted key repeats were dropped: ' + diagnostics);
    await delay(200);
    assert.equal(diagnostics.split('Hosted key step').length - beforeKey, 5, 'Keys repeated after release');
    const beforeTouch = diagnostics.split('Hosted swipe step').length;
    await call('Input.dispatchTouchEvent', {type: 'touchStart', touchPoints: [{x: 100, y: 360, id: 1}]});
    await delay(1000);
    const duringTouch = diagnostics.split('Hosted swipe step').length;
    const beforeCancel = diagnostics.split('Hosted swipe released').length;
    await call('Input.dispatchTouchEvent', {type: 'touchCancel', touchPoints: []});
    for (let end = Date.now() + 5000; diagnostics.split('Hosted swipe released').length === beforeCancel && Date.now() < end;) await delay(20);
    assert(diagnostics.split('Hosted swipe released').length > beforeCancel, 'Touch cancellation did not release the control');
    const afterCancel = diagnostics.split('Hosted swipe step').length;
    assert(duringTouch - beforeTouch >= 3, 'Touch hold did not repeat: ' + diagnostics);
    await delay(300);
    assert.equal(diagnostics.split('Hosted swipe step').length, afterCancel, 'Touch cancellation left movement held');
    await call('Emulation.setDeviceMetricsOverride', {width: 1000, height: 800, deviceScaleFactor: 1, mobile: false});
    await delay(500);
    const endCount = () => diagnostics.split('Hosted scroll reached end').length;
    const scrollOrigin = await evaluate('(()=>{const r=document.querySelector("canvas").getBoundingClientRect();return {x:r.x,y:r.y};})()');
    const beforeWheel = endCount();
    await call('Input.dispatchMouseEvent', {type: 'mouseMoved', x: scrollOrigin.x + 180, y: scrollOrigin.y + 510});
    await delay(100);
    await call('Input.dispatchMouseEvent', {type: 'mouseWheel', x: scrollOrigin.x + 180, y: scrollOrigin.y + 510, deltaX: 0, deltaY: 1200});
    for (let end = Date.now() + 5000; endCount() === beforeWheel && Date.now() < end;) await delay(20);
    assert(endCount() > beforeWheel, 'Hosted scroll wheel could not reach the last row: ' + diagnostics);
    await call('Input.dispatchMouseEvent', {type: 'mouseWheel', x: scrollOrigin.x + 180, y: scrollOrigin.y + 510, deltaX: 0, deltaY: -1200});
    await delay(600);
    const beforeDrag = endCount();
    await call('Input.dispatchMouseEvent', {type: 'mousePressed', x: scrollOrigin.x + 335, y: scrollOrigin.y + 470, button: 'left', buttons: 1, clickCount: 1});
    await delay(100);
    await call('Input.dispatchMouseEvent', {type: 'mouseMoved', x: scrollOrigin.x + 335, y: scrollOrigin.y + 574, buttons: 1});
    await delay(100);
    await call('Input.dispatchMouseEvent', {type: 'mouseReleased', x: scrollOrigin.x + 335, y: scrollOrigin.y + 574, button: 'left', buttons: 0, clickCount: 1});
    for (let end = Date.now() + 5000; endCount() === beforeDrag && Date.now() < end;) await delay(20);
    assert(endCount() > beforeDrag, 'Hosted scrollbar could not reach the last row: ' + diagnostics);
    console.log('Browser hosted ZIB images, widgets, persistence, isolation, replacement and scrolling passed');
  } else if(mode==='gallery') {
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
    const rejected=/not a valid ZIB|invalid|capability/;
    assert.match(await settle('document.querySelector("#zib-controls span").textContent',v=>rejected.test(v)),rejected);
    assert.equal(await evaluate(textPixels),clicked,'Invalid file disturbed the running app');
    await evaluate('Module.FS.writeFile("/tmp/kryon-zib/private.png",Module.FS.readFile("/tmp/kryon-zib/assets/red.png"))');
    await call('DOM.setFileInputFiles',{nodeId:picker.nodeId,files:[downloaded]});
    assert.equal(await settle(textPixels,v=>v===before),before,'Opening the downloaded ZIB did not create a fresh instance');
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
