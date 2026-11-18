// Test tooling: own a headless browser, forward bounded records, then exit.
import {spawn} from 'node:child_process';
import {createServer} from 'node:http';
import {createReadStream} from 'node:fs';
import {mkdtemp, readFile, rm} from 'node:fs/promises';
import {dirname, basename, resolve} from 'node:path';
import {setTimeout as delay} from 'node:timers/promises';
const [file, ...args] = process.argv.slice(2);
if (!file || args.length !== 7) throw Error('Expected program and seven benchmark arguments');
const program = resolve(file), directory = dirname(program);
const profile = await mkdtemp(directory + '/chromium-');
const env = {...process.env, HARMONY_DESKTOP_RECOVERY: '0', YUE_DESKTOP_RECOVERY: '0'};
for (const key of ['DISPLAY','WAYLAND_DISPLAY','XAUTHORITY','DBUS_SESSION_BUS_ADDRESS','SESSION_MANAGER']) delete env[key];
const script = basename(program);
const server = createServer((request, response) => {
  if (request.url === '/') {
    response.setHeader('Content-Type', 'text/html');
    response.end('<!doctype html><meta charset=utf-8><script>var benchmarkDone=false, benchmarkError=null;var Module={print:s=>console.log(s),printErr:s=>console.error(s),onAbort:s=>{benchmarkError=String(s)},onRuntimeInitialized:()=>setTimeout(()=>{try{let code=Module.callMain(' + JSON.stringify(args) + ');if(code)benchmarkError="Exit "+code;benchmarkDone=true}catch(e){benchmarkError=String(e)}},0)};</script><script src="/' + script + '"></script>');
    return;
  }
  const name = request.url?.slice(1);
  if (![script, basename(program, '.js') + '.wasm'].includes(name)) { response.writeHead(404); response.end(); return; }
  response.setHeader('Content-Type', name.endsWith('.wasm') ? 'application/wasm' : 'text/javascript');
  const stream = createReadStream(resolve(directory, name));
  stream.on('error', () => { response.destroy(); });
  stream.pipe(response);
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
let browser, socket, errors = '', records = 0;
try {
  browser = spawn(process.env.CHROMIUM || 'chromium', ['--headless', '--no-sandbox', '--disable-dev-shm-usage', '--no-first-run', '--remote-debugging-port=0', '--user-data-dir=' + profile, 'about:blank'], {env, detached:true, stdio:['ignore','ignore','pipe']});
  browser.on('error', e => {errors = String(e);});
  browser.stderr.on('data', d => {errors = (errors + d).slice(-3000);});
  let port;
  const startup = Date.now() + 30000;
  while (Date.now() < startup) {
    try { [port] = (await readFile(profile + '/DevToolsActivePort', 'utf8')).split('\n'); break; } catch {}
    if (browser.exitCode !== null) throw Error('Browser exited: ' + errors);
    await delay(50);
  }
  if (!port) throw Error('Browser unavailable: ' + errors);
  const pages = await (await fetch('http://127.0.0.1:' + port + '/json/list')).json();
  socket = new WebSocket(pages.find(p => p.type === 'page').webSocketDebuggerUrl);
  await new Promise((resolve,reject) => {socket.addEventListener('open', resolve, {once:true});socket.addEventListener('error', reject, {once:true});});
  let id=0, diagnostics='', recordError=''; const pending=new Map();
  socket.addEventListener('message', event => {
    const message=JSON.parse(event.data);
    if (message.method === 'Runtime.consoleAPICalled') {
      const line=message.params.args.map(a => a.value ?? a.description ?? '').join(' ');
      if (line.startsWith('sample ') || line.startsWith('workload ')) {
        if (++records > 9000 || line.length > 8192) recordError='Unbounded benchmark output';
        else process.stdout.write(line + '\n');
      } else diagnostics=(diagnostics + line).slice(-3000);
    }
    const item=pending.get(message.id);
    if (item) {clearTimeout(item.timer);pending.delete(message.id);message.error ? item.reject(Error(JSON.stringify(message.error))) : item.resolve(message.result);}
  });
  const command=(method,params={}) => new Promise((resolve,reject) => {
    const number=++id; const timer=setTimeout(()=>{pending.delete(number);reject(Error(method+' timed out'));},1900000);
    pending.set(number,{resolve,reject,timer});socket.send(JSON.stringify({id:number,method,params}));
  });
  await command('Runtime.enable'); await command('Page.enable');
  await command('Page.navigate',{url:'http://127.0.0.1:'+server.address().port+'/'});
  const deadline=Date.now()+1800000; let complete=false;
  while (Date.now()<deadline) {
    const result=await command('Runtime.evaluate',{expression:'JSON.stringify({done:globalThis.benchmarkDone,error:globalThis.benchmarkError})',returnByValue:true});
    if (result.exceptionDetails) throw Error(JSON.stringify(result.exceptionDetails));
    const state=JSON.parse(result.result.value || '{}');
    if (state.error || recordError) throw Error(state.error || recordError);
    if (state.done) {complete=true;break;}
    await delay(100);
  }
  if (!complete) throw Error('Wasm benchmark incomplete: '+diagnostics+' '+errors);
} finally {
  socket?.close();
  if (browser?.pid) {
    try {process.kill(-browser.pid,'SIGTERM');} catch {}
    await Promise.race([new Promise(r => browser.exitCode!==null || browser.signalCode!==null ? r() : browser.once('exit',r)),delay(2000)]);
    if (browser.exitCode===null && browser.signalCode===null) {try {process.kill(-browser.pid,'SIGKILL');} catch {}}
  }
  await new Promise(resolve => server.close(resolve));
  await rm(profile,{recursive:true,force:true});
}
