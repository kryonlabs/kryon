# Shared dependency paths for standalone backend tests. The ordinary package
# resolver/artifact path is exercised separately by image_integration.sh.
image_avif=${KRYON_AVIF_PACKAGE_DIR:-$("$ziran" pkg path avif)}
image_webp=$("$ziran" pkg path webp)
image_codec=$("$ziran" pkg path image_codec)
image_ziran() {
    image_compile_command=$1
    shift
    "$ziran" "$image_compile_command" --module-path "avif=$image_avif/src" --module-path "$image_avif/src" \
        --module-path "$image_avif/src/bridge" --module-path "webp=$image_webp/src" \
        --module-path "image_codec=$image_codec/src" --module-path "std=$ziran_root/std" \
        --module-path "$ziran_root/std" "$@"
}
