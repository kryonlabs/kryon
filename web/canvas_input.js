// Raw browser effects; state and exported host policy live in canvas_input.zi.
addToLibrary({
  js_input_query__sig: "iii",
  js_input_query: function(which, code) {
    var K = globalThis.__kryonCanvas;
    if (!K) return 0;
    switch (which) {
    case 0: return K.keysDown[code] ? 1 : 0;
    case 1: return K.keysPressed.indexOf(code) >= 0 ? 1 : 0;
    case 2: return K.keysReleased.indexOf(code) >= 0 ? 1 : 0;
    case 3: return K.keysPressed.length > 0 ? K.keysPressed.shift() : 0;
    case 4: return K.chars.length > 0 ? K.chars.shift() : 0;
    case 5: return K.buttonsDown[code] ? 1 : 0;
    case 6: return K.buttonsPressed.indexOf(code) >= 0 ? 1 : 0;
    case 7: return K.buttonsReleased.indexOf(code) >= 0 ? 1 : 0;
    case 8: return K.mouseX;
    case 9: return K.mouseY;
    case 10: return K.mouseDeltaX;
    case 11: return K.mouseDeltaY;
    case 12: {
        var w = Math.abs(K.wheelX) > Math.abs(K.wheelY) ? K.wheelX : K.wheelY;
        return w < 0 ? Math.floor(w) : Math.ceil(w);
    }
    case 13: K.keysPressed = []; return 1;
    case 14: K.keysReleased = []; return 1;
    case 15: K.buttonsPressed = []; return 1;
    case 16: K.buttonsReleased = []; return 1;
    case 17: K.mouseDeltaX = 0; K.mouseDeltaY = 0; return 1;
    case 18: K.wheelX = 0; K.wheelY = 0; return 1;
    case 19: return K.keysRepeated.indexOf(code) >= 0 ? 1 : 0;
    case 20: K.keysRepeated = []; return 1;
    }
    return 0;
  },
  js_input_float__sig: "di",
  js_input_float: function(which) {
    var K = globalThis.__kryonCanvas;
    if (!K) return 0.0;
    switch (which) {
    case 0: return K.mouseDeltaX;
    case 1: return K.mouseDeltaY;
    case 2: return K.wheelX;
    case 3: return K.wheelY;
    case 4: return Math.abs(K.wheelX) > Math.abs(K.wheelY) ? K.wheelX : K.wheelY;
    }
    return 0.0;
  },
  js_input_set_mouse__sig: "vii",
  js_input_set_mouse: function(x, y) {
    var K = globalThis.__kryonCanvas;
    if (!K) return;
    K.mouseDeltaX += x - K.mouseX;
    K.mouseDeltaY += y - K.mouseY;
    K.mouseX = x;
    K.mouseY = y;
  },
  js_input_mouse_config__sig: "vdddd",
  js_input_mouse_config: function(ox, oy, sx, sy) {
    var K = globalThis.__kryonCanvas;
    if (!K) return;
    K.mouseOffsetX = ox;
    K.mouseOffsetY = oy;
    K.mouseScaleX = sx;
    K.mouseScaleY = sy;
  },
  js_touch_query__sig: "iii",
  js_touch_query: function(index, field) {
    var K = globalThis.__kryonCanvas;
    if (!K || index < 0 || index >= K.touches.length) return 0;
    var t = K.touches[index];
    switch (field) {
    case 0: return t.x | 0;
    case 1: return t.y | 0;
    case 2: return t.id | 0;
    case 3: return K.touches.length | 0;
    }
    return 0;
  },
  js_gamepad_query__sig: "iiii",
  js_gamepad_query: function(gamepad, which, index) {
    var K = globalThis.__kryonCanvas;
    if (!K) return 0;
    var pads = (typeof navigator !== 'undefined' && navigator.getGamepads) ?
        navigator.getGamepads() : [];
    var pad = pads && pads[gamepad] ? pads[gamepad] : null;
    if (!pad) return 0;
    var mapButton = function (button) {
        switch (button) {
        case 1: return 12; /* dpad up */
        case 2: return 15; /* dpad right */
        case 3: return 13; /* dpad down */
        case 4: return 14; /* dpad left */
        case 5: return 3;  /* Y/triangle */
        case 6: return 1;  /* B/circle */
        case 7: return 0;  /* A/cross */
        case 8: return 2;  /* X/square */
        case 9: return 4;
        case 10: return 6;
        case 11: return 5;
        case 12: return 7;
        case 13: return 8;
        case 14: return 16;
        case 15: return 9;
        case 16: return 10;
        case 17: return 11;
        default: return -1;
        }
    };
    var buttons = pad.buttons || [];
    var now = [];
    for (var i = 0; i < buttons.length; i++)
        now[i] = !!(buttons[i] && buttons[i].pressed);
    var prev = K.gamepadPrev[gamepad] || [];
    K.gamepadNow[gamepad] = now;
    if (!K.gamepadPressed[gamepad]) K.gamepadPressed[gamepad] = [];
    if (!K.gamepadReleased[gamepad]) K.gamepadReleased[gamepad] = [];
    K.gamepadPressed[gamepad] = [];
    K.gamepadReleased[gamepad] = [];
    for (var b = 0; b < now.length; b++) {
        if (now[b] && !prev[b]) {
            K.gamepadPressed[gamepad].push(b);
            K.lastGamepadButton = b;
        } else if (!now[b] && prev[b]) {
            K.gamepadReleased[gamepad].push(b);
        }
    }
    if (which === 0) return 1;
    var browserButton = mapButton(index);
    if (which === 1) return browserButton >= 0 && K.gamepadPressed[gamepad].indexOf(browserButton) >= 0 ? 1 : 0;
    if (which === 2) return browserButton >= 0 && now[browserButton] ? 1 : 0;
    if (which === 3) return browserButton >= 0 && K.gamepadReleased[gamepad].indexOf(browserButton) >= 0 ? 1 : 0;
    if (which === 4) {
        switch (K.lastGamepadButton) {
        case 12: return 1;
        case 15: return 2;
        case 13: return 3;
        case 14: return 4;
        case 3: return 5;
        case 1: return 6;
        case 0: return 7;
        case 2: return 8;
        case 4: return 9;
        case 6: return 10;
        case 5: return 11;
        case 7: return 12;
        case 8: return 13;
        case 16: return 14;
        case 9: return 15;
        case 10: return 16;
        case 11: return 17;
        default: return 0;
        }
    }
    if (which === 5) return pad.axes ? pad.axes.length : 0;
    return 0;
  },
  js_gamepad_axis__sig: "dii",
  js_gamepad_axis: function(gamepad, axis) {
    var pads = (typeof navigator !== 'undefined' && navigator.getGamepads) ?
        navigator.getGamepads() : [];
    var pad = pads && pads[gamepad] ? pads[gamepad] : null;
    if (!pad) return 0.0;
    if (axis >= 0 && axis < 4 && pad.axes && axis < pad.axes.length)
        return pad.axes[axis] || 0.0;
    if (axis === 4 && pad.buttons && pad.buttons[6])
        return (pad.buttons[6].value || 0.0) * 2.0 - 1.0;
    if (axis === 5 && pad.buttons && pad.buttons[7])
        return (pad.buttons[7].value || 0.0) * 2.0 - 1.0;
    return 0.0;
  },
  js_gamepad_name__deps: ["$stringToUTF8", "$lengthBytesUTF8", "malloc"],
  js_gamepad_name__sig: "ii",
  js_gamepad_name: function(gamepad) {
    var pads = (typeof navigator !== 'undefined' && navigator.getGamepads) ?
        navigator.getGamepads() : [];
    var pad = pads && pads[gamepad] ? pads[gamepad] : null;
    var name = pad && pad.id ? pad.id : "";
    var len = lengthBytesUTF8(name) + 1;
    var ptr = _malloc(len);
    stringToUTF8(name, ptr, len);
    return ptr;
  },
  js_input_end_frame__sig: "v",
  js_input_end_frame: function() {
    var K = globalThis.__kryonCanvas;
    if (!K) return;
    for (var i = 0; i < K.gamepadNow.length; i++)
        if (K.gamepadNow[i]) K.gamepadPrev[i] = K.gamepadNow[i].slice();
  },
  js_input_offset__sig: 'vii',
  js_input_offset: function(x, y) { var K=globalThis.__kryonCanvas; if(K) { K.mouseOffsetX=x; K.mouseOffsetY=y; } },
  js_input_scale__sig: 'vdd',
  js_input_scale: function(x, y) { var K=globalThis.__kryonCanvas; if(K) { K.mouseScaleX=x; K.mouseScaleY=y; } },
  js_input_cursor__sig: 'vi',
  js_input_cursor: function(mode) {
    var K=globalThis.__kryonCanvas, c=K && K.canvas; if(!c) return;
    if(mode === 0 || mode === 2) { K.cursorHidden=false; if(c.style) c.style.cursor=K.cursor || 'default'; }
    if(mode === 1 || mode === 3) { K.cursorHidden=true; if(c.style) c.style.cursor='none'; }
    if(mode === 2 && typeof document !== 'undefined' && document.pointerLockElement === c && document.exitPointerLock) document.exitPointerLock();
    if(mode === 3 && c.requestPointerLock) { var result=c.requestPointerLock(); if(result && result.catch) result.catch(function(){}); }
  },
  js_input_cursor_state__sig: 'ii',
  js_input_cursor_state: function(mode) { var K=globalThis.__kryonCanvas; if(!K) return 0; return mode === 0 ? +!!K.cursorHidden : +(K.mouseX>=0 && K.mouseY>=0 && K.mouseX<K.w && K.mouseY<K.h); },
  js_gamepad_vibration__sig: 'viddd',
  js_gamepad_vibration: function(index, left, right, seconds) {
    var pads=typeof navigator !== 'undefined' && navigator.getGamepads ? navigator.getGamepads() : [];
    var pad=pads[index], effect=pad && pad.vibrationActuator;
    if(effect && effect.playEffect) effect.playEffect('dual-rumble', {startDelay:0,duration:Math.max(0,seconds*1000),weakMagnitude:Math.max(0,Math.min(1,right)),strongMagnitude:Math.max(0,Math.min(1,left))}).catch(function(){});
  },
});
