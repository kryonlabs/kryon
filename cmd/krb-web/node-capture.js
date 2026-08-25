/* Pre-js for the KRB_WEB_ONESHOT node build: stubs a canvas whose 2D
 * context records the blitted ImageData, so the wasm blit path can be
 * byte-compared against the native kry_sw frame without a browser. */
var Module = {
    printErr: function(t) { console.error(t); },
    preRun: [function() {
        var path = process.argv[2];
        if (!path) {
            console.error('node-capture: missing cartridge path');
            process.exit(2);
        }
        var data = require('fs').readFileSync(path);
        FS.writeFile('/app.krb', new Uint8Array(data));
    }],
    canvas: {
        getContext: function() {
            return {
                createImageData: function(w, h) {
                    return { width: w, height: h,
                             data: new Uint8ClampedArray(w * h * 4) };
                },
                putImageData: function(img) {
                    if (globalThis.__krb_frame)
                        return;
                    globalThis.__krb_frame = Buffer.from(img.data);
                    require('fs').writeSync(1, globalThis.__krb_frame);
                    process.exit(0);
                }
            };
        }
    },
    onRuntimeInitialized: function() {
        var rc = Module._krb_web_start();
        if (rc)
            process.exit(rc);
    },
    onExit: function() {
        if (!globalThis.__krb_frame) {
            console.error('node-capture: no frame captured');
            process.exit(1);
        }
    }
};
