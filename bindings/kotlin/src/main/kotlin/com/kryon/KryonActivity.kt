package com.kryon

import android.app.Activity
import android.os.Bundle
import android.view.SurfaceHolder
import android.view.SurfaceView
import android.view.MotionEvent
import android.view.KeyEvent
import android.util.Log

/**
 * Base Activity class for Kryon applications.
 *
 * Extend this class and override `onKryonCreate()` to load your Kryon UI.
 *
 * Example:
 * ```kotlin
 * class MainActivity : KryonActivity() {
 *     override fun onKryonCreate() {
 *         loadKryonFile("app.krb")  // Load bytecode
 *         // or
 *         loadKryonSource("app.kry") // Compile and run
 *     }
 * }
 * ```
 */
abstract class KryonActivity : Activity(), SurfaceHolder.Callback {

    companion object {
        private const val TAG = "KryonActivity"

        init {
            try {
                System.loadLibrary("kryon_android")
            } catch (e: UnsatisfiedLinkError) {
                Log.e(TAG, "Failed to load Kryon native library", e)
            }
        }
    }

    private var surfaceView: SurfaceView? = null
    private var surfaceCreated = false
    private var nativeHandle: Long = 0

    // ========================================================================
    // Activity Lifecycle
    // ========================================================================

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Log.i(TAG, "onCreate")

        // Create surface view for rendering
        surfaceView = SurfaceView(this).apply {
            holder.addCallback(this@KryonActivity)
        }
        setContentView(surfaceView)

        // Initialize native platform
        nativeHandle = nativeInit(this)
        if (nativeHandle == 0L) {
            Log.e(TAG, "Failed to initialize Kryon platform")
            finish()
            return
        }

        nativeOnCreate(nativeHandle)

