package org.kryonlabs.play;

import android.app.Activity;
import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.PendingPurchasesParams;
import com.android.billingclient.api.ProductDetails;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.QueryProductDetailsParams;
import com.android.billingclient.api.QueryPurchasesParams;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

/**
 * Google Play Billing for the Google Play edition of a Kryon app. Native code
 * reaches it through the activity's playBilling bridge with one request:
 *   "products:a,b"  load one-time products and their prices in the user's currency
 *   "buy:a"         open Play's purchase sheet for product a
 *   "done:token"    forget a purchase that the app's server verified and consumed
 *   "state"         answer only
 * Every request answers the current state as JSON. A purchase stays listed
 * until "done", across restarts, so the server can always verify it; the
 * server consumes it, never this class.
 */
public final class PlayBilling {
    private static BillingClient client;
    private static boolean connecting;
    private static String error = "";
    private static String packageName = "";
    private static int sequence;
    private static final List<String> wanted = new ArrayList<>();
    private static final Map<String, ProductDetails> products = new LinkedHashMap<>();
    private static final Map<String, Purchase> purchases = new LinkedHashMap<>();
    private static final Set<String> finished = new HashSet<>();

    private PlayBilling() {}

    public static synchronized String call(Activity activity, String request) {
        if (request == null) {
            request = "";
        }
        packageName = activity.getPackageName();
        connect(activity);
        if (request.startsWith("products:")) {
            List<String> ids = new ArrayList<>();
            for (String id : request.substring(9).split(",")) {
                if (!id.isEmpty()) {
                    ids.add(id);
                }
            }
            if (!ids.equals(wanted) || products.isEmpty()) {
                wanted.clear();
                wanted.addAll(ids);
                queryProducts();
            }
        } else if (request.startsWith("buy:")) {
            buy(activity, request.substring(4));
        } else if (request.startsWith("done:")) {
            String token = request.substring(5);
            purchases.remove(token);
            finished.add(token);
            sequence++;
        }
        return state();
    }

    private static void connect(Activity activity) {
        if (client == null) {
            client = BillingClient.newBuilder(activity.getApplicationContext())
                .setListener(PlayBilling::purchasesUpdated)
                .enablePendingPurchases(PendingPurchasesParams.newBuilder().enableOneTimeProducts().build())
                .enableAutoServiceReconnection()
                .build();
        }
        if (client.isReady() || connecting) {
            return;
        }
        connecting = true;
        client.startConnection(new BillingClientStateListener() {
            @Override
            public void onBillingSetupFinished(BillingResult result) {
                synchronized (PlayBilling.class) {
                    connecting = false;
                    if (result.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                        error = "";
                        queryProducts();
                        queryPurchases();
                    } else {
                        error = describe(result);
                    }
                    sequence++;
                }
            }

            @Override
            public void onBillingServiceDisconnected() {
                synchronized (PlayBilling.class) {
                    connecting = false;
                    sequence++;
                }
            }
        });
    }

    private static void queryProducts() {
        if (client == null || !client.isReady() || wanted.isEmpty()) {
            return;
        }
        List<QueryProductDetailsParams.Product> list = new ArrayList<>();
        for (String id : wanted) {
            list.add(QueryProductDetailsParams.Product.newBuilder()
                .setProductId(id)
                .setProductType(BillingClient.ProductType.INAPP)
                .build());
        }
        client.queryProductDetailsAsync(QueryProductDetailsParams.newBuilder().setProductList(list).build(),
            (result, details) -> {
                synchronized (PlayBilling.class) {
                    if (result.getResponseCode() == BillingClient.BillingResponseCode.OK) {
                        products.clear();
                        for (ProductDetails item : details.getProductDetailsList()) {
                            products.put(item.getProductId(), item);
                        }
                        error = "";
                    } else {
                        error = describe(result);
                    }
                    sequence++;
                }
            });
    }

    // Purchases the server has not verified yet, such as one completed just
    // before the app closed.
    private static void queryPurchases() {
        client.queryPurchasesAsync(QueryPurchasesParams.newBuilder()
                .setProductType(BillingClient.ProductType.INAPP).build(),
            (result, list) -> {
                synchronized (PlayBilling.class) {
                    if (result.getResponseCode() == BillingClient.BillingResponseCode.OK && list != null) {
                        for (Purchase purchase : list) {
                            remember(purchase);
                        }
                    }
                    sequence++;
                }
            });
    }

