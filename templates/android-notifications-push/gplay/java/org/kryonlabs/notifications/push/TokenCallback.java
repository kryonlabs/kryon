package org.kryonlabs.notifications.push;

import android.content.Context;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;

/** Captures request scope for asynchronous getToken completion. */
public final class TokenCallback implements OnCompleteListener<String> {
    private final Context context;
    private final String wire;
    public TokenCallback(Context context, String wire) {
        this.context = context.getApplicationContext();
        this.wire = wire;
    }
    @Override public void onComplete(Task<String> task) { complete(context, wire, task); }
    private static native void complete(Context context, String wire, Task<String> task);
}
