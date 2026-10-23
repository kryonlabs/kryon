// Test-only browser effect bridge. Actual pixels come from the browser Canvas2D.
addToLibrary({
  canvas_test_density__sig: 'vd',
  canvas_test_density: function(density) {
    Object.defineProperty(globalThis, 'devicePixelRatio', {value:density, configurable:true});
  },
  canvas_test_retained__sig: 'ii',
  canvas_test_retained: function(phase) {
    var canvas = document.getElementById('canvas'), dpi = Math.max(1, globalThis.devicePixelRatio || 1);
    var reference = new OffscreenCanvas(canvas.width, canvas.height), ctx = reference.getContext('2d');
    ctx.fillStyle = phase >= 12 ? '#e8f0f8' : 'white'; ctx.fillRect(0,0,canvas.width,canvas.height); ctx.scale(dpi,dpi);
    function box(x,y,w,h,color) {
      ctx.fillStyle = color;
      ctx.beginPath(); ctx.roundRect(x,y,w,h,0); ctx.fill();
    }
    if (phase !== 7 && phase !== 8) box(10,10,40,36,phase === 4 ? 'rgba(0,160,0,0.501961)' : 'rgba(240,0,0,0.501961)');
    if (phase >= 17) {
      ctx.globalAlpha = 0.5;
      for (var round=0;round<phase-16;round++) {
        ctx.fillStyle = '#e8f0f8'; ctx.fillRect(0,0,160,120);
        box(10,10,40,36,'rgba(240,0,0,0.501961)');
      }
    }
    if (phase <= 6 && phase !== 2) box(24 + (phase >= 5 ? 3 : phase)*6,12,7,9,'rgba(0,0,240,0.501961)');
    if (phase === 9 || phase === 10 || phase === 15) {
      ctx.fillStyle = '#0000f0';
      ctx.fillRect(Math.round(80*dpi)/dpi,Math.round(10*dpi)/dpi,
        (Math.round(100*dpi)-Math.round(80*dpi))/dpi,
        (Math.round(30*dpi)-Math.round(10*dpi))/dpi);
    }
    var expected = ctx.getImageData(0,0,canvas.width,canvas.height).data;
    var actual = canvas.getContext('2d').getImageData(0,0,canvas.width,canvas.height).data;
    for (var index=0;index<actual.length;index++) {
      if (Math.abs(actual[index]-expected[index]) > 1) {
        globalThis.canvasTestError = 'retained frame '+phase+' differs at byte '+index+': '+actual[index]+' expected '+expected[index];
        return 0;
      }
    }
    return 1;
  },
  canvas_test_sprite_state__sig: 'iiii',
  canvas_test_sprite_state: function(context, source, phase) {
    var W = globalThis.__ziranWeb, ctx = W.value(context);
    function setup(c) {
      c.fillStyle = '#123456'; c.fillRect(0, 0, 32, 32);
      c.translate(5, 3); c.rotate(0.17); c.scale(1.1, 0.9);
      c.beginPath(); c.rect(0, 0, 26, 20); c.clip();
      c.globalAlpha = 0.25; c.imageSmoothingEnabled = true;
    }
    if (phase % 2 === 0) { setup(ctx); return 1; }
    var expected = new OffscreenCanvas(32, 32), reference = expected.getContext('2d');
    setup(reference);
    for (var index = 0; index < 4; index++) {
      reference.save();
      if (phase === 3) {
        var matrix = reference.getTransform();
        var corners = [[0,0],[18,0],[0,16],[18,16]].map(([x,y]) => matrix.transformPoint({x,y}));
        var left = Math.floor(Math.min(...corners.map(p => p.x)));
        var top = Math.floor(Math.min(...corners.map(p => p.y)));
        var right = Math.ceil(Math.max(...corners.map(p => p.x)));
        var bottom = Math.ceil(Math.max(...corners.map(p => p.y)));
        reference.setTransform(1,0,0,1,0,0);
        reference.beginPath(); reference.rect(left,top,right-left,bottom-top); reference.clip();
        reference.setTransform(matrix);
      }
      reference.imageSmoothingEnabled = !!(index % 2);
      if (index % 2) reference.globalAlpha = 128 / 255;
      reference.translate(index * 5 + 7, 4);
      reference.drawImage(W.value(source), 0, 0, 2, 2, -7, -3, 5, 12);
      reference.restore();
    }
    if (ctx.globalAlpha !== 0.25 || !ctx.imageSmoothingEnabled) return 0;
    var actual = ctx.getImageData(0, 0, 32, 32).data;
    var pixels = reference.getImageData(0, 0, 32, 32).data;
    for (var byte = 0; byte < pixels.length; byte++) {
      if (actual[byte] !== pixels[byte]) {
        globalThis.canvasTestError = 'sprite state pixels differ at byte ' + byte;
        return 0;
      }
    }
    return 1;
  },
  canvas_test_text__sig: 'iiifii',
  canvas_test_text: function(face, size, zoom, width, dark) {
    var canvas = document.getElementById('canvas');
    var dpi = Math.max(1, globalThis.devicePixelRatio || 1);
    if (canvas.width !== Math.round(160 * dpi) || canvas.height !== Math.round(120 * dpi)) {
      globalThis.canvasTestError = 'backing store did not follow display density ' + dpi;
      return 0;
    }
    var reference = typeof OffscreenCanvas === 'function'
      ? new OffscreenCanvas(canvas.width, canvas.height)
      : document.createElement('canvas');
    reference.width = canvas.width;
    reference.height = canvas.height;
    var context = reference.getContext('2d');
    var family = face ? 'kryon-face-' + (face - 1) : 'monospace';
    context.font = size + 'px ' + family;
    var metrics = context.measureText('H');
    var line = metrics.fontBoundingBoxAscent + metrics.fontBoundingBoxDescent;
    var em = Math.max(100, Math.round(size * size * 100 / line)) / 100;
    context.font = em.toFixed(2) + 'px ' + family;
    var expectedWidth = Math.round(context.measureText('AV café ›…').width);
    if (width !== expectedWidth) {
      globalThis.canvasTestError = 'text width ' + width + ' expected ' + expectedWidth;
      return 0;
    }
    var ascent = context.measureText('H').fontBoundingBoxAscent;
    context.fillStyle = dark ? '#182238' : '#ffffff';
    context.fillRect(0, 0, reference.width, reference.height);
    context.setTransform(dpi, 0, 0, dpi, 0, 0);
    context.translate(3, 4);
    context.scale(zoom, zoom);
    context.beginPath();
    context.rect(4, 5, 52, size);
    context.clip();
    context.fillStyle = dark ? 'rgba(247,249,255,' + (190 / 255) + ')' : '#141e28';
    context.fillText('AV café ›…', 4, 5 + ascent);
    var actual = canvas.getContext('2d').getImageData(0, 0, canvas.width, canvas.height).data;
    var expected = context.getImageData(0, 0, canvas.width, canvas.height).data;
    for (var index = 0; index < actual.length; index++) {
      if (actual[index] !== expected[index]) {
        globalThis.canvasTestError = 'text pixels differ at byte ' + index +
          ', face=' + face + ', size=' + size + ', zoom=' + zoom + ', dpi=' + dpi +
          ', dark=' + dark + ', actual=' + actual[index] + ', expected=' + expected[index];
        return 0;
      }
    }
    return 1;
  },
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
    if(phase===3) {
      var density = globalThis.devicePixelRatio === 2 ? 1.25 : 2;
      Object.defineProperty(globalThis, 'devicePixelRatio', {value:density, configurable:true});
    }
    if(phase===4) {
      event('mousedown',{clientX:17,clientY:19,button:0});
      event('mouseup',{clientX:17,clientY:19,button:0});
      event('keydown',{code:'KeyC',key:'c'});
      event('keyup',{code:'KeyC',key:'c'});
      event('wheel',{deltaX:0,deltaY:-120,deltaMode:0});
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
  canvas_test_finish__sig: 'ii',
  canvas_test_finish: function(meshDraws) {
    globalThis.canvasTestMeshDraws = meshDraws;
    // main's exit code is lost once Asyncify has suspended it (font loading
    // awaits), so a failure is also recorded for the page's onExit.
    var code=(function() {
    var dpi=Math.max(1,globalThis.devicePixelRatio||1),ctx=document.getElementById('canvas').getContext('2d');
    var cases=[
      [1,1,[255,255,255,255]], [21,11,[0,255,0,255]],
      [25,15,[0,0,255,255]], [35,11,[255,255,0,255]],
      [45,11,[255,0,0,255]], [81,11,[255,0,255,255]],
      [91,11,[128,0,255,255]], [101,11,[0,255,255,255]],
      [81,31,[255,128,0,255]], [81,38,[0,0,0,255]],
      [99,33,[0,255,0,255]], [108,33,[255,0,0,255]],
      [99,42,[255,255,255,255]], [108,42,[0,0,255,255]],
      [110,55,[127,255,255,255]], [130,80,[0,255,255,255]],
      [15,55,[0,255,255,255]], [23,55,[255,0,255,255]],
      [31,55,[127,128,255,255]], [40,53,[127,255,255,255]],
      [150,78,[255,0,255,255]]
    ];
    if (meshDraws) {
      cases.push([129,33,[127,255,127,255]], [138,33,[191,127,127,255]],
        [152,94,[0,128,0,255]]);
    }
    for(var item of cases) {
      var got=Array.from(ctx.getImageData(Math.round(item[0]*dpi),Math.round(item[1]*dpi),1,1).data);
      if(got.some(function(value,index){return value!==item[2][index];})) {
        globalThis.canvasTestError='pixel '+item[0]+','+item[1]+' '+got+' expected '+item[2];
        return 20;
      }
    }
    var source = new OffscreenCanvas(2, 2), sourceContext = source.getContext('2d');
    sourceContext.putImageData(new ImageData(new Uint8ClampedArray([
      192,0,0,255, 0,192,0,meshDraws ? 128 : 255,
      0,0,192,meshDraws ? 64 : 255, 192,192,192,meshDraws ? 0 : 255
    ]), 2, 2), 0, 0);
    var reference = new OffscreenCanvas(ctx.canvas.width, ctx.canvas.height);
    var referenceContext = reference.getContext('2d');
    referenceContext.scale(dpi, dpi);
    referenceContext.fillStyle = 'white';
    referenceContext.fillRect(0, 0, 160, 120);
    referenceContext.translate(96, 52);
    referenceContext.scale(-1, 1);
    referenceContext.drawImage(source, 0, 0, 16, 16);
    for (var y = 53; y < 67; y++) for (var x = 81; x < 95; x++) {
      var actual = ctx.getImageData(Math.round(x*dpi), Math.round(y*dpi), 1, 1).data;
      var expected = referenceContext.getImageData(Math.round(x*dpi), Math.round(y*dpi), 1, 1).data;
      if (actual.some((value, channel) => Math.abs(value-expected[channel]) > 2)) {
        globalThis.canvasTestError = 'smooth warped image differs at '+x+','+y+' '+actual+' expected '+expected;
        return 20;
      }
    }
    // The warped image's two triangles share the cell's diagonal; no seam
    // lets the background through along it.
    for(var k=1;k<15;k++) {
      if(k===7||k===8) continue;
      var expected=k<7?[0,255,0,255]:[0,0,255,255];
      var seam=Array.from(ctx.getImageData(Math.round((96+k)*dpi),Math.round((30+k)*dpi),1,1).data);
      if(seam.some(function(value,index){return value!==expected[index];})) {
        globalThis.canvasTestError='warped image seam at '+(96+k)+','+(30+k)+' '+seam+' expected '+expected;
        return 37;
      }
    }
    // A shape's four triangles meet along both diagonals of their square
    // without a seam.
    for(var j=1;j<11;j++) {
      var points=[[64+j,52+j],[75-j,52+j]];
      for(var point of points) {
        var fill=Array.from(ctx.getImageData(Math.round(point[0]*dpi),Math.round(point[1]*dpi),1,1).data);
        if(fill[0]!==0 || fill[1]!==160 || fill[2]!==0 || fill[3]!==255) {
          globalThis.canvasTestError='triangle seam at '+point+' '+fill;
          return 38;
        }
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
