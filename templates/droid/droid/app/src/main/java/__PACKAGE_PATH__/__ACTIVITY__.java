package __PACKAGE__;

import android.app.NativeActivity;
import android.content.res.Configuration;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.util.DisplayMetrics;
import android.util.Log;
import android.view.DisplayCutout;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewTreeObserver;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import android.view.inputmethod.InputMethodManager;

/**
 * The native surface activity: edge-to-edge under the system bars, with the
 * window insets, IME state, display density, soft-keyboard text and
 * lifecycle mirrored into the Kryon host through JNI. Extend this class and
 * add your own native methods for application services.
 */
public class __ACTIVITY__ extends NativeActivity {
    static {
        System.loadLibrary("main");
    }

    private static final String TAG = "__ACTIVITY__";

    // [system bars, IME, cutouts] mirrored in native.
    private final int[] cachedInsets = new int[9];
    private boolean activityPaused = false;
    private boolean windowFocused = true;
    private boolean firstFrameReady = false;
    private long startupStarted;
    private int lastDeleteRepeatCount = -1;

    private native void nativeSetInsets(int left, int top, int right, int bottom, int imeBottom,
        int cutoutLeft, int cutoutTop, int cutoutRight, int cutoutBottom);
    private native void nativeSetDeviceDensity(float density);
    private native void nativeSetSystemDark(int dark);
    private native void nativeSetOrientation(int orientation);
    private native void nativeTextInputCommit(int codepoint);
    private native void nativeTextInputBackspace();
    private native void nativeTextInputEnter();
    private native int nativeSyncLifecycleState(boolean paused, boolean windowFocused);
    private native void nativeInvalidateGraphicsResources();

    public void onNativeFirstFrame() {
        runOnUiThread(() -> {
            if (firstFrameReady || isFinishing() || isDestroyed()) {
                return;
            }
            firstFrameReady = true;
            reportFullyDrawn();
            Log.i(TAG, "First native frame ready in "
                + (SystemClock.elapsedRealtime() - startupStarted) + " ms");
        });
    }

