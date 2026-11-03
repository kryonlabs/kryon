#!/usr/bin/env python3
"""Play request/account binding regression, without a device or payment service.
Compile the complete production bridge against the actual Android/Billing SDKs;
execute its request dispatch and buy/hash methods with a recorded billing flow.
"""
import hashlib
import os
from pathlib import Path
import re
import subprocess
import zipfile

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT / 'work/play-billing'
SOURCE = (ROOT / 'templates/droid/droid/app/src/gplay/java/org/kryonlabs/play/PlayBilling.java').read_text()


def run(*args, **kw):
    subprocess.run(list(map(str, args)), check=True, timeout=180, **kw)


def method(name):
    match = re.search(r'^    (?:public|private)[^\n]*\b' + name + r'\([^\n]*\) \{', SOURCE, re.M)
    assert match, name
    end, depth = match.end(), 1
    while depth:
        depth += (SOURCE[end] == '{') - (SOURCE[end] == '}')
        end += 1
    return SOURCE[match.start():end]


FIXTURE = r'''
import java.util.*;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
class Activity {
    List<Runnable> queued = new ArrayList<>();
    String getPackageName() { return "org.kryonlabs.fixture"; }
    void runOnUiThread(Runnable r) { queued.add(r); }
    void drain() { for (Runnable r : queued) r.run(); queued.clear(); }
}
class ProductDetails {
    static class OneTimePurchaseOfferDetails { String getOfferToken() { return "offer"; } }
    OneTimePurchaseOfferDetails getOneTimePurchaseOfferDetails() { return new OneTimePurchaseOfferDetails(); }
}
class BillingResult { int getResponseCode() { return 0; } }
class BillingClient {
    static class BillingResponseCode { static final int OK = 0; }
    boolean isReady() { return true; }
    int launches; BillingFlowParams last; List<String> bindings = new ArrayList<>();
    BillingResult launchBillingFlow(Activity a, BillingFlowParams p) { launches++; last = p; bindings.add(p.account); return new BillingResult(); }
}
class BillingFlowParams {
    String account; List<ProductDetailsParams> items;
    static class ProductDetailsParams {
        ProductDetails details; String offer;
        static Builder newBuilder() { return new Builder(); }
        static class Builder {
            ProductDetailsParams p = new ProductDetailsParams();
            Builder setProductDetails(ProductDetails d) { p.details = d; return this; }
            Builder setOfferToken(String s) { p.offer = s; return this; }
            ProductDetailsParams build() { return p; }
        }
    }
    static Builder newBuilder() { return new Builder(); }
    static class Builder {
        BillingFlowParams p = new BillingFlowParams();
        Builder setProductDetailsParamsList(List<ProductDetailsParams> i) { p.items = i; return this; }
        Builder setObfuscatedAccountId(String a) { p.account = a; return this; }
        BillingFlowParams build() { return p; }
    }
}
public class BillingFixture {
    static BillingClient client = new BillingClient();
    static String error = "", packageName = ""; static int sequence;
    static List<String> wanted = new ArrayList<>();
    static Map<String, ProductDetails> products = new HashMap<>();
    static Map<String, Object> purchases = new HashMap<>();
    static Set<String> finished = new HashSet<>();
    static void connect(Activity a) {} static void queryProducts() {}
    static String state() { return error; } static String describe(BillingResult r) { return "failed"; }
__PRODUCTION__
    static void require(boolean b) { if (!b) throw new AssertionError(); }
    public static void main(String[] args) {
        Activity activity = new Activity();
        products.put("dew_100", new ProductDetails());
        for (String invalid : new String[]{"buy:dew_100", "buy:dew_100|", "buy:|a", "buy:dew_100|a|b", "buy:dew_100|a\0b", "buy:dew\0_100|a"}) {
            int seq = sequence;
            require(call(activity, invalid).equals("unavailable"));
            require(sequence == seq + 1 && activity.queued.isEmpty() && client.launches == 0);
        }
        call(activity, "buy:missing|account"); require(activity.queued.isEmpty());
        call(activity, "buy:dew_100|account-a");
        call(activity, "buy:dew_100|account-b");
        require(client.launches == 0); // UI work deferred: bindings must remain per request
        activity.drain();
        require(client.launches == 2 && client.last.account.equals(args[1]));
        require(client.bindings.equals(Arrays.asList(args[0], args[1])));
        require(client.last.items.get(0).details == products.get("dew_100"));
        require(client.last.items.get(0).offer.equals("offer"));
        require(accountBinding("account-a").equals(args[0]));
        require(accountBinding("用户🚀").equals(args[2]));
        require(accountBinding("account-a").matches("[0-9a-f]{64}"));
        call(activity, "buy:dew_100|用户🚀"); activity.drain();
        require(client.last.account.equals(args[2]));
        System.out.println("Play Java: malformed/unbound rejection, deferred account switching, UTF-8 SHA256 and SDK parameters passed");
    }
}
'''


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    sdk = Path(os.environ.get('ANDROID_HOME', str(Path.home() / 'Android/Sdk')))
    jars = sorted(sdk.glob('platforms/*/android.jar'))
    billing = list((Path.home() / '.gradle/caches/modules-2/files-2.1/com.android.billingclient/billing/9.1.0').glob('*/*.aar'))
    assert jars and billing, 'Android SDK and Billing 9.1.0 AAR required (or run a gplay build first)'
    with zipfile.ZipFile(billing[0]) as aar:
        (WORK / 'billing.jar').write_bytes(aar.read('classes.jar'))
    run('javac', '-cp', f'{jars[-1]}:{WORK / "billing.jar"}', '-d', WORK / 'sdk', ROOT / 'templates/droid/droid/app/src/gplay/java/org/kryonlabs/play/PlayBilling.java')
    print('Complete PlayBilling.java compiled against Android SDK and Billing 9.1.0', flush=True)
    fixture = WORK / 'BillingFixture.java'
    fixture.write_text(FIXTURE.replace('__PRODUCTION__', '\n'.join(method(n) for n in ('call', 'buy', 'accountBinding')).replace('PlayBilling.class', 'BillingFixture.class')))
    run('javac', '-d', WORK / 'jvm', fixture)
    hashes = [hashlib.sha256(s.encode()).hexdigest() for s in ('account-a', 'account-b', '用户🚀')]
    run('java', '-cp', WORK / 'jvm', 'BillingFixture', *hashes)

    # Execute the exact production request writer, replacing only the JNI boundary.
    zi = (ROOT / 'src/backend/play_billing.zi').read_text()
    writer = zi[zi.index('PlayBillingBuy ::'):zi.index('\nPlayBillingAsk ::')]
    test = WORK / 'billing_request.zi'
    test.write_text('''#import "std/byte_text_linux"
PlayBillingCall :: (request: string, output: []u8) -> string {
    if output.count < request.count { return "" }
    for byte, at: request { output[at] = byte }
    return TextView(output[:request.count])
}
''' + writer + '''
Main :: () -> s32 {
    out: [1200]u8
    if PlayBillingBuy("dew_100", "account-a", out[:]) != "buy:dew_100|account-a" { return 1 }
    if PlayBillingBuy("dew_100", "用户🚀", out[:]) != "buy:dew_100|用户🚀" { return 2 }
    if PlayBillingBuy("dew_100", "", out[:]) != "" { return 3 }
    if PlayBillingBuy("", "a", out[:]) != "" { return 4 }
    if PlayBillingBuy("dew|100", "a", out[:]) != "" { return 5 }
    if PlayBillingBuy("dew_100", "a|b", out[:]) != "" { return 6 }
    if PlayBillingBuy("dew_100", "a\\0b", out[:]) != "" { return 7 }
    if PlayBillingBuy("dew\\0_100", "a", out[:]) != "" { return 8 }
    large: [1160]u8
    for at: 0..large.count - 1 { large[at] = #char "a" }
    if PlayBillingBuy("dew_100", TextView(large[:]), out[:]) != "" { return 9 }
    if PlayBillingBuy("dew_100", TextView(large[:1157]), out[:]).count != 1169 { return 10 }
    if PlayBillingBuy("dew_100", TextView(large[:1158]), out[:]) != "" { return 11 }
    return 0
}
''')
    ziran = os.environ.get('ZIRAN_BIN', 'ziran')
    for target in ('c', 'go'):
        dest = WORK / target
        run(ziran, 'build', f'--target={target}', *(['--pkg', 'main'] if target == 'go' else []), '--exe', '--entry', 'billing_request:Main', '--root', WORK, '-o', dest, test)
        if target == 'go':
            run('go', 'run', *sorted(dest.glob('*.go')), env={**os.environ, 'GO111MODULE': 'off'})
        else:
            run(dest / 'billing_request')
        print(f'Play {target}: production request writer vectors passed', flush=True)
    # Include full JNI implementation in Android generation; compile every generated C file with the NDK.
    zroot = Path(subprocess.check_output(['ziran', 'pkg', 'path', 'ziran'], text=True).strip())
    ndks = sorted(sdk.glob('ndk/*/toolchains/llvm/prebuilt/linux-x86_64'))
    assert ndks, 'Android NDK required'
    for triple, arch in [('aarch64-linux-android24','__aarch64__'), ('armv7a-linux-androideabi24','__arm__'), ('i686-linux-android24','__i386__'), ('x86_64-linux-android24','__x86_64__')]:
        dest = WORK / arch
        run(ziran, 'build', '--target=c', '--no-main', '--define', 'ANDROID_BUILD', '--define', arch, '--root', ROOT / 'src/backend', '-o', dest, ROOT / 'src/backend/play_billing.zi')
        includes = [f'-I{zroot / "include"}'] + [f'-I{p}' for p in sorted({p.parent for p in dest.rglob('*.h')})]
        for source in dest.rglob('*.c'):
            run(ndks[-1] / 'bin/clang', f'--target={triple}', '-std=c11', '-fsyntax-only', *includes, source)
    print('Play Ziran: C/Go request vectors, bounds, malformed input, and full JNI C compilation for 4 Android ABIs passed')


if __name__ == '__main__':
    main()
