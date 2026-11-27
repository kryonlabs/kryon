// Test-only browser effect bridge. Pixels come from the WebGL2 presenter's
// drawing buffer: premultiplied RGBA with GL's bottom-up rows.
globalThis.__canvasGLRegion = function(x, y, width, height) {
  var canvas=document.getElementById('canvas');
  var gl=canvas.getContext('webgl2');
  var base=gl.drawingBufferHeight-y-height;
  var data=new Uint8ClampedArray(width*height*4);
  gl.readPixels(x, base, width, height, gl.RGBA, gl.UNSIGNED_BYTE, data);
  var row=width*4, top=0, bottom=height-1;
  while(top<bottom) {
    var start=top*row, end=bottom*row;
    for(var at=0;at<row;at++) {var swap=data[start+at]; data[start+at]=data[end+at]; data[end+at]=swap;}
    top++; bottom--;
  }
  return data;
};
globalThis.__canvasGLPixel = function(x, y) {
  var data=globalThis.__canvasGLRegion(x, y, 1, 1);
  return [data[0], data[1], data[2], data[3]];
};
// 2D canvases report straight alpha; the presenter's buffer is premultiplied.
globalThis.__canvasPremul = function(straight) {
  var out=new Uint8ClampedArray(straight.length);
  for(var at=0;at<straight.length;at+=4) {
    var alpha=straight[at+3]/255;
    out[at]=straight[at]*alpha; out[at+1]=straight[at+1]*alpha;
    out[at+2]=straight[at+2]*alpha; out[at+3]=straight[at+3];
  }
  return out;
};
globalThis.__canvasCompare = function(actual, expected, tolerance, label) {
  for(var index=0;index<actual.length;index++) {
    if(Math.abs(actual[index]-expected[index])>tolerance) {
      globalThis.canvasTestError=label+' differs at byte '+index+': '+actual[index]+' expected '+expected[index];
      return false;
    }
  }
  return true;
};
addToLibrary({
  canvas_test_density__sig: 'vd',
  canvas_test_density: function(density) {
    Object.defineProperty(globalThis, 'devicePixelRatio', {value:density, configurable:true});
  },
  canvas_test_retained__sig: 'ii',
  canvas_test_retained: function(phase) {
    var canvas=document.getElementById('canvas'), dpi=Math.max(1,globalThis.devicePixelRatio||1);
    var reference=new OffscreenCanvas(canvas.width, canvas.height), ctx=reference.getContext('2d');
    ctx.fillStyle=phase>=12?'#e8f0f8':'white'; ctx.fillRect(0,0,canvas.width,canvas.height); ctx.scale(dpi,dpi);
    function box(x,y,w,h,color) {
      ctx.fillStyle=color;
      ctx.beginPath(); ctx.roundRect(x,y,w,h,0); ctx.fill();
    }
    if(phase!==7 && phase!==8) box(10,10,40,36,phase===4?'rgba(0,160,0,0.501961)':'rgba(240,0,0,0.501961)');
    if(phase<=6 && phase!==2) box(24+(phase>=5?3:phase)*6,12,7,9,'rgba(0,0,240,0.501961)');
    if(phase===9||phase===10||phase===15) {
      ctx.fillStyle='#0000f0';
      ctx.fillRect(Math.round(80*dpi)/dpi,Math.round(10*dpi)/dpi,
        (Math.round(100*dpi)-Math.round(80*dpi))/dpi,
        (Math.round(30*dpi)-Math.round(10*dpi))/dpi);
    }
    var expected=globalThis.__canvasPremul(ctx.getImageData(0,0,canvas.width,canvas.height).data);
    var actual=globalThis.__canvasGLRegion(0,0,canvas.width,canvas.height);
    return globalThis.__canvasCompare(actual,expected,12,'retained frame '+phase)?1:0;
  },
  canvas_test_sprite_state__sig: 'iiii',
  canvas_test_sprite_state: function(context, source, phase) {
    var W=globalThis.__ziranWeb, actual=W.value(context), image=W.value(source);
    if(phase%2===0) return 1;
    var reference=new OffscreenCanvas(32, 32), rctx=reference.getContext('2d');
    // BeginScissorMode(0,0,26,20) applies in target device pixels.
    if(phase===3) { rctx.beginPath(); rctx.rect(0,0,26,20); rctx.clip(); }
    rctx.translate(5,3); rctx.rotate(9.7403*Math.PI/180); rctx.scale(1.1,1.1);
    for(var index=0;index<4;index++) {
      rctx.save();
      if(phase===3) {
        // CanvasDrawImageClipped clips rect(0,0,18,16) through the camera.
        var matrix=rctx.getTransform();
        var corners=[[0,0],[18,0],[0,16],[18,16]].map(([x,y]) => matrix.transformPoint({x,y}));
        var left=Math.floor(Math.min(...corners.map(p => p.x)));
        var top=Math.floor(Math.min(...corners.map(p => p.y)));
        var right=Math.ceil(Math.max(...corners.map(p => p.x)));
        var bottom=Math.ceil(Math.max(...corners.map(p => p.y)));
        rctx.setTransform(1,0,0,1,0,0);
        rctx.beginPath(); rctx.rect(left,top,right-left,bottom-top); rctx.clip();
        rctx.setTransform(matrix);
      }
      rctx.imageSmoothingEnabled=!!(index%2);
      if(index%2) rctx.globalAlpha=128/255;
      rctx.translate(index*5+7,4);
      rctx.drawImage(image,0,0,2,2,-7,-3,5,12);
      rctx.restore();
    }
    var expected=rctx.getImageData(0,0,32,32).data;
    var got=actual.getContext('2d').getImageData(0,0,32,32).data;
    return globalThis.__canvasCompare(got,expected,6,'sprite state pixels')?1:0;
  },
  canvas_test_text__sig: 'iiifii',
  canvas_test_text: function(face, size, zoom, width, dark) {
    var canvas=document.getElementById('canvas');
    var dpi=Math.max(1,globalThis.devicePixelRatio||1);
    if(canvas.width!==Math.round(160*dpi)||canvas.height!==Math.round(120*dpi)) {
      globalThis.canvasTestError='backing store did not follow display density '+dpi;
      return 0;
    }
    var reference=typeof OffscreenCanvas==='function'
      ? new OffscreenCanvas(canvas.width, canvas.height)
      : document.createElement('canvas');
    reference.width=canvas.width;
    reference.height=canvas.height;
    var context=reference.getContext('2d');
    var family=face?'kryon-face-'+(face-1):'monospace';
    context.font=size+'px '+family;
    var metrics=context.measureText('H');
    var line=metrics.fontBoundingBoxAscent+metrics.fontBoundingBoxDescent;
    var em=Math.max(100,Math.round(size*size*100/line))/100;
    context.font=em.toFixed(2)+'px '+family;
    var expectedWidth=Math.round(context.measureText('AV caf\u00e9 \u203a\u2026').width);
    if(width!==expectedWidth) {
      globalThis.canvasTestError='text width '+width+' expected '+expectedWidth;
      return 0;
    }
    var ascent=context.measureText('H').fontBoundingBoxAscent;
    context.fillStyle=dark?'#182238':'#ffffff';
    context.fillRect(0,0,reference.width,reference.height);
    context.setTransform(dpi,0,0,dpi,0,0);
    context.translate(3,4);
    context.scale(zoom,zoom);
    context.beginPath();
    context.rect(4,5,52,size);
    context.clip();
    context.fillStyle=dark?'rgba(247,249,255,'+(190/255)+')':'#141e28';
    context.fillText('AV caf\u00e9 \u203a\u2026',4,5+ascent);
    var expected=globalThis.__canvasPremul(context.getImageData(0,0,canvas.width,canvas.height).data);
    var actual=globalThis.__canvasGLRegion(0,0,canvas.width,canvas.height);
    return globalThis.__canvasCompare(actual,expected,3,'text pixels face='+face+', size='+size+', zoom='+zoom+', dpi='+dpi+', dark='+dark)?1:0;
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
      event('keypress',{key:'\ud83d\ude00'});
      event('mousedown',{clientX:17,clientY:19,button:0});
    }
    if(phase===1) event('keyup',{code:'KeyA',key:'a'});
    if(phase===2) {
      event('keydown',{code:'KeyB',key:'b'});
      event('keypress',{key:'B'});
      var dpi=Math.max(1,globalThis.devicePixelRatio||1);
      var pixel=globalThis.__canvasGLPixel(Math.round(50*dpi),Math.round(50*dpi));
      if(pixel[0]!==0||pixel[1]!==0||pixel[2]!==0||pixel[3]!==255) return 1;
    }
    if(phase===3) {
      var density=globalThis.devicePixelRatio===2?1.25:2;
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
    var got=globalThis.__canvasGLPixel(Math.round(dpi),Math.round(dpi));
    return got[0]===r&&got[1]===g&&got[2]===b?1:0;
  },
  canvas_test_finish__sig: 'ii',
  canvas_test_finish: function(meshDraws) {
    // main's exit code is lost once Asyncify has suspended it (font loading
    // awaits), so a failure is also recorded for the page's onExit.
    var code=(function() {
    var dpi=Math.max(1,globalThis.devicePixelRatio||1);
    var surface=globalThis.__canvasGLRegion(0,0,document.getElementById('canvas').width,document.getElementById('canvas').height);
    function pixelAt(x,y) {
      var row=Math.round(160*dpi)*4;
      var at=(Math.round(y*dpi)*Math.round(160*dpi)+Math.round(x*dpi))*4;
      return [surface[at],surface[at+1],surface[at+2],surface[at+3]];
    }
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
      [150,78,[255,0,255,255]],
      [129,33,[127,255,127,255]], [138,33,[191,127,127,255]],
      [152,94,[0,128,0,255]]
    ];
    for(var item of cases) {
      var got=pixelAt(item[0],item[1]);
      if(got.some(function(value,index){return value!==item[2][index];})) {
        globalThis.canvasTestError='pixel '+item[0]+','+item[1]+' '+got+' expected '+item[2];
        return 20;
      }
    }
    var source=new OffscreenCanvas(2, 2), sourceContext=source.getContext('2d');
    sourceContext.putImageData(new ImageData(new Uint8ClampedArray([
      192,0,0,255, 0,192,0,128,
      0,0,192,64, 192,192,192,0
    ]), 2, 2), 0, 0);
    var reference=new OffscreenCanvas(document.getElementById('canvas').width, document.getElementById('canvas').height);
    var referenceContext=reference.getContext('2d');
    referenceContext.scale(dpi, dpi);
    referenceContext.fillStyle='white';
    referenceContext.fillRect(0, 0, 160, 120);
    referenceContext.translate(96, 52);
    referenceContext.scale(-1, 1);
    referenceContext.drawImage(source, 0, 0, 16, 16);
    var expected=globalThis.__canvasPremul(referenceContext.getImageData(0,0,reference.width,reference.height).data);
    for(var y=53;y<67;y++) for(var x=81;x<95;x++) {
      var at=(Math.round(y*dpi)*Math.round(160*dpi)+Math.round(x*dpi))*4;
      var same=true;
      for(var channel=0;channel<4;channel++) {
        if(Math.abs(surface[at+channel]-expected[at+channel])>10) {same=false;break;}
      }
      if(!same) {
        globalThis.canvasTestError='smooth warped image differs at '+x+','+y+' '+
          [surface[at],surface[at+1],surface[at+2],surface[at+3]]+' expected '+
          [expected[at],expected[at+1],expected[at+2],expected[at+3]];
        return 20;
      }
    }
    // The warped image's two triangles share the cell's diagonal; no seam
    // lets the background through along it.
    for(var k=1;k<15;k++) {
      if(k===7||k===8) continue;
      var expectedColor=k<7?[0,255,0,255]:[0,0,255,255];
      var seam=pixelAt(96+k,30+k);
      if(seam.some(function(value,index){return value!==expectedColor[index];})) {
        globalThis.canvasTestError='warped image seam at '+(96+k)+','+(30+k)+' '+seam+' expected '+expectedColor;
        return 37;
      }
    }
    // A shape's four triangles meet along both diagonals of their square
    // without a seam.
    for(var j=1;j<11;j++) {
      var points=[[64+j,52+j],[75-j,52+j]];
      for(var point of points) {
        var fill=pixelAt(point[0],point[1]);
        if(fill[0]!==0||fill[1]!==160||fill[2]!==0||fill[3]!==255) {
          globalThis.canvasTestError='triangle seam at '+point+' '+fill;
          return 38;
        }
      }
    }
    function inkIn(x,y,width,height) {
      var data=globalThis.__canvasGLRegion(Math.round(x*dpi),Math.round(y*dpi),Math.round(width*dpi),Math.round(height*dpi));
      var count=0;
      for(var at=0;at<data.length;at+=4) if(data[at]<100) count++;
      return count;
    }
    var strip=globalThis.__canvasGLRegion(0,Math.round(70*dpi),Math.round(160*dpi),Math.round(35*dpi));
    var ink=0; for(var pixel=0;pixel<strip.length;pixel+=4) if(strip[pixel]<100) ink++;
    if(ink<40) {globalThis.canvasTestError='font atlas produced no text';return 22;}
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
