#!/bin/sh
# Writes one self-contained page that carries two Emscripten builds of a
# program: one compiled with -msimd128, whose std/resample Lanczos kernels
# use WebAssembly SIMD128, and a baseline build. At load time a small script
# asks WebAssembly.validate about a probe module holding one SIMD128
# instruction and starts the build the browser can run; the other is
# dropped unread. Each FILE@MOUNT or DIR@MOUNT is carried once for both and
# written to the module's file system before main.
#
# usage: web-page.sh SHELL SIMD128_JS BASELINE_JS [SOURCE@MOUNT ...] > PAGE
#
# SHELL is an Emscripten shell page; its {{{ SCRIPT }}} marker becomes the
# builds and the loader. Both builds are SINGLE_FILE JavaScript that export
# FS.
set -eu
if test $# -lt 3; then
    echo "usage: web-page.sh SHELL SIMD128_JS BASELINE_JS [SOURCE@MOUNT ...]" >&2
    exit 2
fi
shell=$1
simd128=$2
baseline=$3
shift 3
marker='{{{ SCRIPT }}}'
if ! grep -F -q "$marker" "$shell"; then
    echo "web-page: $shell has no $marker marker" >&2
    exit 1
fi
# The builds sit inside script elements, which end at the first </script.
for module in "$simd128" "$baseline"; do
    if LC_ALL=C grep -a -i -F -q '</script' "$module"; then
        echo "web-page: $module contains </script and cannot be inlined" >&2
        exit 1
    fi
done
mkdir -p build
manifest=$(mktemp build/kryon-web-page-manifest.XXXXXX)
trap 'rm -f "$manifest"' EXIT HUP INT TERM
tab=$(printf '\t')
for entry in "$@"; do
    case $entry in
        *@*) source=${entry%@*}; mount=${entry##*@} ;;
        *) echo "web-page: $entry is not SOURCE@MOUNT" >&2; exit 1 ;;
    esac
    case $mount in
        /*) ;;
        *) echo "web-page: $mount must be an absolute path" >&2; exit 1 ;;
    esac
    case $mount in
        *[!A-Za-z0-9._/-]*) echo "web-page: $mount may hold only letters, digits and . _ / -" >&2; exit 1 ;;
    esac
    if [ "$mount" != / ]; then
        mount=${mount%/}
    fi
    if [ -f "$source" ]; then
        printf '%s%s%s\n' "$source" "$tab" "$mount" >> "$manifest"
    elif [ -d "$source" ]; then
        source=${source%/}
        find "$source" -type f | LC_ALL=C sort | while IFS= read -r child || [ -n "$child" ]; do
            relative=${child#"$source"/}
            if [ "$mount" = / ]; then
                destination="/$relative"
            else
                destination="$mount/$relative"
            fi
            printf '%s%s%s\n' "$child" "$tab" "$destination" >> "$manifest"
        done
    else
        echo "web-page: $source is missing" >&2
        exit 1
    fi
done

awk -v marker="$marker" '{
    at = index($0, marker)
    if (at) { printf "%s", substr($0, 1, at - 1); exit }
    print
}' "$shell"
printf '<script type="text/plain" id="kryon-module-simd128">\n'
cat "$simd128"
printf '\n</script>\n<script type="text/plain" id="kryon-module-baseline">\n'
cat "$baseline"
printf '\n</script>\n'
seen_mounts=
while IFS="$tab" read -r source mount || [ -n "$source" ]; do
    case ",$seen_mounts," in
        *,"$mount",*) continue ;;
    esac
    seen_mounts="$seen_mounts,$mount"
    printf '<script type="text/plain" class="kryon-file" data-mount="%s">' "$mount"
    base64 < "$source" | tr -d '\n'
    printf '</script>\n'
done < "$manifest"
cat <<'LOADER'
<script>
(function () {
  // The smallest module that uses a SIMD128 instruction (i8x16.popcnt).
  var probe = new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 1, 5, 1, 96, 0, 1, 123, 3, 2, 1, 0,
    10, 10, 1, 8, 0, 65, 0, 253, 15, 253, 98, 11]);
  var simd128 = false;
  try { simd128 = typeof WebAssembly === "object" && WebAssembly.validate(probe); } catch (error) {}
  var name = simd128 ? "simd128" : "baseline";
  var text = document.getElementById("kryon-module-" + name).text;
  var files = [];
  document.querySelectorAll("script.kryon-file").forEach(function (element) {
    files.push([element.getAttribute("data-mount"), element.text]);
    element.remove();
  });
  document.querySelectorAll('script[id^="kryon-module-"]').forEach(function (element) {
    element.remove();
  });
  var module = window.Module || (window.Module = {});
  // Which build runs, for tools and tests.
  module.kryonBuild = name;
  if (files.length) {
    module.preRun = [].concat(module.preRun || [], function () {
      files.forEach(function (file) {
        var data = atob(file[1]), bytes = new Uint8Array(data.length);
        for (var index = 0; index < data.length; index++) bytes[index] = data.charCodeAt(index);
        var directory = file[0].replace(/\/[^/]+$/, "");
        if (directory && directory !== "/") {
          module.FS.createPath("/", directory.replace(/^\//, ""), true, true);
        }
        module.FS.writeFile(file[0], bytes);
      });
    });
  }
  // The build finds its own text through this id.
  var script = document.createElement("script");
  script.id = "mainScript";
  script.text = text;
  document.body.appendChild(script);
})();
</script>
LOADER
awk -v marker="$marker" '
    found { print; next }
    { at = index($0, marker); if (at) { found = 1; print substr($0, at + length(marker)) } }
' "$shell"
