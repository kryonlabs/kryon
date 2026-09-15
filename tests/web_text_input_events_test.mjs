import {spawn} from "node:child_process";
import {writeFileSync} from "node:fs";
import assert from "node:assert/strict";

const work = process.argv[2];
const browser = spawn("chromium", ["--headless=new", "--no-sandbox", "--disable-gpu",
  "--allow-file-access-from-files", "--remote-debugging-port=0",
  `--user-data-dir=${work}/events-profile`, "about:blank"]);
let socket;
try {
  const endpoint = await new Promise((resolve, reject) => {
    const timeout = setTimeout(() => reject(Error("Chromium startup timed out")), 15000);
    browser.stderr.on("data", chunk => {
      const match = String(chunk).match(/DevTools listening on (ws:\/\/\S+)/);
      if (match) { clearTimeout(timeout); resolve(match[1]); }
    });
    browser.on("error", reject);
  });
  socket = new WebSocket(endpoint);
  await new Promise(resolve => socket.addEventListener("open", resolve, {once: true}));
  let id = 0;
  const pending = new Map();
  socket.addEventListener("message", event => {
    const response = JSON.parse(event.data);
    const callback = pending.get(response.id);
    if (callback) { pending.delete(response.id); callback(response); }
  });
  function send(method, params = {}, sessionId) {
    return new Promise((resolve, reject) => {
      const request = ++id;
      pending.set(request, response => response.error ? reject(Error(JSON.stringify(response.error))) : resolve(response.result));
      socket.send(JSON.stringify({id: request, method, params, sessionId}));
    });
  }
  const {targetId} = await send("Target.createTarget", {url: `file://${work}/index.html`});
  const {sessionId} = await send("Target.attachToTarget", {targetId, flatten: true});
  const call = (method, params) => send(method, params, sessionId);
  async function evaluate(expression) {
    const response = await call("Runtime.evaluate", {expression, returnByValue: true, awaitPromise: true});
    if (response.exceptionDetails) throw Error(JSON.stringify(response.exceptionDetails));
    return response.result.value;
  }
  await evaluate(`new Promise((resolve,reject)=>{
    const deadline=Date.now()+5000;
    const poll=()=>window.textTest?resolve(true):Date.now()>deadline?reject(Error(document.querySelector('#result').textContent)):setTimeout(poll,20);
    poll();
  })`);
  await evaluate(`textTest.state.notes='alpha beta gamma delta epsilon zeta eta theta iota';textTest.draw();
    window.area=document.querySelector('textarea');area.style.width='130px';area.style.height='55px';
    area.focus();area.setSelectionRange(0,0);`);
  async function key(name, code, modifiers = 0) {
    await call("Input.dispatchKeyEvent", {type: "keyDown", key: name, code: name, windowsVirtualKeyCode: code, modifiers});
    await call("Input.dispatchKeyEvent", {type: "keyUp", key: name, code: name, windowsVirtualKeyCode: code, modifiers});
  }
  await key("ArrowDown", 40);
  assert.ok(await evaluate("area.selectionStart>0 && textTest.state.notesCursor===area.selectionStart"), "wrapped Down and cursor ownership");
  await key("End", 35);
  const lineEnd = await evaluate("area.selectionStart");
  await key("Home", 36);
  assert.ok(await evaluate(`area.selectionStart<${lineEnd}`), "visual Home/End");
  await key("PageDown", 34);
  assert.ok(await evaluate("area.scrollTop>0"), "PageDown scrolls the caret into view");
  await key("PageUp", 33);
  await evaluate("area.scrollTop=0");
  const rect = await evaluate("(()=>{const r=area.getBoundingClientRect();return {x:r.x+16,y:r.y+18}})()");
  await call("Input.dispatchMouseEvent", {type:"mousePressed",x:rect.x,y:rect.y,button:"left",buttons:1,clickCount:1});
  await call("Input.dispatchMouseEvent", {type:"mouseMoved",x:rect.x+75,y:rect.y+25,button:"left",buttons:1});
  await call("Input.dispatchMouseEvent", {type:"mouseReleased",x:rect.x+75,y:rect.y+25,button:"left",clickCount:1});
  await evaluate("new Promise(resolve=>requestAnimationFrame(()=>requestAnimationFrame(resolve)))");
  const pointer = await evaluate("({start:area.selectionStart,end:area.selectionEnd,cursor:textTest.state.notesCursor,direction:area.selectionDirection})");
  assert.ok(pointer.end > pointer.start && pointer.cursor === pointer.end, "pointer selection across wrapped lines: " + JSON.stringify(pointer));
  assert.ok(await evaluate("!textTest.rt.input.events.some(event=>event.type==='tap')"), "DOM selection queued a duplicate synthetic tap");
  await key("Tab", 9, 8);
  assert.ok(await evaluate("document.activeElement===document.querySelectorAll('input')[1] && textTest.rt.Focus()===2"), "backward Tab follows browser focus order");
  await evaluate("document.querySelector('#result').textContent+='\\nPASS: real key events, wrapped lines, Home/End, Page Up/Down, pointer selection, backward Tab';");
  await evaluate("window.first=document.querySelector('input');first.focus();first.setSelectionRange(first.value.length,first.value.length);window.beforeIME=textTest.state.first;");
  await call("Input.imeSetComposition", {text:"に",selectionStart:1,selectionEnd:1});
  assert.ok(await evaluate("first.value.endsWith('に') && textTest.state.first===beforeIME"), "native browser preedit presentation");
  await evaluate("textTest.draw()");
  assert.ok(await evaluate("first.value.endsWith('に') && document.activeElement===first"), "native preedit survives redraw");
  const preedit = await call("Page.captureScreenshot", {format:"png"});
  writeFileSync(`${work}/text-input-preedit.png`, Buffer.from(preedit.data,"base64"));
  await call("Input.insertText", {text:"日"});
  assert.ok(await evaluate("textTest.state.first===beforeIME+'日' && first.value===textTest.state.first"), "native browser composition commits once");
  const capture = await call("Page.captureScreenshot", {format:"png"});
  writeFileSync(`${work}/text-input-events.png`, Buffer.from(capture.data,"base64"));
  console.log("Real browser text input events passed");
} finally {
  socket?.close();
  browser.kill();
}
