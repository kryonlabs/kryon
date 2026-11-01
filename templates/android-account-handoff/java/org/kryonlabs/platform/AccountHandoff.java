package org.kryonlabs.platform;

import android.app.Activity;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.ProviderInfo;
import android.content.pm.ResolveInfo;
import android.content.pm.Signature;
import android.content.pm.SigningInfo;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.SystemClock;
import java.io.File;
import java.io.IOException;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.util.TreeSet;

/**
 * One app on a device asking another for its account. Platform wiring only:
 * who may ask, the seal and the account all belong to the app's native code,
 * which registers the natives below when the library loads. Without them an
 * app offers nothing and asks for nothing, and the person signs in as usual.
 * Nothing here logs: a request carries public keys and a reply carries a
 * message only the asking app can open, but neither belongs in a log.
 */
public final class AccountHandoff {
    static final String ACTION = "org.kryonlabs.account.CONTINUE";
    static final String EXTRA_BUNDLE = "bundle";
    static final String EXTRA_ID = "id";
    static final String EXTRA_SEALED = "sealed";
    static final String EXTRA_REFUSED = "refused";
    static final String EXTRA_PACKAGE = "package";
    private static final String CONSENT_ACTIVITY = "org.kryonlabs.platform.AccountHandoffConsentActivity";
    private static final long REFRESH_MILLIS = 15000;

    private static boolean natives;
    private static String offeredPackage = "";
    private static String offeredName = "";
    private static long nextRefresh;
    private static boolean refreshing;
    // "pending", "cancelled", "refused" or "sealed:<message>".
    private static String answer = "cancelled";

    static {
        try {
            System.loadLibrary("main");
            natives = true;
        } catch (UnsatisfiedLinkError error) {
            natives = false;
        }
    }

    private AccountHandoff() {}

    private static native boolean nativeAllowed(String packageName, String certificate);
    private static native String nativeName(String root);
    private static native String nativeLabel(String root);
    private static native String nativeSeal(String root, String bundle, String id);

    /** Whether this exact package, as installed, is in the app's allowlist. */
    static boolean trusted(Context context, String packageName) {
        if (!natives || packageName == null) return false;
        String certificate = certificate(context.getPackageManager(), packageName);
        if (certificate == null) return false;
        try {
            return nativeAllowed(packageName, certificate);
        } catch (UnsatisfiedLinkError error) {
            return false;
        }
    }

    /** The SHA-256 of the package's one current signer as lowercase hex, or null. */
    static String certificate(PackageManager manager, String packageName) {
        try {
            Signature[] signers;
            if (Build.VERSION.SDK_INT >= 28) {
                PackageInfo info = manager.getPackageInfo(packageName, PackageManager.GET_SIGNING_CERTIFICATES);
                SigningInfo signing = info.signingInfo;
                if (signing == null || signing.hasMultipleSigners()) return null;
                signers = signing.getApkContentsSigners();
            } else {
                PackageInfo info = manager.getPackageInfo(packageName, PackageManager.GET_SIGNATURES);
                signers = info.signatures;
            }
            if (signers == null || signers.length != 1) return null;
            byte[] digest = MessageDigest.getInstance("SHA-256").digest(signers[0].toByteArray());
            StringBuilder text = new StringBuilder(digest.length * 2);
            for (byte value : digest) {
                text.append(Character.forDigit((value >> 4) & 15, 16));
                text.append(Character.forDigit(value & 15, 16));
            }
            return text.toString();
        } catch (PackageManager.NameNotFoundException | NoSuchAlgorithmException | RuntimeException error) {
            return null;
        }
    }

    /** This app's folder for the copy its native code keeps, or null. */
    private static String root(Context context) {
        try {
            File folder = context.getNoBackupFilesDir();
            return folder == null ? null : folder.getCanonicalPath() + "/waozi-account";
        } catch (IOException error) {
            return null;
        }
    }

    /** The public name this app offers, or null when it is signed out. */
    static String name(Context context) {
        String root = root(context);
        if (!natives || root == null) return null;
        try {
            return nativeName(root);
        } catch (UnsatisfiedLinkError error) {
            return null;
        }
    }

    /** The name as the consent dialog shows it, or null when signed out. */
    static String label(Context context) {
        String root = root(context);
        if (!natives || root == null) return null;
        try {
            return nativeLabel(root);
        } catch (UnsatisfiedLinkError error) {
            return null;
        }
    }

