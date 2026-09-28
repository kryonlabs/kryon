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
  canvas_test_finish__sig: 'i',
  canvas_test_finish: function() {
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
    return 0;
  }
});
