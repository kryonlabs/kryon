package org.kryonlabs.platform;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;

/**
 * Lets an app in the allowlist learn the public name to offer: one column, one
 * row, nothing else. Everyone else gets nothing, as does anyone while this app
 * is signed out.
 */
public final class AccountHandoffNameProvider extends ContentProvider {
    @Override
    public boolean onCreate() {
        return true;
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] arguments, String order) {
        Context context = getContext();
        if (context == null) return null;
        String caller;
        try {
            caller = getCallingPackage();
        } catch (SecurityException error) {
            return null;
        }
        if (!AccountHandoff.trusted(context, caller)) return null;
        String name = AccountHandoff.name(context);
        if (name == null || name.isEmpty()) return null;
        MatrixCursor cursor = new MatrixCursor(new String[] {"name"}, 1);
        cursor.addRow(new Object[] {name});
        return cursor;
    }

    @Override
    public String getType(Uri uri) {
        return null;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) {
        return null;
    }

    @Override
    public int delete(Uri uri, String selection, String[] arguments) {
        return 0;
    }

    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] arguments) {
        return 0;
    }
}