    /** The account sealed to the asker's keys, or null if it cannot be. */
    static String seal(Context context, String bundle, String id) {
        String root = root(context);
        if (!natives || root == null || bundle == null || id == null) return null;
        try {
            return nativeSeal(root, bundle, id);
        } catch (UnsatisfiedLinkError error) {
            return null;
        }
    }

    /**
     * The app's native code calls this through the activity's class loader:
     * "offer" answers a name or "", "start:ID:BUNDLE" answers "started" or "",
     * "poll" answers the state above, and "cancel" ends a request.
     */
    public static String call(Activity activity, String request) {
        if (!natives || activity == null || request == null) return "";
        if (request.equals("offer")) return offer(activity);
        if (request.equals("poll")) return answer();
        if (request.equals("cancel")) {
            finish("cancelled");
            return "";
        }
        if (request.startsWith("start:")) return start(activity, request.substring(6));
        return "";
    }

    private static synchronized String offer(Activity activity) {
        long now = SystemClock.elapsedRealtime();
        if (!refreshing && now >= nextRefresh) {
            refreshing = true;
            nextRefresh = now + REFRESH_MILLIS;
            final Context context = activity.getApplicationContext();
            new Thread(() -> refresh(context), "account-handoff").start();
        }
        return offeredName;
    }

    // Looks for a signed-in app. A package has to pass the allowlist before it
    // is asked, and packages are tried in a fixed order so the offer is stable.
    private static void refresh(Context context) {
        String foundPackage = "";
        String foundName = "";
        try {
            PackageManager manager = context.getPackageManager();
            TreeSet<String> packages = new TreeSet<>();
            for (ResolveInfo info : manager.queryIntentActivities(new Intent(ACTION), 0)) {
                if (info.activityInfo != null) packages.add(info.activityInfo.packageName);
            }
            packages.remove(context.getPackageName());
            for (String packageName : packages) {
                if (!trusted(context, packageName)) continue;
                String name = askName(context, packageName);
                if (name == null || name.isEmpty()) continue;
                foundPackage = packageName;
                foundName = name;
                break;
            }
        } catch (RuntimeException error) {
            foundPackage = "";
            foundName = "";
        }
        synchronized (AccountHandoff.class) {
            offeredPackage = foundPackage;
            offeredName = foundName;
            refreshing = false;
        }
    }

    // An app can claim another's provider name when that app is not installed,
    // so the provider has to belong to the package that passed the allowlist.
    private static String askName(Context context, String packageName) {
        String authority = packageName + ".account-handoff";
        ProviderInfo provider = context.getPackageManager().resolveContentProvider(authority, 0);
        if (provider == null || !packageName.equals(provider.packageName)) return null;
        ContentResolver resolver = context.getContentResolver();
        try (Cursor cursor = resolver.query(new Uri.Builder().scheme("content").authority(authority).path("name").build(),
                null, null, null, null)) {
            if (cursor == null || !cursor.moveToFirst() || cursor.getColumnCount() != 1) return null;
            return cursor.getString(0);
        } catch (RuntimeException error) {
            return null;
        }
    }

    private static String start(Activity activity, String request) {
        int split = request.indexOf(':');
        if (split <= 0) return "";
        String packageName;
        synchronized (AccountHandoff.class) {
            packageName = offeredPackage;
            if (packageName.isEmpty()) return "";
            answer = "pending";
        }
        final Intent intent = new Intent(activity, AccountHandoffRequestActivity.class)
            .putExtra(EXTRA_PACKAGE, packageName)
            .putExtra(EXTRA_ID, request.substring(0, split))
            .putExtra(EXTRA_BUNDLE, request.substring(split + 1));
        activity.runOnUiThread(() -> {
            try {
                activity.startActivity(intent);
            } catch (RuntimeException error) {
                finish("cancelled");
            }
        });
        return "started";
    }

    /** The consent activity of an app that has passed the allowlist. */
    static Intent consent(String packageName) {
        return new Intent(ACTION).setComponent(new ComponentName(packageName, CONSENT_ACTIVITY));
    }

    static synchronized void finish(String value) {
        answer = value;
    }

    /** Ends a request unless its answer already arrived. */
    static synchronized void abandon() {
        if (answer.equals("pending")) answer = "cancelled";
    }

    /** Forgets the offer, as when the other app has just refused. */
    static synchronized void forgetOffer() {
        offeredPackage = "";
        offeredName = "";
        nextRefresh = 0;
    }

    // A finished answer is handed over once; the sealed message goes with it.
    private static synchronized String answer() {
        String value = answer;
        if (!value.equals("pending")) answer = "cancelled";
        return value;
    }
}
