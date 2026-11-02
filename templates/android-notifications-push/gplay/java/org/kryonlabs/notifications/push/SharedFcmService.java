package org.kryonlabs.notifications.push;

import com.google.firebase.messaging.FirebaseMessagingService;
import com.google.firebase.messaging.RemoteMessage;

/** Apps subclass for their existing payload handler; no automatic display. */
public class SharedFcmService extends FirebaseMessagingService {
    static { System.loadLibrary("main"); }
    @Override public void handleIntent(android.content.Intent intent) {
        if (ingress(this, intent)) super.handleIntent(intent);
    }
    @Override public void onNewToken(String token) { refresh(this, token); }
    @Override public void onMessageReceived(RemoteMessage message) {
        if (acceptsMessage(this, message)) onPushMessage(message);
    }
    protected void onPushMessage(RemoteMessage message) {}
    private static native void refresh(android.content.Context context, String token);
    private static native boolean ingress(android.content.Context context, android.content.Intent intent);
    private static native boolean acceptsMessage(android.content.Context context, RemoteMessage message);
}
