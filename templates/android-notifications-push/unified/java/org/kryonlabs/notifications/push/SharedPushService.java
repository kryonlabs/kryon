package org.kryonlabs.notifications.push;

import org.unifiedpush.android.connector.FailedReason;
import org.unifiedpush.android.connector.PushService;
import org.unifiedpush.android.connector.data.PublicKeySet;
import org.unifiedpush.android.connector.data.PushEndpoint;
import org.unifiedpush.android.connector.data.PushMessage;

/** Platform callbacks only. Apps may subclass to handle accepted payloads. */
public class SharedPushService extends PushService {
    @Override public void onNewEndpoint(PushEndpoint endpoint, String instance) {
        PublicKeySet keys = endpoint.getPubKeySet();
        PushCallbacks.endpoint(this, 1, instance, endpoint.getUrl(),
                keys == null ? null : keys.getAuth(), keys == null ? null : keys.getPubKey());
    }
    @Override public void onRegistrationFailed(FailedReason reason, String instance) {
        PushCallbacks.failed(this, 1, instance, false);
    }
    @Override public void onUnregistered(String instance) {
        PushCallbacks.failed(this, 1, instance, false);
    }
    @Override public void onTempUnavailable(String instance) {
        PushCallbacks.failed(this, 1, instance, true);
    }
    @Override public void onMessage(PushMessage message, String instance) {
        if (PushCallbacks.accepts(this, 1, instance)) onPushMessage(message, instance);
    }
    protected void onPushMessage(PushMessage message, String instance) {}
}