        // Call user initialization
        onKryonCreate()
    }

    override fun onStart() {
        super.onStart()
        Log.i(TAG, "onStart")
        if (nativeHandle != 0L) {
            nativeOnStart(nativeHandle)
        }
        onKryonStart()
    }

    override fun onResume() {
        super.onResume()
        Log.i(TAG, "onResume")
        if (nativeHandle != 0L) {
            nativeOnResume(nativeHandle)
        }
        onKryonResume()
    }

    override fun onPause() {
        super.onPause()
        Log.i(TAG, "onPause")
        if (nativeHandle != 0L) {
            nativeOnPause(nativeHandle)
        }
        onKryonPause()
    }

    override fun onStop() {
        super.onStop()
        Log.i(TAG, "onStop")
        if (nativeHandle != 0L) {
            nativeOnStop(nativeHandle)
        }
        onKryonStop()
    }

    override fun onDestroy() {
        super.onDestroy()
        Log.i(TAG, "onDestroy")
        if (nativeHandle != 0L) {
            nativeOnDestroy(nativeHandle)
            nativeShutdown(nativeHandle)
            nativeHandle = 0
        }
        onKryonDestroy()
    }

    // ========================================================================
    // SurfaceHolder.Callback
    // ========================================================================

    override fun surfaceCreated(holder: SurfaceHolder) {
        Log.i(TAG, "surfaceCreated")
        surfaceCreated = true

        if (nativeHandle != 0L) {
            nativeSurfaceCreated(nativeHandle, holder.surface)
        }
    }

    override fun surfaceChanged(holder: SurfaceHolder, format: Int, width: Int, height: Int) {
        Log.i(TAG, "surfaceChanged: ${width}x${height}")

        if (nativeHandle != 0L) {
            nativeSurfaceChanged(nativeHandle, width, height)
        }
    }

    override fun surfaceDestroyed(holder: SurfaceHolder) {
        Log.i(TAG, "surfaceDestroyed")
        surfaceCreated = false

        if (nativeHandle != 0L) {
            nativeSurfaceDestroyed(nativeHandle)
        }
    }

    // ========================================================================
    // Input Events
    // ========================================================================

    override fun onTouchEvent(event: MotionEvent): Boolean {
        if (nativeHandle != 0L) {
            return nativeTouchEvent(nativeHandle, event)
        }
        return super.onTouchEvent(event)
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        if (nativeHandle != 0L && nativeKeyEvent(nativeHandle, event)) {
            return true
        }
        return super.onKeyDown(keyCode, event)
    }

    override fun onKeyUp(keyCode: Int, event: KeyEvent): Boolean {
        if (nativeHandle != 0L && nativeKeyEvent(nativeHandle, event)) {
            return true
        }
        return super.onKeyUp(keyCode, event)
    }

    // ========================================================================
    // Public API (for subclasses)
    // ========================================================================

    /**
     * Called after native platform is initialized.
     * Override this to load your Kryon UI.
     */
    protected open fun onKryonCreate() {}

    /**
     * Called when activity starts.
     */
    protected open fun onKryonStart() {}

    /**
     * Called when activity resumes.
     */
    protected open fun onKryonResume() {}

    /**
     * Called when activity pauses.
     */
    protected open fun onKryonPause() {}

    /**
     * Called when activity stops.
     */
    protected open fun onKryonStop() {}

    /**
     * Called when activity is being destroyed.
     */
    protected open fun onKryonDestroy() {}

    /**
     * Load Kryon bytecode file (.krb).
     */
    protected fun loadKryonFile(path: String): Boolean {
        if (nativeHandle == 0L) {
            Log.e(TAG, "Cannot load file: native handle is null")
            return false
        }

        return nativeLoadFile(nativeHandle, path)
    }

    /**
     * Load and compile Kryon source file (.kry).
     */
    protected fun loadKryonSource(path: String): Boolean {
        if (nativeHandle == 0L) {
            Log.e(TAG, "Cannot load source: native handle is null")
            return false
        }

        return nativeLoadSource(nativeHandle, path)
    }

    /**
     * Set a state value in the Kryon runtime.
     */
    protected fun setKryonState(key: String, value: String): Boolean {
        if (nativeHandle == 0L) return false
        return nativeSetState(nativeHandle, key, value)
    }

    /**
     * Get a state value from the Kryon runtime.
     */
    protected fun getKryonState(key: String): String? {
        if (nativeHandle == 0L) return null
        return nativeGetState(nativeHandle, key)
    }

    /**
     * Get current FPS.
     */
    fun getFPS(): Float {
        if (nativeHandle == 0L) return 0f
        return nativeGetFPS(nativeHandle)
    }

    /**
     * Check if Kryon is ready for rendering.
     */
    fun isKryonReady(): Boolean {
        return nativeHandle != 0L && surfaceCreated
    }

    // ========================================================================
    // Native Methods (JNI)
    // ========================================================================

    private external fun nativeInit(activity: Activity): Long
    private external fun nativeShutdown(handle: Long)

    private external fun nativeOnCreate(handle: Long)
    private external fun nativeOnStart(handle: Long)
    private external fun nativeOnResume(handle: Long)
    private external fun nativeOnPause(handle: Long)
    private external fun nativeOnStop(handle: Long)
    private external fun nativeOnDestroy(handle: Long)

    private external fun nativeSurfaceCreated(handle: Long, surface: Any)
    private external fun nativeSurfaceChanged(handle: Long, width: Int, height: Int)
    private external fun nativeSurfaceDestroyed(handle: Long)

    private external fun nativeTouchEvent(handle: Long, event: MotionEvent): Boolean
    private external fun nativeKeyEvent(handle: Long, event: KeyEvent): Boolean

    private external fun nativeLoadFile(handle: Long, path: String): Boolean
    private external fun nativeLoadSource(handle: Long, path: String): Boolean

    private external fun nativeSetState(handle: Long, key: String, value: String): Boolean
    private external fun nativeGetState(handle: Long, key: String): String?

    private external fun nativeGetFPS(handle: Long): Float
}
