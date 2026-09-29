// Test-only browser effect bridge. Actual pixels come from the browser Canvas2D.
addToLibrary({
  canvas_test_input__sig: 'ii',
  canvas_test_input: function(phase) {
    function event(name, values) {
      var e=new Event(name, {bubbles:true,cancelable:true});
      Object.keys(values).forEach(function(key) { Object.defineProperty(e,key,{value:values[key]}); });
      document.dispatchEvent(e);
    }
    if(phase===0) {
      event('keydown',{code:'KeyA',key:'a'});
      event('keypress',{key:'😀'});
      event('mousedown',{clientX:17,clientY:19,button:0});
    }
    if(phase===1) event('keyup',{code:'KeyA',key:'a'});
    if(phase===2) {
      event('keydown',{code:'KeyB',key:'b'});
      event('keypress',{key:'B'});
      var dpi=Math.max(1,globalThis.devicePixelRatio||1), pixel=document.getElementById('canvas').getContext('2d').getImageData(50*dpi,50*dpi,1,1).data;
      if(pixel[0]!==0 || pixel[1]!==0 || pixel[2]!==0 || pixel[3]!==255) return 1;
    }
    return 0;
  },
  // Records a failure found after main was suspended; see canvas_test_finish.
  canvas_test_fail__sig: 'vi',
  canvas_test_fail: function(code) {
    if(!globalThis.canvasTestFailure) globalThis.canvasTestFailure=code;
    globalThis.canvasTestError=globalThis.canvasTestError||('check '+code);
  },
  // Whether the page's canvas shows this colour at (1, 1) right now.
  canvas_test_front__sig: 'iiii',
  canvas_test_front: function(r, g, b) {
    var dpi=Math.max(1,globalThis.devicePixelRatio||1);
    var got=document.getElementById('canvas').getContext('2d').getImageData(Math.round(dpi),Math.round(dpi),1,1).data;
    return got[0]===r && got[1]===g && got[2]===b ? 1 : 0;
  },
  canvas_test_finish__sig: 'i',
  canvas_test_finish: function() {
    // main's exit code is lost once Asyncify has suspended it (font loading
    // awaits), so a failure is also recorded for the page's onExit.
    var code=(function() {
    var dpi=Math.max(1,globalThis.devicePixelRatio||1),ctx=document.getElementById('canvas').getContext('2d');
    var cases=[
      [1,1,[255,255,255,255]], [21,11,[0,255,0,255]],
      [25,15,[0,0,255,255]], [35,11,[255,255,0,255]],
      [45,11,[255,0,0,255]], [81,11,[255,0,255,255]],
      [91,11,[128,0,255,255]], [101,11,[0,255,255,255]],
      [81,31,[255,128,0,255]], [81,38,[0,0,0,255]]
    ];
    for(var item of cases) {
      var got=Array.from(ctx.getImageData(Math.round(item[0]*dpi),Math.round(item[1]*dpi),1,1).data);
      if(got.some(function(value,index){return value!==item[2][index];})) {
        globalThis.canvasTestError='pixel '+item[0]+','+item[1]+' '+got+' expected '+item[2];
        return 20;
      }
    }
    var textPixels=ctx.getImageData(0,70*dpi,160*dpi,35*dpi).data;
    var ink=0; for(var pixel=0;pixel<textPixels.length;pixel+=4) if(textPixels[pixel]<100) ink++;
    if(ink<40) {globalThis.canvasTestError='font atlas produced no text';return 22;}
    function inkIn(x,y,width,height) {
      var data=ctx.getImageData(x*dpi,y*dpi,width*dpi,height*dpi).data, count=0;
      for(var at=0;at<data.length;at+=4) if(data[at]<100) count++;
      return count;
    }
    if(inkIn(128,0,32,22)<6) {globalThis.canvasTestError='glyphs added on first use are not drawn';return 27;}
    if(inkIn(115,30,30,20)<10) {globalThis.canvasTestError='glyph missing from its line box';return 23;}
    if(inkIn(115,51,30,9)!==0) {globalThis.canvasTestError='glyph drawn below its line box';return 24;}
    if(inkIn(44,96,34,20)<10) {globalThis.canvasTestError='zoomed text clip is not scaled';return 25;}
    if(inkIn(82,94,40,26)!==0) {globalThis.canvasTestError='zoomed text escaped its clip';return 26;}
    return 0;
    })();
    if(code) globalThis.canvasTestFailure=code;
    return code;
  }
});
