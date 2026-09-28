// Raw browser effects; state and exported host policy live in canvas_input.zi.
addToLibrary({
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
  js_input_cursor__sig: 'vi',
  js_input_cursor: function(mode) {
    var K=globalThis.__kryonCanvas, c=K && K.canvas; if(!c) return;
    if(mode === 0 || mode === 2) { K.cursorHidden=false; if(c.style) c.style.cursor=K.cursor || 'default'; }
    if(mode === 1 || mode === 3) { K.cursorHidden=true; if(c.style) c.style.cursor='none'; }
    if(mode === 2 && typeof document !== 'undefined' && document.pointerLockElement === c && document.exitPointerLock) document.exitPointerLock();
    if(mode === 3 && c.requestPointerLock) { var result=c.requestPointerLock(); if(result && result.catch) result.catch(function(){}); }
  },
  js_gamepad_vibration__sig: 'viddd',
  js_gamepad_vibration: function(index, left, right, seconds) {
    var pads=typeof navigator !== 'undefined' && navigator.getGamepads ? navigator.getGamepads() : [];
    var pad=pads[index], effect=pad && pad.vibrationActuator;
    if(effect && effect.playEffect) effect.playEffect('dual-rumble', {startDelay:0,duration:Math.max(0,seconds*1000),weakMagnitude:Math.max(0,Math.min(1,right)),strongMagnitude:Math.max(0,Math.min(1,left))}).catch(function(){});
  },
});
