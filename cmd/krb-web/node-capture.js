/* Pre-js for the KRB_WEB_ONESHOT node build: stubs a canvas whose 2D
 * context records the blitted ImageData, so the wasm blit path can be
 * byte-compared against the native kry_sw frame without a browser. */
var Module = {
    printErr: function(t) { console.error(t); },
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
                }
            };
        }
    },
    onRuntimeInitialized: function() {
        var FS = Module.FS;
        if (FS)
            console.error('FS root: ' + FS.readdir('/').join(','));
    },
    onExit: function() {
        if (!globalThis.__krb_frame) {
            console.error('node-capture: no frame captured');
            process.exit(1);
        }
        process.stdout.write(globalThis.__krb_frame);
    }
};
