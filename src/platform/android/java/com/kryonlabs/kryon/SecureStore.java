package com.kryonlabs.kryon;

import android.app.Activity;
import android.app.KeyguardManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.hardware.biometrics.BiometricPrompt;
import android.hardware.fingerprint.FingerprintManager;
import android.os.Build;
import android.os.CancellationSignal;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.security.keystore.UserNotAuthenticatedException;

import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.concurrent.Executor;

import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

public final class SecureStore {
    public static final int STATUS_IDLE = 0;
    public static final int STATUS_PENDING = 1;
    public static final int STATUS_OK = 2;
    public static final int STATUS_ERROR = 3;

    private static final String PREF_CIPHERTEXT = "secret_ciphertext";
    private static final String PREF_IV = "secret_iv";
    private static final String PREF_KEY_ALIAS = "secret_key_alias";
    private static final String AUTH_SUFFIX = "_auth";

    private final Activity activity;
    private final String preferencesName;
    private final String keyAlias;
    private final Object lock = new Object();
    private int status = STATUS_IDLE;
    private String result = "";

    public SecureStore(Activity activity, String preferencesName, String keyPrefix) {
        this.activity = activity;
        this.preferencesName = preferencesName;
        this.keyAlias = keyPrefix + AUTH_SUFFIX;
    }

    public boolean hasStoredSecret() {
        SharedPreferences prefs = prefs();
        return prefs.contains(PREF_CIPHERTEXT) &&
               prefs.contains(PREF_IV) &&
               keyAlias.equals(prefs.getString(PREF_KEY_ALIAS, ""));
    }

