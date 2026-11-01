package org.kryonlabs.platform;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.widget.Button;

/**
 * The signed-in app's side: the person sees which app is asking and chooses
 * "Continue as @name". getCallingPackage names the caller only when it used
 * startActivityForResult, so anything else is refused. The account is sealed
 * to the caller's one-time keys only after the person has agreed.
 */
public final class AccountHandoffConsentActivity extends Activity {
    private AlertDialog dialog;

    @Override
    protected void onCreate(Bundle saved) {
        super.onCreate(saved);
        String caller = getCallingPackage();
        String label = AccountHandoff.trusted(this, caller) ? AccountHandoff.label(this) : null;
        if (label == null || label.isEmpty()) {
            refuse();
            return;
        }
        int text = getResources().getIdentifier("kryon_account_continue_as", "string", getPackageName());
        if (text == 0) {
            refuse();
            return;
        }
        dialog = new AlertDialog.Builder(this, android.R.style.Theme_Material_Dialog_Alert)
            .setTitle(callerName(caller))
            .setPositiveButton(getString(text, label), (shown, which) -> agree())
            .setNegativeButton(android.R.string.cancel, (shown, which) -> decline())
            .setOnCancelListener(shown -> decline())
            .create();
        dialog.show();
        // A window drawn over this one must not be able to steal the tap.
        Button agree = dialog.getButton(DialogInterface.BUTTON_POSITIVE);
        if (agree != null) agree.setFilterTouchesWhenObscured(true);
    }

    private String callerName(String caller) {
        PackageManager manager = getPackageManager();
        try {
            ApplicationInfo info = manager.getApplicationInfo(caller, 0);
            return manager.getApplicationLabel(info).toString();
        } catch (PackageManager.NameNotFoundException error) {
            return caller;
        }
    }

    private void agree() {
        Intent request = getIntent();
        // Asked again in case this app signed out while the dialog was open.
        String sealed = AccountHandoff.trusted(this, getCallingPackage())
            ? AccountHandoff.seal(this, request.getStringExtra(AccountHandoff.EXTRA_BUNDLE),
                request.getStringExtra(AccountHandoff.EXTRA_ID))
            : null;
        if (sealed == null || sealed.isEmpty()) {
            refuse();
            return;
        }
        setResult(RESULT_OK, new Intent().putExtra(AccountHandoff.EXTRA_SEALED, sealed));
        finish();
    }

    private void decline() {
        setResult(RESULT_CANCELED);
        finish();
    }

    private void refuse() {
        setResult(RESULT_CANCELED, new Intent().putExtra(AccountHandoff.EXTRA_REFUSED, true));
        finish();
    }

    @Override
    protected void onDestroy() {
        if (dialog != null) {
            dialog.setOnCancelListener(null);
            dialog.dismiss();
        }
        super.onDestroy();
    }
}
