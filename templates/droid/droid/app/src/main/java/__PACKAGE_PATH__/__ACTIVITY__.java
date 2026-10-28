package __PACKAGE__;

import android.app.NativeActivity;
import android.content.Intent;
import android.content.ClipData;
import android.content.ClipboardManager;
import java.nio.charset.StandardCharsets;
import java.io.File;
import java.io.FileOutputStream;
import java.io.FileInputStream;
import java.io.OutputStream;
import java.io.InputStream;
import android.net.Uri;
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
import android.view.ViewGroup;
import android.widget.EditText;
import android.text.Editable;
import android.text.TextWatcher;
import android.text.InputType;
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
    private String pendingBundle;
    private int lastDeleteRepeatCount = -1;
    private static final int DOCUMENT_PICKER = 19741;
    private int documentRequest = -1;
    private boolean documentSave;
    private Uri saveDocument;
    private File saveStage;
    private EditText inputEditor;
    private boolean applyingInput;
    private long inputTarget;
    private long inputRevision;

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
    private native void nativeImportSelectedFile(int request, String path);
    private native void nativeImportCancelled(int request);
    private native void nativeTextInputState(long target, long revision, byte[] value, int cursor, int anchor);

    private EditText inputEditor() {
        if (inputEditor != null) return inputEditor;
        inputEditor = new EditText(this) {
            @Override protected void onSelectionChanged(int start, int end) {
                super.onSelectionChanged(start, end);
                publishInput();
            }
        };
        inputEditor.setAlpha(0);
        inputEditor.setPadding(0, 0, 0, 0);
        inputEditor.setBackgroundColor(android.graphics.Color.TRANSPARENT);
        inputEditor.setFocusableInTouchMode(true);
        inputEditor.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        inputEditor.addTextChangedListener(new TextWatcher() {
            public void beforeTextChanged(CharSequence value, int start, int count, int after) {}
            public void onTextChanged(CharSequence value, int start, int before, int count) {}
            public void afterTextChanged(Editable value) { publishInput(); }
        });
        inputEditor.setOnEditorActionListener((view, action, event) -> {
            nativeTextInputEnter();
            return true;
        });
        addContentView(inputEditor, new ViewGroup.LayoutParams(1, 1));
        return inputEditor;
    }

    private void publishInput() {
        if (applyingInput || inputEditor == null || inputTarget == 0) return;
        String text = inputEditor.getText().toString();
        int start = Math.max(0, inputEditor.getSelectionStart());
        int end = Math.max(0, inputEditor.getSelectionEnd());
        nativeTextInputState(inputTarget, ++inputRevision, text.getBytes(StandardCharsets.UTF_8),
            text.substring(0, end).getBytes(StandardCharsets.UTF_8).length,
            text.substring(0, start).getBytes(StandardCharsets.UTF_8).length);
    }

    private static int utf16Offset(String text, int bytes) {
        int at = 0;
        int used = 0;
        while (at < text.length() && used < bytes) {
            int next = at + Character.charCount(text.codePointAt(at));
            int size = text.substring(at, next).getBytes(StandardCharsets.UTF_8).length;
            if (used + size > bytes) break;
            used += size;
            at = next;
        }
        return at;
    }

    public void setTextInputState(long target, long revision, byte[] value, int cursor, int anchor,
                                  boolean secure, boolean numeric, boolean multiline) {
        runOnUiThread(() -> {
            if (inputTarget == target && revision < inputRevision) return;
            EditText editor = inputEditor();
            applyingInput = true;
            try {
                int type = numeric ? InputType.TYPE_CLASS_NUMBER : InputType.TYPE_CLASS_TEXT;
                if (secure) type |= InputType.TYPE_TEXT_VARIATION_PASSWORD;
                if (multiline) type |= InputType.TYPE_TEXT_FLAG_MULTI_LINE;
                if (editor.getInputType() != type) editor.setInputType(type);
                inputTarget = target;
                inputRevision = revision;
                String text = new String(value, StandardCharsets.UTF_8);
                if (!editor.getText().toString().equals(text)) editor.setText(text);
                editor.setSelection(utf16Offset(text, anchor), utf16Offset(text, cursor));
            } finally {
                applyingInput = false;
            }
        });
    }

    public void openImportPicker(String mime, int request) {
        openDocumentPicker(mime, null, request);
    }

    public void openSavePicker(String filename, String mime, int request) {
        openDocumentPicker(mime, filename, request);
    }

    private void openDocumentPicker(String mime, String filename, int request) {
        runOnUiThread(() -> {
            if (documentRequest >= 0) nativeImportCancelled(documentRequest);
            documentRequest = request;
            documentSave = filename != null;
            Intent intent = new Intent(documentSave ? Intent.ACTION_CREATE_DOCUMENT : Intent.ACTION_OPEN_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType(mime == null || mime.isEmpty() ? "*/*" : mime);
            if (documentSave) intent.putExtra(Intent.EXTRA_TITLE, filename);
            try {
                startActivityForResult(intent, DOCUMENT_PICKER);
            } catch (RuntimeException error) {
                documentRequest = -1;
                nativeImportCancelled(request);
                Log.w(TAG, "Could not open document picker", error);
            }
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != DOCUMENT_PICKER || documentRequest < 0) return;
        int request = documentRequest;
        documentRequest = -1;
        if (resultCode != RESULT_OK || data == null || data.getData() == null) {
            nativeImportCancelled(request);
            return;
        }
        File stage = null;
        try {
            stage = File.createTempFile("kryon-document-", ".tmp", getCacheDir());
            if (documentSave) {
                synchronized (this) {
                    if (saveStage != null) saveStage.delete();
                    saveStage = stage;
                    saveDocument = data.getData();
                }
            } else {
                try (InputStream input = getContentResolver().openInputStream(data.getData());
                     OutputStream output = new FileOutputStream(stage)) {
                    if (input == null) throw new java.io.IOException("No document stream");
                    copyDocument(input, output);
                }
            }
            nativeImportSelectedFile(request, stage.getAbsolutePath());
        } catch (Exception error) {
            if (stage != null) stage.delete();
            nativeImportCancelled(request);
            Log.w(TAG, "Could not read selected document", error);
        }
    }

    private static void copyDocument(InputStream input, OutputStream output) throws java.io.IOException {
        byte[] buffer = new byte[8192];
        int total = 0;
        int count;
        while ((count = input.read(buffer)) != -1) {
            total += count;
            if (total > 1024 * 1024) throw new java.io.IOException("Document exceeds 1 MiB");
            output.write(buffer, 0, count);
        }
    }

    public synchronized boolean savePickedFile(String stagedPath) {
        if (saveDocument == null || saveStage == null || !saveStage.getAbsolutePath().equals(stagedPath)) return false;
        try (InputStream input = new FileInputStream(saveStage);
             OutputStream output = getContentResolver().openOutputStream(saveDocument, "wt")) {
            if (output == null) return false;
            copyDocument(input, output);
            output.flush();
            saveStage.delete();
            saveStage = null;
            saveDocument = null;
            return true;
        } catch (Exception error) {
            Log.w(TAG, "Could not save selected document", error);
            return false;
        }
    }

    // The native clipboard bridge uses bytes so full UTF-8 survives JNI.
    public byte[] readClipboardText() {
        try {
            ClipboardManager clipboard = (ClipboardManager)getSystemService(CLIPBOARD_SERVICE);
            ClipData data = clipboard == null ? null : clipboard.getPrimaryClip();
            if (data == null || data.getItemCount() == 0) return new byte[0];
            CharSequence text = data.getItemAt(0).coerceToText(this);
            return (text == null ? "" : text.toString()).getBytes(StandardCharsets.UTF_8);
        } catch (RuntimeException error) {
            Log.w(TAG, "Could not read the clipboard", error);
            return null;
        }
    }

    public boolean writeClipboardText(byte[] bytes) {
        if (bytes == null) return false;
        try {
            ClipboardManager clipboard = (ClipboardManager)getSystemService(CLIPBOARD_SERVICE);
            if (clipboard == null) return false;
            clipboard.setPrimaryClip(ClipData.newPlainText("", new String(bytes, StandardCharsets.UTF_8)));
            return true;
        } catch (RuntimeException error) {
            Log.w(TAG, "Could not write the clipboard", error);
            return false;
        }
    }

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
            View view = visible ? inputEditor() : inputEditor;
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
        if (inputEditor != null && inputEditor.hasFocus() && inputTarget != 0) {
            return super.dispatchKeyEvent(event);
        }
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

    private synchronized void stageBundle(Intent intent) {
        if (intent == null || !Intent.ACTION_VIEW.equals(intent.getAction()) || intent.getData() == null) return;
        File incoming = new File(getCacheDir(), "zx-incoming");
        File selected = new File(getCacheDir(), "zx-selected");
        try (InputStream source = getContentResolver().openInputStream(intent.getData());
             FileOutputStream output = new FileOutputStream(incoming)) {
            if (source == null) throw new java.io.IOException("Unavailable document");
            byte[] bytes = new byte[65536];
            long size = 0;
            int count;
            while ((count = source.read(bytes)) != -1) {
                size += count;
                if (size > 64L * 1024 * 1024) throw new java.io.IOException("Bundle exceeds 64 MiB");
                output.write(bytes, 0, count);
            }
            output.getFD().sync();
            if (!incoming.renameTo(selected)) throw new java.io.IOException("Could not retain document");
            pendingBundle = selected.getAbsolutePath();
        } catch (Exception error) {
            incoming.delete();
            Log.w("ZX", "Could not read the selected bundle", error);
        }
    }

    public synchronized String takeBundleFile() {
        String path = pendingBundle;
        pendingBundle = null;
        return path;
    }

    public void bundleResult(int accepted) {
        if (accepted != 0) {
            Log.i("ZX", "Bundle loaded");
        } else {
            Log.w("ZX", "Bundle rejected");
            runOnUiThread(() -> android.widget.Toast.makeText(this,
                "Could not open this ZX file", android.widget.Toast.LENGTH_LONG).show());
        }
    }

    @Override
    protected void onNewIntent(Intent intent) {
        super.onNewIntent(intent);
        setIntent(intent);
        stageBundle(intent);
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        startupStarted = SystemClock.elapsedRealtime();
        // Only window attribute calls run before the decor view exists.
        configureWindowFlags();
        super.onCreate(savedInstanceState);
        stageBundle(getIntent());
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
