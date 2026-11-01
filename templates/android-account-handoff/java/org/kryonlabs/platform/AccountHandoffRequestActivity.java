package org.kryonlabs.platform;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;

/**
 * This app's side of a request: an invisible activity that is never exported.
 * It rechecks the other app against the allowlist, because the bundle should
 * only go to, and an account only come from, an app the allowlist names.
 */
public final class AccountHandoffRequestActivity extends Activity {
    private static final int REQUEST = 19742;
    private static final String STARTED = "started";

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        if (saved != null && saved.getBoolean(STARTED)) return;
        Intent request = getIntent();
        String packageName = request.getStringExtra(AccountHandoff.EXTRA_PACKAGE);
        String id = request.getStringExtra(AccountHandoff.EXTRA_ID);
        String bundle = request.getStringExtra(AccountHandoff.EXTRA_BUNDLE);
        if (id == null || bundle == null || !AccountHandoff.trusted(this, packageName)) {
            AccountHandoff.forgetOffer();
            AccountHandoff.finish("refused");
            finish();
            return;
        }
        Intent consent = AccountHandoff.consent(packageName)
            .putExtra(AccountHandoff.EXTRA_ID, id)
            .putExtra(AccountHandoff.EXTRA_BUNDLE, bundle);
        try {
            startActivityForResult(consent, REQUEST);
        } catch (RuntimeException error) {
            AccountHandoff.forgetOffer();
            AccountHandoff.finish("refused");
            finish();
        }
    }

    @Override
    protected void onSaveInstanceState(Bundle output) {
        super.onSaveInstanceState(output);
        output.putBoolean(STARTED, true);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != REQUEST) return;
        String sealed = data == null ? null : data.getStringExtra(AccountHandoff.EXTRA_SEALED);
        if (resultCode == RESULT_OK && sealed != null) {
            AccountHandoff.finish("sealed:" + sealed);
        } else if (data != null && data.getBooleanExtra(AccountHandoff.EXTRA_REFUSED, false)) {
            AccountHandoff.forgetOffer();
            AccountHandoff.finish("refused");
        } else {
            AccountHandoff.finish("cancelled");
        }
        finish();
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (isFinishing()) AccountHandoff.abandon();
    }
}
