package org.kryonlabs.platform;

import android.content.Intent;
import java.util.ArrayList;

/** MIME filters for Android's system document picker. */
public final class DocumentTypes {
    private DocumentTypes() {}

    public static void apply(Intent intent, String mimeTypesCsv) {
        ArrayList<String> types = new ArrayList<>();
        if (mimeTypesCsv != null) {
            for (String value : mimeTypesCsv.split(",")) {
                String type = value.trim();
                if (!type.isEmpty() && !types.contains(type)) types.add(type);
            }
        }
        // Android requires */* for multiple MIME types. EXTRA_MIME_TYPES
        // supplies the actual allowlist, including types from different families.
        intent.setType(types.size() == 1 ? types.get(0) : "*/*");
        intent.removeExtra(Intent.EXTRA_MIME_TYPES);
        if (types.size() > 1) {
            intent.putExtra(Intent.EXTRA_MIME_TYPES, types.toArray(new String[0]));
        }
    }
}