    private static void purchasesUpdated(BillingResult result, List<Purchase> list) {
        synchronized (PlayBilling.class) {
            int code = result.getResponseCode();
            if (code == BillingClient.BillingResponseCode.OK && list != null) {
                for (Purchase purchase : list) {
                    remember(purchase);
                }
                error = "";
            } else if (code == BillingClient.BillingResponseCode.USER_CANCELED) {
                error = "cancelled";
            } else if (code == BillingClient.BillingResponseCode.ITEM_ALREADY_OWNED && client != null && client.isReady()) {
                queryPurchases();
            } else {
                error = describe(result);
            }
            sequence++;
        }
    }

    // A pending payment (for example cash at a shop) is listed once it completes.
    private static void remember(Purchase purchase) {
        String token = purchase.getPurchaseToken();
        if (purchase.getPurchaseState() == Purchase.PurchaseState.PURCHASED && !finished.contains(token)) {
            purchases.put(token, purchase);
        }
    }

    private static void buy(final Activity activity, String id) {
        final ProductDetails details = products.get(id);
        if (client == null || !client.isReady() || details == null) {
            error = "unavailable";
            sequence++;
            return;
        }
        BillingFlowParams.ProductDetailsParams.Builder item =
            BillingFlowParams.ProductDetailsParams.newBuilder().setProductDetails(details);
        ProductDetails.OneTimePurchaseOfferDetails offer = details.getOneTimePurchaseOfferDetails();
        if (offer != null && offer.getOfferToken() != null) {
            item.setOfferToken(offer.getOfferToken());
        }
        final BillingFlowParams params = BillingFlowParams.newBuilder()
            .setProductDetailsParamsList(Collections.singletonList(item.build()))
            .build();
        error = "";
        activity.runOnUiThread(() -> {
            BillingResult result = client.launchBillingFlow(activity, params);
            if (result.getResponseCode() != BillingClient.BillingResponseCode.OK) {
                synchronized (PlayBilling.class) {
                    error = describe(result);
                    sequence++;
                }
            }
        });
    }

    private static String state() {
        StringBuilder out = new StringBuilder();
        out.append("{\"sequence\":").append(sequence)
            .append(",\"ready\":").append(client != null && client.isReady())
            .append(",\"package\":");
        quote(out, packageName);
        out.append(",\"error\":");
        quote(out, error);
        out.append(",\"products\":[");
        boolean first = true;
        for (ProductDetails item : products.values()) {
            ProductDetails.OneTimePurchaseOfferDetails offer = item.getOneTimePurchaseOfferDetails();
            if (offer == null) {
                continue;
            }
            if (!first) {
                out.append(',');
            }
            first = false;
            out.append("{\"id\":");
            quote(out, item.getProductId());
            out.append(",\"price\":");
            quote(out, offer.getFormattedPrice());
            out.append(",\"micros\":").append(offer.getPriceAmountMicros()).append(",\"currency\":");
            quote(out, offer.getPriceCurrencyCode());
            out.append('}');
        }
        out.append("],\"purchases\":[");
        first = true;
        for (Purchase purchase : purchases.values()) {
            for (String product : purchase.getProducts()) {
                if (!first) {
                    out.append(',');
                }
                first = false;
                out.append("{\"product\":");
                quote(out, product);
                out.append(",\"token\":");
                quote(out, purchase.getPurchaseToken());
                out.append(",\"order\":");
                quote(out, purchase.getOrderId() == null ? "" : purchase.getOrderId());
                out.append('}');
            }
        }
        out.append("]}");
        return out.toString();
    }

    private static String describe(BillingResult result) {
        String message = result.getDebugMessage();
        return "code " + result.getResponseCode() + (message == null || message.isEmpty() ? "" : ": " + message);
    }

    private static void quote(StringBuilder out, String text) {
        out.append('"');
        for (int i = 0; i < text.length(); i++) {
            char c = text.charAt(i);
            if (c == '"' || c == '\\') {
                out.append('\\').append(c);
            } else if (c < 0x20) {
                out.append(String.format(Locale.ROOT, "\\u%04x", (int) c));
            } else {
                out.append(c);
            }
        }
        out.append('"');
    }
}
