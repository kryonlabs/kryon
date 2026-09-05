package com.kryonlabs.kryon;

import android.app.NativeActivity;
import android.content.SharedPreferences;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.Insets;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.view.DisplayCutout;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.WindowInsets;
import android.view.WindowManager;

import java.util.HashMap;
import java.util.Map;

public class KryonActivity extends NativeActivity {
    public static final String META_SECURE_PREFERENCES =
        "com.kryonlabs.kryon.SECURE_PREFERENCES";
    public static final String META_SECURE_DEFAULT_KEY_PREFIX =
        "com.kryonlabs.kryon.SECURE_DEFAULT_KEY_PREFIX";
    public static final String META_APP_PREFERENCES =
        "com.kryonlabs.kryon.APP_PREFERENCES";

    private final Map<String, SecureStore> secureStores = new HashMap<>();
    private TextInputBridge textInputBridge;
    private String securePreferencesName = "";
    private String secureDefaultKeyPrefix = "";
    private String appPreferencesName = "";

    private native void nativeSetInsets(int left, int top, int right, int bottom,
        int ime, int cutoutLeft, int cutoutTop, int cutoutRight, int cutoutBottom);
    private native void nativeSetDeviceDensity(float density);
    private native void nativeTextInputCommit(int codepoint);
    private native void nativeTextInputComposition(int phase, String text,
                                                   int cursor, int selectionLength);
    private native void nativeTextInputBackspace();
    private native void nativeTextInputEnter();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        loadNativeLibrary();
        super.onCreate(savedInstanceState);
        loadConfig();
        kryonApplySystemBars();
        setupInsetsListener();
        setupTextInputBridge();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            // Raylib requests FLAG_FULLSCREEN while creating the native window.
            // Restore Kryon's inset-aware system bars once that window is active.
            kryonApplySystemBars();
        }
    }

    private void loadNativeLibrary() {
        System.loadLibrary(nativeLibraryName());
    }

    private String nativeLibraryName() {
        try {
            ActivityInfo info = getPackageManager().getActivityInfo(getComponentName(),
                PackageManager.GET_META_DATA);
            if (info.metaData != null) {
                String name = info.metaData.getString("android.app.lib_name");
                if (name != null && name.length() != 0) {
                    return name;
                }
            }
        } catch (PackageManager.NameNotFoundException ignored) {
        }
        return "main";
    }

    private void loadConfig() {
        try {
            ActivityInfo info = getPackageManager().getActivityInfo(getComponentName(),
                PackageManager.GET_META_DATA);
            if (info.metaData == null) {
                return;
            }
            String prefs = info.metaData.getString(META_SECURE_PREFERENCES);
            String defaultPrefix = info.metaData.getString(META_SECURE_DEFAULT_KEY_PREFIX);
            String appPrefs = info.metaData.getString(META_APP_PREFERENCES);
            if (prefs != null) {
                securePreferencesName = prefs;
            }
            if (defaultPrefix != null) {
                secureDefaultKeyPrefix = defaultPrefix;
            }
            if (appPrefs != null) {
                appPreferencesName = appPrefs;
            }
        } catch (PackageManager.NameNotFoundException ignored) {
        }
    }

    public int[] kryonSystemThemeColors() {
        boolean dark = (getResources().getConfiguration().uiMode
                & Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_YES;
        int background = dark ? 0xFF141218 : 0xFFFFFBFE;
        int surface = dark ? 0xFF211F26 : 0xFFF7F2FA;
        int text = dark ? 0xFFE6E0E9 : 0xFF1D1B20;
        int accent = dark ? 0xFFD0BCFF : 0xFF6750A4;
        int control = dark ? 0xFFE6E0E9 : 0xFF1D1B20;
        int button = blend(accent, background, dark ? 65 : 80);
        int buttonHover = blend(accent, background, dark ? 45 : 60);

        return new int[] {
            dark ? 1 : 0,
            background,
            surface,
            text,
            accent,
            button,
            buttonHover,
            control,
            accent
        };
    }

    public void kryonApplySystemBars() {
        int[] colors = kryonSystemThemeColors();
        boolean dark = colors[0] != 0;
        getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().setStatusBarColor(colors[1]);
        getWindow().setNavigationBarColor(colors[1]);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            int flags = getWindow().getDecorView().getSystemUiVisibility();
            flags &= ~View.SYSTEM_UI_FLAG_FULLSCREEN;
            if (!dark) {
                flags |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    flags |= View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
                }
            } else {
                flags &= ~View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    flags &= ~View.SYSTEM_UI_FLAG_LIGHT_NAVIGATION_BAR;
                }
            }
            getWindow().getDecorView().setSystemUiVisibility(flags);
        }
    }

    private static int blend(int from, int to, int percentTo) {
        int p = Math.max(0, Math.min(100, percentTo));
        int a = (((from >>> 24) & 0xff) * (100 - p) + ((to >>> 24) & 0xff) * p) / 100;
        int r = (((from >>> 16) & 0xff) * (100 - p) + ((to >>> 16) & 0xff) * p) / 100;
        int g = (((from >>> 8) & 0xff) * (100 - p) + ((to >>> 8) & 0xff) * p) / 100;
        int b = ((from & 0xff) * (100 - p) + (to & 0xff) * p) / 100;
        return (a << 24) | (r << 16) | (g << 8) | b;
    }

    public boolean kryonSecureStoreBiometricAvailable() {
        return secureStore("default").isBiometricAvailable();
    }

    public boolean kryonSecureStoreBiometricSetupRequired() {
        return secureStore("default").isBiometricSetupRequired();
    }

    public boolean kryonSecureStoreHasSecret(String key) {
        return secureStore(key).hasStoredSecret();
    }

    public boolean kryonSecureStoreSecretUsesBiometric(String key) {
        return kryonSecureStoreHasSecret(key);
    }

    public int kryonSecureStoreStatus(String key) {
        return secureStore(key).status();
    }

    public String kryonSecureStoreTakeResult(String key) {
        return secureStore(key).takeResult();
    }

    public void kryonSecureStoreSaveSecret(final String key, final String secret,
            final boolean requireBiometric, final String label) {
        secureStore(key).saveSecret(secret, label);
    }

    public void kryonSecureStoreUnlockSecret(String key, String label) {
        secureStore(key).unlockSecret(label);
    }

    public void kryonSecureStoreClearSecret(String key) {
        secureStore(key).clearSecret();
    }

    private SecureStore secureStore(String key) {
        String safeKey = sanitizeKey(key);
        SecureStore store = secureStores.get(safeKey);
        if (store == null) {
            String prefs = securePreferencesName.length() == 0
                ? getPackageName() + "_secure"
                : securePreferencesName;
            String keyPrefix = "default".equals(safeKey) &&
                secureDefaultKeyPrefix.length() != 0
                    ? secureDefaultKeyPrefix
                    : "kryon_" + safeKey;
            store = new SecureStore(this, prefs, keyPrefix);
            secureStores.put(safeKey, store);
        }
        return store;
    }

    public String kryonAppStorageGetString(String scope, String key, String fallback) {
        String safeKey = sanitizeKey(key);
        String defaultValue = fallback == null ? "" : fallback;
        return appPreferences(scope).getString(safeKey, defaultValue);
    }

    public boolean kryonAppStorageHasKey(String scope, String key) {
        return appPreferences(scope).contains(sanitizeKey(key));
    }

    public boolean kryonAppStorageSetString(String scope, String key, String value) {
        String safeKey = sanitizeKey(key);
        String storedValue = value == null ? "" : value;
        appPreferences(scope).edit().putString(safeKey, storedValue).apply();
        return true;
    }

    private SharedPreferences appPreferences(String scope) {
        String base = appPreferencesName.length() == 0
            ? getPackageName() + "_preferences"
            : appPreferencesName;
        String safeScope = sanitizeKey(scope);
        if ("default".equals(safeScope)) {
            return getSharedPreferences(base, MODE_PRIVATE);
        }
        return getSharedPreferences(base + "_" + safeScope, MODE_PRIVATE);
    }

    private static String sanitizeKey(String key) {
        if (key == null || key.length() == 0) {
            return "default";
        }
        StringBuilder out = new StringBuilder(key.length());
        for (int i = 0; i < key.length(); i++) {
            char c = key.charAt(i);
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '_' || c == '-') {
                out.append(c);
            } else {
                out.append('_');
            }
        }
        return out.length() == 0 ? "default" : out.toString();
    }

    private void setupTextInputBridge() {
        textInputBridge = new TextInputBridge(this, new TextInputBridge.Callbacks() {
            @Override
            public void commitText(int codepoint) {
                nativeTextInputCommit(codepoint);
            }

            @Override
            public void composition(int phase, String text, int cursor,
                                    int selectionLength) {
                nativeTextInputComposition(phase, text, cursor, selectionLength);
            }

            @Override
            public void backspace() {
                nativeTextInputBackspace();
            }

            @Override
            public void enter() {
                nativeTextInputEnter();
            }
        });
        addContentView(textInputBridge.getView(), new ViewGroup.LayoutParams(1, 1));
    }

    public void kryonSetSoftKeyboardVisible(final boolean visible) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (textInputBridge != null) {
                    textInputBridge.setVisible(visible);
                }
            }
        });
    }

    private void setupInsetsListener() {
        final View decorView = getWindow().getDecorView();

        nativeSetDeviceDensity(getResources().getDisplayMetrics().density);

        decorView.setOnApplyWindowInsetsListener(new View.OnApplyWindowInsetsListener() {
            @Override
            public WindowInsets onApplyWindowInsets(View v, WindowInsets insets) {
                updateInsets(insets);
                return insets;
            }
        });

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
            }
        });

        decorView.post(new Runnable() {
            @Override
            public void run() {
                decorView.requestApplyInsets();
            }
        });
    }

    private void updateInsets(WindowInsets insets) {
        if (insets == null) return;

        nativeSetDeviceDensity(getResources().getDisplayMetrics().density);

        int systemLeft = 0;
        int systemTop = 0;
        int systemRight = 0;
        int systemBottom = 0;
        int imeBottom = 0;
        int cLeft = 0, cTop = 0, cRight = 0, cBottom = 0;

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Insets systemBars = insets.getInsetsIgnoringVisibility(
                    WindowInsets.Type.systemBars());
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

        nativeSetInsets(systemLeft, systemTop, systemRight, systemBottom,
                imeBottom, cLeft, cTop, cRight, cBottom);
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
}
