package com.kryon

import android.app.Activity
import android.os.Bundle
import android.opengl.GLSurfaceView
import android.view.MotionEvent
import android.view.KeyEvent
import android.util.Log
import com.kryon.dsl.ContainerBuilder
import javax.microedition.khronos.egl.EGLConfig
import javax.microedition.khronos.opengles.GL10

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
abstract class KryonActivity : Activity() {

    companion object {
        private const val TAG = "KryonActivity"

        init {
            try {
                System.loadLibrary("kryon_android")
            } catch (e: UnsatisfiedLinkError) {
                Log.e(TAG, "Failed to load Kryon native library", e)
            }
        }

        // Global callback registry for event handlers
        private val eventCallbacks = mutableMapOf<Int, () -> Unit>()
        private var nextCallbackId = 1

        // Called from JNI when an event occurs
        @JvmStatic
        fun invokeEventCallback(callbackId: Int) {
            eventCallbacks[callbackId]?.invoke()
        }
    }

    private var glSurfaceView: GLSurfaceView? = null
    protected var nativeHandle: Long = 0

    // Register a callback and return its ID
    internal fun registerCallback(callback: () -> Unit): Int {
        val id = nextCallbackId++
        eventCallbacks[id] = callback
        return id
    }

    // ========================================================================
    // GLSurfaceView.Renderer
    // ========================================================================

    inner class KryonGLRenderer : GLSurfaceView.Renderer {
        override fun onSurfaceCreated(gl: GL10?, config: EGLConfig?) {
            Log.i(TAG, "GL onSurfaceCreated")
            if (nativeHandle != 0L) {
                nativeGLSurfaceCreated(nativeHandle)
            }
        }

        override fun onSurfaceChanged(gl: GL10?, width: Int, height: Int) {
            Log.i(TAG, "GL onSurfaceChanged: ${width}x${height}")
            if (nativeHandle != 0L) {
                nativeGLSurfaceChanged(nativeHandle, width, height)
            }
        }

        override fun onDrawFrame(gl: GL10?) {
            if (nativeHandle != 0L) {
                nativeGLRender(nativeHandle)
            }
        }
    }

    // ========================================================================
    // Activity Lifecycle
    // ========================================================================

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Log.i(TAG, "onCreate")

        // Initialize native platform
        nativeHandle = nativeInit(this)
        if (nativeHandle == 0L) {
            Log.e(TAG, "Failed to initialize Kryon platform")
            finish()
            return
        }

        // Create GLSurfaceView with renderer
        glSurfaceView = GLSurfaceView(this).apply {
            setEGLContextClientVersion(3)  // OpenGL ES 3.0
            setRenderer(KryonGLRenderer())
            renderMode = GLSurfaceView.RENDERMODE_CONTINUOUSLY
        }
        setContentView(glSurfaceView)

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
        glSurfaceView?.onResume()
        if (nativeHandle != 0L) {
            nativeOnResume(nativeHandle)
        }
        onKryonResume()
    }

    override fun onPause() {
        super.onPause()
        Log.i(TAG, "onPause")
        glSurfaceView?.onPause()
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
     * Set content using declarative DSL builder.
     *
     * Example:
     * ```kotlin
     * setContent {
     *     Container {
     *         background("#191970")
     *         Text {
     *             text("Hello World")
     *         }
     *     }
     * }
     * ```
     */
    protected fun setContent(block: ContainerBuilder.() -> Unit) {
        if (nativeHandle == 0L) {
            Log.e(TAG, "Cannot set content: native handle is null")
            return
        }

        // Initialize DSL build session
        nativeBeginDSLBuild(nativeHandle)

        // Build component tree via DSL
        val rootBuilder = ContainerBuilder(nativeHandle)
        rootBuilder.block()

        // Finalize tree in native layer
        nativeFinalizeContent(nativeHandle)
    }

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
        return nativeHandle != 0L
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

    // GLSurfaceView.Renderer callbacks
    private external fun nativeGLSurfaceCreated(handle: Long)
    private external fun nativeGLSurfaceChanged(handle: Long, width: Int, height: Int)
    private external fun nativeGLRender(handle: Long)

    private external fun nativeTouchEvent(handle: Long, event: MotionEvent): Boolean
    private external fun nativeKeyEvent(handle: Long, event: KeyEvent): Boolean

    private external fun nativeBeginDSLBuild(handle: Long)
    private external fun nativeFinalizeContent(handle: Long)

    private external fun nativeLoadFile(handle: Long, path: String): Boolean
    private external fun nativeLoadSource(handle: Long, path: String): Boolean

    private external fun nativeSetState(handle: Long, key: String, value: String): Boolean
    private external fun nativeGetState(handle: Long, key: String): String?

    private external fun nativeGetFPS(handle: Long): Float
}
