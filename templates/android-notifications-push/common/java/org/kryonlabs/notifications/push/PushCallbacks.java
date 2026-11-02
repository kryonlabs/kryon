package org.kryonlabs.notifications.push;

import android.content.Context;

/** Native transport API. Ziran owns selection, persistence and cancellation.
 * Snapshot contains credentials: pass it only to the app's existing server
 * registration code, never logs. One active account scope per package.
 */
public final class PushCallbacks {
    static { System.loadLibrary("main"); }
    private PushCallbacks() {}
    public static native int start(Context context, String scope, String label);
    public static native boolean cancel(Context context, String scope);
    public static native String snapshot(Context context, String scope);
    public static native boolean endpoint(Context context, int provider, String wire,
            String endpoint, String auth, String p256dh);
    public static native void failed(Context context, int provider, String wire, boolean temporary);
    public static native boolean accepts(Context context, int provider, String wire);
}