    @SuppressWarnings("deprecation")
    public boolean isBiometricAvailable() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P) return false;
        try {
            FingerprintManager fm =
                    (FingerprintManager)activity.getSystemService(Context.FINGERPRINT_SERVICE);
            KeyguardManager km =
                    (KeyguardManager)activity.getSystemService(Context.KEYGUARD_SERVICE);
            return fm != null && fm.isHardwareDetected() && fm.hasEnrolledFingerprints() &&
                   km != null && km.isDeviceSecure();
        } catch (SecurityException ignored) {
            return false;
        }
    }

    @SuppressWarnings("deprecation")
    public boolean isBiometricSetupRequired() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return false;
        try {
            FingerprintManager fm =
                    (FingerprintManager)activity.getSystemService(Context.FINGERPRINT_SERVICE);
            if (fm == null || !fm.isHardwareDetected()) return false;
            KeyguardManager km =
                    (KeyguardManager)activity.getSystemService(Context.KEYGUARD_SERVICE);
            return !fm.hasEnrolledFingerprints() || km == null || !km.isDeviceSecure();
        } catch (SecurityException ignored) {
            return false;
        }
    }

    public int status() {
        synchronized (lock) {
            return status;
        }
    }

    public String takeResult() {
        synchronized (lock) {
            String value = result == null ? "" : result;
            result = "";
            status = STATUS_IDLE;
            return value;
        }
    }

    public void saveSecret(final String secret, final String label) {
        activity.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if (secret == null || secret.length() == 0) {
                    setError("Enter a value first");
                    return;
                }
                if (!isBiometricAvailable()) {
                    setError(unavailableMessage());
                    return;
                }
                String title = label == null || label.length() == 0 ? "Save secret" : "Save " + label;
                prompt(title, "Use your fingerprint",
                        "This app will encrypt the value with an authenticated Android Keystore key",
                        new Runnable() {
                    @Override
                    public void run() {
                        encryptAndStore(secret);
                    }
                });
            }
        });
    }

    public void unlockSecret(final String label) {
        if (!hasStoredSecret()) {
            setError("No saved secret");
            return;
        }
        String title = label == null || label.length() == 0 ? "Unlock secret" : "Unlock " + label;
        prompt(title, "Use your fingerprint",
                "This app will decrypt the saved value after unlock",
                new Runnable() {
            @Override
            public void run() {
                decryptStored();
            }
        });
    }

    public void clearSecret() {
        prefs().edit().clear().apply();
        try {
            KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
            keyStore.load(null);
            if (keyStore.containsAlias(keyAlias))
                keyStore.deleteEntry(keyAlias);
        } catch (Exception ignored) {
        }
        setOk("Saved secret removed");
    }

    private SharedPreferences prefs() {
        return activity.getSharedPreferences(preferencesName, Context.MODE_PRIVATE);
    }

    private String unavailableMessage() {
        if (isBiometricSetupRequired())
            return "Set up Android screen lock and fingerprint first";
        return "Biometric unlock is not available on this device";
    }

    private void prompt(String title, String subtitle, String description, final Runnable onSuccess) {
        if (!isBiometricAvailable()) {
            setError(unavailableMessage());
            return;
        }
        synchronized (lock) {
            status = STATUS_PENDING;
            result = "";
        }
        Executor executor = new Executor() {
            @Override
            public void execute(Runnable command) {
                activity.runOnUiThread(command);
            }
        };
        BiometricPrompt prompt = new BiometricPrompt.Builder(activity)
            .setTitle(title)
            .setSubtitle(subtitle)
            .setDescription(description)
            .setNegativeButton("Cancel", executor, (dialog, which) -> setError("Canceled"))
            .build();
        prompt.authenticate(new CancellationSignal(), executor,
            new BiometricPrompt.AuthenticationCallback() {
                @Override
                public void onAuthenticationSucceeded(BiometricPrompt.AuthenticationResult authResult) {
                    onSuccess.run();
                }

                @Override
                public void onAuthenticationError(int errorCode, CharSequence errString) {
                    setError(errString == null ? "Unlock failed" : errString.toString());
                }

                @Override
                public void onAuthenticationFailed() {
                    setError("Biometric unlock failed");
                }
            });
    }

    private void encryptAndStore(String secret) {
        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, getOrCreateKey());
            byte[] encrypted = cipher.doFinal(secret.getBytes(StandardCharsets.UTF_8));
            byte[] iv = cipher.getIV();
            if (iv == null || iv.length == 0) {
                setError("Save failed: missing encryption IV");
                return;
            }
            prefs().edit()
                .putString(PREF_IV, b64(iv))
                .putString(PREF_CIPHERTEXT, b64(encrypted))
                .putString(PREF_KEY_ALIAS, keyAlias)
                .apply();
            setOk("Secret saved");
        } catch (UserNotAuthenticatedException e) {
            setError("Unlock first");
        } catch (Exception e) {
            setError("Save failed: " + e.getMessage());
        }
    }

    private void decryptStored() {
        try {
            SharedPreferences prefs = prefs();
            String ivText = prefs.getString(PREF_IV, "");
            String encryptedText = prefs.getString(PREF_CIPHERTEXT, "");
            if (ivText.length() == 0 || encryptedText.length() == 0) {
                setError("No saved secret");
                return;
            }
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, getOrCreateKey(),
                    new GCMParameterSpec(128, unb64(ivText)));
            setOk(new String(cipher.doFinal(unb64(encryptedText)), StandardCharsets.UTF_8));
        } catch (UserNotAuthenticatedException e) {
            setError("Unlock first");
        } catch (Exception e) {
            setError("Unlock failed: " + e.getMessage());
        }
    }

    private SecretKey getOrCreateKey() throws Exception {
        KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
        keyStore.load(null);
        if (keyStore.containsAlias(keyAlias))
            return (SecretKey)keyStore.getKey(keyAlias, null);
        KeyGenerator generator =
                KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        KeyGenParameterSpec.Builder spec = new KeyGenParameterSpec.Builder(
                keyAlias, KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setRandomizedEncryptionRequired(true)
            .setUserAuthenticationRequired(true);
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            spec.setUserAuthenticationParameters(30,
                    KeyProperties.AUTH_BIOMETRIC_STRONG | KeyProperties.AUTH_DEVICE_CREDENTIAL);
        } else {
            spec.setUserAuthenticationValidityDurationSeconds(30);
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N)
            spec.setInvalidatedByBiometricEnrollment(true);
        generator.init(spec.build());
        return generator.generateKey();
    }

    private void setOk(String value) {
        synchronized (lock) {
            status = STATUS_OK;
            result = value == null ? "" : value;
        }
    }

    private void setError(String value) {
        synchronized (lock) {
            status = STATUS_ERROR;
            result = value == null ? "Secure store failed" : value;
        }
    }

    private static String b64(byte[] data) {
        return android.util.Base64.encodeToString(data, android.util.Base64.NO_WRAP);
    }

    private static byte[] unb64(String text) {
        return android.util.Base64.decode(text, android.util.Base64.NO_WRAP);
    }
}