    public void setSoftKeyboardVisible(final boolean visible) {
        runOnUiThread(() -> {
            InputMethodManager imm =
                (InputMethodManager)getSystemService(INPUT_METHOD_SERVICE);
            View view = getWindow() != null ? getWindow().getDecorView() : null;
            if (imm == null || view == null) return;
            if (visible) {
                view.requestFocus();
                imm.showSoftInput(view, InputMethodManager.SHOW_IMPLICIT);
            } else {
                imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
            }
        });
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (event != null && event.getAction() == KeyEvent.ACTION_DOWN) {
            int keyCode = event.getKeyCode();
            if (keyCode == KeyEvent.KEYCODE_DEL) {
                int repeatCount = event.getRepeatCount();
                int deleteCount = lastDeleteRepeatCount < 0
                    ? 1
                    : Math.max(1, repeatCount - lastDeleteRepeatCount);
                lastDeleteRepeatCount = repeatCount;
                for (int i = 0; i < deleteCount; i++) {
                    nativeTextInputBackspace();
                }
            } else if (keyCode == KeyEvent.KEYCODE_ENTER ||
                       keyCode == KeyEvent.KEYCODE_NUMPAD_ENTER) {
                lastDeleteRepeatCount = -1;
                nativeTextInputEnter();
            } else {
                lastDeleteRepeatCount = -1;
                int unicode = event.getUnicodeChar();
                if (unicode >= 32) {
                    nativeTextInputCommit(unicode);
                }
            }
        } else if (event != null && event.getAction() == KeyEvent.ACTION_UP) {
            if (event.getKeyCode() == KeyEvent.KEYCODE_DEL) {
                lastDeleteRepeatCount = -1;
            }
        } else if (event != null && event.getAction() == KeyEvent.ACTION_MULTIPLE &&
                   event.getCharacters() != null) {
            lastDeleteRepeatCount = -1;
            String chars = event.getCharacters();
            for (int i = 0; i < chars.length();) {
                int codepoint = chars.codePointAt(i);
                if (codepoint >= 32) {
                    nativeTextInputCommit(codepoint);
                }
                i += Character.charCount(codepoint);
            }
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        startupStarted = SystemClock.elapsedRealtime();
        // Only window attribute calls run before the decor view exists.
        configureWindowFlags();
        super.onCreate(savedInstanceState);
        configureSystemBars();

        synchronized (cachedInsets) {
            for (int i = 0; i < cachedInsets.length; i++) {
                cachedInsets[i] = 0;
            }
        }
        setupInsetsListener();
        pushDeviceConfiguration();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        nativeInvalidateGraphicsResources();
        configureWindowFlags();
        configureSystemBars();
        pushDeviceConfiguration();
        requestInsetRefresh();
    }

    // Window flags and cutout mode need no decor view; set them before the
    // activity attaches so the first frame is already edge-to-edge.
    private void configureWindowFlags() {
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_NAVIGATION);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams attrs = getWindow().getAttributes();
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                attrs.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS;
            } else {
                attrs.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            }
            getWindow().setAttributes(attrs);
        }
    }

    private void configureSystemBars() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            getWindow().setDecorFitsSystemWindows(false);
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                controller.hide(WindowInsets.Type.statusBars());
                controller.show(WindowInsets.Type.navigationBars());
            }
        } else {
            int flags = getWindow().getDecorView().getSystemUiVisibility();
            flags |= View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
            flags |= View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION;
            flags |= View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
            getWindow().getDecorView().setSystemUiVisibility(flags);
        }
    }

    private void pushDeviceConfiguration() {
        Configuration config = getResources().getConfiguration();
        int nightMask = config.uiMode & Configuration.UI_MODE_NIGHT_MASK;
        int dark = nightMask == Configuration.UI_MODE_NIGHT_YES ? 1 : 0;
        int orientation = 0;
        if (config.orientation == Configuration.ORIENTATION_PORTRAIT) {
            orientation = 1;
        } else if (config.orientation == Configuration.ORIENTATION_LANDSCAPE) {
            orientation = 2;
        }
        nativeSetSystemDark(dark);
        nativeSetOrientation(orientation);
    }

    private void requestInsetRefresh() {
        final View decorView = getWindow().getDecorView();
        decorView.post(() -> decorView.requestApplyInsets());
    }

    private void setupInsetsListener() {
        final View decorView = getWindow().getDecorView();
        decorView.setOnApplyWindowInsetsListener((v, insets) -> {
            updateInsets(insets);
            return insets;
        });
        // Startup safety net: catches frame sizing on the initial layout pass.
        decorView.getViewTreeObserver().addOnGlobalLayoutListener(
            new ViewTreeObserver.OnGlobalLayoutListener() {
                @Override
                public void onGlobalLayout() {
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                        WindowInsets insets = decorView.getRootWindowInsets();
                        if (insets != null) {
                            updateInsets(insets);
                        }
                    }
                    decorView.getViewTreeObserver().removeOnGlobalLayoutListener(this);
                }
            });
        decorView.post(() -> decorView.requestApplyInsets());
    }

    private void updateInsets(WindowInsets insets) {
        if (insets == null) return;

        try {
            int systemLeft = 0;
            int systemTop = 0;
            int systemRight = 0;
            int systemBottom = 0;
            int imeBottom = 0;
            int cLeft = 0, cTop = 0, cRight = 0, cBottom = 0;

            // This activity owns a single edge-to-edge native surface. Java
            // reports the system bar insets; native applies them against the
            // real GL surface through Kryon's viewport policies.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                Insets systemBars =
                    insets.getInsetsIgnoringVisibility(WindowInsets.Type.systemBars());
                Insets ime = insets.getInsets(WindowInsets.Type.ime());
                systemLeft = systemBars.left;
                systemTop = systemBars.top;
                systemRight = systemBars.right;
                systemBottom = systemBars.bottom;
                imeBottom = ime.bottom;
            } else {
                systemLeft = insets.getSystemWindowInsetLeft();
                systemTop = insets.getSystemWindowInsetTop();
                systemRight = insets.getSystemWindowInsetRight();
                systemBottom = insets.getSystemWindowInsetBottom();
                imeBottom = inferImeBottom(systemBottom);
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                DisplayCutout cutout = insets.getDisplayCutout();
                if (cutout != null) {
                    cLeft = cutout.getSafeInsetLeft();
                    cTop = cutout.getSafeInsetTop();
                    cRight = cutout.getSafeInsetRight();
                    cBottom = cutout.getSafeInsetBottom();
                }
            }

            synchronized (cachedInsets) {
                cachedInsets[0] = systemLeft;
                cachedInsets[1] = systemTop;
                cachedInsets[2] = systemRight;
                cachedInsets[3] = systemBottom;
                cachedInsets[4] = imeBottom;
                cachedInsets[5] = cLeft;
                cachedInsets[6] = cTop;
                cachedInsets[7] = cRight;
                cachedInsets[8] = cBottom;
            }

            nativeSetInsets(systemLeft, systemTop, systemRight, systemBottom, imeBottom,
                cLeft, cTop, cRight, cBottom);

            DisplayMetrics metrics = new DisplayMetrics();
            getWindowManager().getDefaultDisplay().getMetrics(metrics);
            nativeSetDeviceDensity(metrics.density);
        } catch (Exception e) {
            Log.e(TAG, "Error structuralizing window layout properties: " + e.getMessage());
        }
    }

    private int inferImeBottom(int navBar) {
        View decorView = getWindow().getDecorView();
        Rect visible = new Rect();
        decorView.getWindowVisibleDisplayFrame(visible);
        int rootHeight = decorView.getRootView().getHeight();
        int hiddenBottom = rootHeight - visible.bottom;

        if (hiddenBottom <= navBar) return 0;
        return hiddenBottom;
    }

    private void syncLifecycleState() {
        nativeSyncLifecycleState(activityPaused, windowFocused);
    }

    @Override
    protected void onPause() {
        super.onPause();
        activityPaused = true;
        syncLifecycleState();
    }

    @Override
    protected void onResume() {
        super.onResume();
        activityPaused = false;
        configureSystemBars();
        nativeInvalidateGraphicsResources();
        requestInsetRefresh();
        syncLifecycleState();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        windowFocused = hasFocus;
        if (hasFocus) {
            configureSystemBars();
            requestInsetRefresh();
        }
        syncLifecycleState();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }
}
