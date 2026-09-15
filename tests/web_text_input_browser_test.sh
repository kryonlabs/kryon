#!/bin/sh
set -eu
root=$(cd "${1:-.}" && pwd)
work=${2:-$(mktemp -d /tmp/kryon-text-browser.XXXXXX)}
mkdir -p "$work/profile"
cat > "$work/index.html" <<HTML
<!doctype html><meta charset="utf-8"><title>Kryon text input verification</title>
<style>body{font:18px sans-serif;background:#f5f7fa;padding:32px}input,textarea{display:block;margin:16px;padding:12px;width:300px;font:20px monospace}#result{white-space:pre-wrap}</style>
<h1>Kryon text input verification</h1><div id="app"></div><pre id="result">Running</pre>
<script type="module">
import * as k from 'file://$root/web/kryon-runtime.js';
const rt=k.createRuntime();
const state={first:'a界β',cursor:6,second:'second',secondCursor:6,notes:'line one\nline two\nline three\nline four\nline five\nline six',notesCursor:0,readonly:false};
function draw(){
 k.beginFrame(rt);
 k.widget(rt,'Screen',{},state,{path:'root'});
 for(const [name,text,cursor,id] of [['TextField','first','cursor',1],['TextField','second','secondCursor',2],['TextArea','notes','notesCursor',3]])
  k.widget(rt,name,'(TextFieldProps){.text = '+text+', .cursor_position = &'+cursor+', .focus_id = '+id+', .text_size = 64, .read_only = readonly}',state,{path:'root/'+id,parentPath:'root'});
 k.endFrame(rt);k.mount(rt,document.querySelector('#app'));
}
function check(value,message){if(!value)throw Error(message)}
function input(el,text){el.dispatchEvent(new InputEvent('beforeinput',{bubbles:true,cancelable:true,inputType:'insertText',data:text}));}
function key(el,name,options={}){el.dispatchEvent(new KeyboardEvent('keydown',{bubbles:true,cancelable:true,key:name,...options}));}
try{
 draw();
 const first=document.querySelector('input'),second=document.querySelectorAll('input')[1];
 first.focus();first.setSelectionRange(1,2);input(first,'日');
 check(state.first==='a日β' && state.cursor===4,'Unicode replacement/byte cursor');
 const clipboard=new DataTransfer();clipboard.setData('text/plain','界');
 first.dispatchEvent(new ClipboardEvent('paste',{bubbles:true,cancelable:true,clipboardData:clipboard}));
 check(state.first==='a日界β','paste');
 first.dispatchEvent(new CompositionEvent('compositionstart',{bubbles:true}));
 first.value='a日界にβ';
 first.dispatchEvent(new CompositionEvent('compositionupdate',{bubbles:true,data:'に'}));
 check(state.first==='a日界β','preedit mutated committed text');
 draw();
 check(first.value==='a日界にβ','rerender destroyed preedit');
 first.dispatchEvent(new CompositionEvent('compositionend',{bubbles:true,data:'日本'}));
 check(state.first==='a日界日本β','composition commit');
 first.dispatchEvent(new InputEvent('input',{bubbles:true,inputType:'insertCompositionText',data:'日本'}));
 check(state.first==='a日界日本β','composition inserted twice');
 first.dispatchEvent(new CompositionEvent('compositionstart',{bubbles:true}));
 first.value='provisional';
 first.dispatchEvent(new CompositionEvent('compositionupdate',{bubbles:true,data:'wrong'}));
 second.focus();
 first.dispatchEvent(new CompositionEvent('compositionend',{bubbles:true,data:'wrong'}));
 check(state.first==='a日界日本β' && state.second==='second','composition leaked after blur');
 first.focus();first.dispatchEvent(new CompositionEvent('compositionstart',{bubbles:true}));
 first.value='provisional';
 first.dispatchEvent(new CompositionEvent('compositionupdate',{bubbles:true,data:'wrong'}));
 state.readonly=true;draw();
 first.dispatchEvent(new CompositionEvent('compositionend',{bubbles:true,data:'wrong'}));
 check(state.first==='a日界日本β' && !first.__kryTextComposing,'readonly transition did not cancel composition');
 second.focus();input(second,'blocked');key(second,'Backspace');
 check(state.second==='second','readonly mutated text');
 state.readonly=false;draw();
 first.focus();first.setSelectionRange(0,first.value.length);input(first,'verified 日本');
 check(state.first==='verified 日本','editing after readonly');
 key(first,'Escape');check(rt.Focus()===0,'Escape did not blur');
 document.querySelector('#result').textContent='PASS: Unicode selection, paste, preedit, rerender, IME commit, blur cancellation, read-only, Escape';
 document.body.dataset.result='ok';
 window.textTest={rt,state,draw};
}catch(error){document.body.dataset.result='fail';document.querySelector('#result').textContent=error.stack;}
</script>
HTML
chromium --headless=new --disable-gpu --no-sandbox --allow-file-access-from-files \
 --virtual-time-budget=3000 --user-data-dir="$work/profile" --window-size=900,900 \
 --screenshot="$work/text-input.png" --dump-dom "file://$work/index.html" > "$work/dom.html" 2> "$work/browser.log"
if ! rg -q 'data-result="ok"' "$work/dom.html"; then
 cat "$work/dom.html"
 exit 1
fi
printf 'Browser text input passed. Screenshot: %s/text-input.png\n' "$work"
node "$root/tests/web_text_input_events_test.mjs" "$work"
