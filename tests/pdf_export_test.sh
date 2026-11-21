#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$repo"
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0 HARMONY_DESKTOP_RECOVERY=0
out="$repo/build/scratch/pdf-export"
mkdir -p "$out"
python3 - "$out/rgba.png" <<'PY'
from PIL import Image
import sys
image = Image.new("RGBA", (2, 1))
image.putdata([(255, 0, 0, 255), (0, 0, 255, 128)])
image.save(sys.argv[1])
PY
"$ziran" ir --project --module-path "$repo/tests" --root "$repo/tests" \
    --define PLATFORM_DESKTOP -o "$out/ir" tests/pdf_export_test.zi
for form in source saved; do
    if [ "$form" = source ]; then
        "$ziran" build --project --module-path "$repo/tests" --root "$repo/tests" \
            --define PLATFORM_DESKTOP --entry pdf_export_test:main --target=c --exe \
            -o "$out/$form" tests/pdf_export_test.zi
    else
        LDLIBS='-lSDL2 -lpangocairo-1.0 -lpango-1.0 -lcairo -lfontconfig -lfreetype -lgio-2.0 -lgobject-2.0 -lglib-2.0 -lX11 -lm -lpthread -ldl' \
            "$ziran" build --root "$out/ir" --module-path "$out/ir" \
            --entry pdf_export_test:main --target=c --exe -o "$out/$form" "$out/ir/pdf_export_test.zir"
    fi
    "$out/$form/pdf_export_test"
    mv "$out/api.pdf" "$out/api-$form.pdf"
    pdftoppm -r 72 -png "$out/api-$form.pdf" "$out/$form-page" >/dev/null
    pdftotext -bbox "$out/api-$form.pdf" "$out/$form-text.html"
    python3 tests/pdf_export_verify.py "$out" "$form"
done
