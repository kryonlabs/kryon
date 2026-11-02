package org.kryonlabs.notifications;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

/** Android callback scaffold. Delivery and persistence live in Ziran. */
public final class NotificationReceiver extends BroadcastReceiver {
    static { System.loadLibrary("main"); }
    public static native void requestPermission(android.app.Activity activity);
    private static native void receive(Context context, Intent intent);

    @Override public void onReceive(Context context, Intent intent) {
        receive(context, intent);
    }
}
