package com.kryon;

import android.app.Activity;
import android.os.Bundle;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.util.Log;

/**
 * Kryon Android Activity
 *
 * Main activity for Kryon apps on Android.
 * Uses OpenGL ES for rendering and JNI to interface with Kryon C code.
 */
public class KryonActivity extends Activity implements SurfaceHolder.Callback {
    private static final String TAG = "KryonActivity";

    static {
        System.loadLibrary("kryon");
    }

    private SurfaceView surfaceView;
    private long nativeContext = 0;
    private long marrowInstance = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        Log.i(TAG, "Creating Kryon activity");

        surfaceView = new SurfaceView(this);
        surfaceView.getHolder().addCallback(this);
        setContentView(surfaceView);

        // Start Marrow server
        marrowInstance = nativeStartMarrow(17010);
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        Log.i(TAG, "Destroying Kryon activity");

        if (marrowInstance != 0) {
            nativeStopMarrow(marrowInstance);
        }
        if (nativeContext != 0) {
            nativeCleanup(nativeContext);
        }
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        Log.i(TAG, "Surface created");

        int width = holder.getSurfaceFrame().width();
        int height = holder.getSurfaceFrame().height();

        nativeContext = nativeInit(width, height);
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        Log.i(TAG, "Surface changed: " + width + "x" + height);
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.i(TAG, "Surface destroyed");
    }

    // Rendering loop (called from Choreographer or timer)
    private void renderFrame() {
        if (nativeContext != 0) {
            nativeRender(nativeContext);
        }
    }

    // Native methods
    private native long nativeInit(int width, int height);
    private native void nativeRender(long context);
    private native void nativeCleanup(long context);
    private native void nativeTouch(long context, float x, float y, int action);
    private native void nativeKey(long context, int keycode, int action);
    private native long nativeStartMarrow(int port);
    private native void nativeStopMarrow(long marrow);
}
