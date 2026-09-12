#!/bin/sh
set -eu

root="${1:-.}"

cd "$root"

fail=0
surface_paths="include src tests examples go cmd docs/API.md docs/PUBLIC_API_SNAPSHOT.txt"
public_api_paths="include docs/PUBLIC_API_SNAPSHOT.txt"

check_absent() {
    label="$1"
    pattern="$2"
    shift 2
    matches=""

    matches="$(rg -n "$pattern" "$@" | rg -v '^tests/canonical_surface_test\.sh:' || true)"
    if [ -n "$matches" ]; then
        printf '%s\n' "$matches"
        echo "canonical-surface: stale $label" >&2
        fail=1
    fi
}

check_missing_path() {
    path="$1"

    if [ -e "$path" ]; then
        echo "$path"
        echo "canonical-surface: stale deleted path" >&2
        fail=1
    fi
}

python3 tests/canonical_widget_surface_doc_test.py

# Names already migrated during the canonical surface cleanup. Keep this list
# focused on completed migrations so it stays a fast regression guard instead
# of a noisy audit of future work.
check_absent "public UI prefixes" \
    '\bUI[A-Z][A-Za-z0-9_]*\b|\bUI_[A-Z0-9_]+' \
    $public_api_paths

check_absent "retained tree event names" \
    '\bUIEvent\b|\bUIEventKind\b|\bUI_EVENT_' \
    $surface_paths

check_absent "retained tree invalidation names" \
    '\bUIInvalidation\b|\bUI_INVALIDATE_' \
    $surface_paths

check_absent "retained tree widget names" \
    '\bUIWidget\b|\bUIWidgetKind\b|\bUIWidgetNode\b|\bUIWidgetData\b|\bUIWidgetTextInputPaint\b|\bBeginUIWidget\b|\bEndUIWidget\b|\bUIWidgetSet[A-Za-z0-9_]*\b|\bUI_WIDGET_[A-Z0-9_]+' \
    $surface_paths

check_absent "swipe/pager legacy names" \
    '\bUISwipe|\bUI_SWIPE_|\bUIGuideStep|\bUIGuidePager|\bUIGuideOverlayDebug\b|include/ui_pager\.h' \
    $surface_paths

check_absent "icon legacy names" \
    '\bUIIconType\b|\bUI_ICON_TYPE_|\bUIIconSheet\b|\bUI_ICON_SHEET_|\bGetUIIconAsset\b|\bLoadUIIconTexture\b|\bLoadAllUIIconTextures\b|\bUnloadAllUIIconTextures\b|\bUIIconSize\b|\bUI_ICON_SIZE_' \
    $surface_paths

check_absent "inspect legacy names" \
    '\bUIInspect|\bBeginUIInspect|\bEndUIInspect|\bSetUIInspect|\bPushUIInspect|\bPopUIInspect|\bIsUIInspectActive\b' \
    $surface_paths

check_absent "accessibility legacy names" \
    '\bUIAccessibilityNode\b' \
    $surface_paths

check_absent "semantic kind legacy names" \
    '\bUISemanticKind\b' \
    $surface_paths web/kryon-runtime.js web/kryon-runtime.d.ts

check_absent "DPI legacy names" \
    '\bUIDPIState\b|\bui_dpi_state\b|\bUI_DPI_BASE_|\bInitUIDPI\b|\bFixUIDPIFramebufferColor\b|\bInvalidateUIDPI\b|\bSetUIDeviceDensity\b|\bUpdateUIDPI\b|\bIsUIDPIDirty\b|\bGetUIDPI' \
    $surface_paths

check_absent "profile image legacy names" \
    '\bProfilePicture\b|\bprofile_picture\b|\bprofile picture\b|\bUISyncProfileIcon\b|\bUI_SYNC_PROFILE_ICON_|\bGetUIProfilePicture|\bGetUISyncIDForProfilePicture|\bDrawProfilePicture' \
    $surface_paths

check_absent "text layout legacy names" \
    '\bUI_TEXT_ELEMENT_|\bTextElementType\b' \
    $surface_paths

check_absent "core frame/focus/input legacy names" \
    '\bUIFrameState\b|\bInitUI\b|\bSetUIDefaultFontAutoLoad\b|\bSetUILinkColor\b|\bIsUIDesktopMode\b|\bGetUIDefaultCamera\b|\bBeginUIFrame\b|\bEndUIFrame\b|\bResolveUIFocusID\b|\bSetUIFrame\b|\bSaveUIFrameState\b|\bRestoreUIFrameState\b|\bSetUIMouseWorldOverride\b|\bSetUIKeyboardInputEnabled\b|\bUIKeyboardInputEnabled\b|\bClearUIInputCaptures\b|\bPushUIInputCapture\b|\bBeginUIModalLayer\b|\bPushUIInputClip\b|\bPopUIInputClip\b|\bSetUIModalCapture\b|\bSetUIIcons\b|\bUIHandle[A-Za-z0-9_]*\b|\bUIInputCapturesClick\b|\bUIReleaseConsumed\b|\bUIConsumeRelease\b|\bUIPointerRelease[A-Za-z0-9_]*\b|\bUIConsumePointerRelease\b|\bUIHoverEffectsEnabled\b|\bBeginUIFocus\b|\bEndUIFocus\b|\bUIFocusFrameOpen\b|\bRegisterUIFocus\b|\bIsUIFocus[A-Za-z0-9_]*\b|\bSetUIFocus[A-Za-z0-9_]*\b|\bGetUIFocus\b|\bClearUIFocus\b' \
    $surface_paths

check_absent "text font legacy names" \
    '\bUIFont[A-Za-z0-9_]*\b|\bUIFonts\b|\bUI_FONT_|\bRegisterUISmallFont\b|\bRegisterUIFixedFontSource\b' \
    $surface_paths

check_absent "clipboard legacy names" \
    '\bUIClipboard[A-Za-z0-9_]*\b|\bUI_CLIPBOARD_|\bSetUIClipboard[A-Za-z0-9_]*\b|\bGetUIClipboard[A-Za-z0-9_]*\b|\bCopyUISelectionTextToClipboard\b|\bRequestUIClipboard[A-Za-z0-9_]*\b|\bHandleUIClipboard[A-Za-z0-9_]*\b|\bWriteUIClipboard[A-Za-z0-9_]*\b|\bInitUIClipboard[A-Za-z0-9_]*\b|\bSyncUIClipboard[A-Za-z0-9_]*\b|\bFlushUIClipboard[A-Za-z0-9_]*\b|\bSetUIPrimarySelection[A-Za-z0-9_]*\b|\bGetUIPrimarySelection[A-Za-z0-9_]*\b' \
    $surface_paths

check_absent "layout legacy names" \
    '\bSetUIViewSize\b|\bGetUIViewWidth\b|\bGetUIViewHeight\b|\bGetUICenteredColumn\b|\bGetUIPageSidePadding\b' \
    $surface_paths web/kryon-runtime.js web/kryon-runtime.d.ts

check_absent "scaling legacy names" \
    '\bSetUIScale\b|\bGetUIScale\b|\bClampUIPx\b' \
    $surface_paths web/kryon-runtime.js web/kryon-runtime.d.ts

check_absent "color legacy names" \
    '\bUIColor(Min|Max|Byte|ToHSL|HueToRGB|FromHSL)\b|\bClampUIColorFloat\b|\bAdjustUIColorLightness\b|\bLightenUIColor\b|\bDarkenUIColor\b' \
    $surface_paths web/kryon-runtime.js web/kryon-runtime.d.ts

check_absent "clip legacy names" \
    '\bBeginUIClip\b|\bEndUIClip\b|\bResetUIClip\b|\bGetUIClipEffective\b|\bGetUIClipIntersection\b|\bUIClipState\b|\bUI_CLIP_' \
    $surface_paths web/kryon-runtime.js web/kryon-runtime.d.ts

check_absent "typed numeric helper legacy names" \
    '\bUI(Float|Int|Double|Angle)(Drag|Slider|Input)[A-Za-z0-9_]*\b|\bDragFloats\b|\bDragInts\b|\bSliderFloats\b|\bSliderInts\b|\bInputInts\b|\bInputFloats\b|\bdrag_floats\b|\bdrag_ints\b|\bslider_floats\b|\bslider_ints\b|\binput_floats\b|\binput_ints\b|\binput_doubles\b' \
    $surface_paths web/kryon-runtime.js web/kryon-runtime.d.ts

check_absent "button variant legacy names" \
    '\b(MenuButton|SplitButton|InfoButton|ArrowButton)\b' \
    include src cmd go web docs examples tests tools scripts \
    --glob '!docs/CANONICAL_WIDGET_SURFACE.md' \
    --glob '!tests/public_api_names_test.sh' \
    --glob '!tests/canonical_surface_test.sh'

check_absent "tab scope compatibility names" \
    '\bBeginTabBar\b|\bBeginTabItem\b|\bEndTabItem\b|\bEndTabBar\b' \
    $surface_paths web/kryon-runtime.js web/kryon-runtime.d.ts

check_absent "lightfield test harness" \
    'lightfield-(capture|go|label|geometry|test|reference|motion|backend|translation)|lightfield_.*test|Lightfield verification|KRYON_LIGHTFIELD|lightfield_js_geometry|design/lightfield-baseline|tests/lightfield' \
    Makefile tests/*.sh tests/*.c tests/*.py tests/*.mjs go docs examples scripts

for deleted in \
    design/lightfield-baseline/README.md \
    design/lightfield-baseline/dark.png \
    design/lightfield-baseline/light.png \
    design/lightfield-baseline/split.png \
    go/kryon/lightfield_typography_fit_test.go \
    tests/lightfield_backend_test.py \
    tests/lightfield_capture.c \
    tests/lightfield_go_capture_test.go \
    tests/lightfield_js_geometry_test.mjs \
    tests/lightfield_motion_test.py \
    tests/lightfield_reference_test.py \
    tests/lightfield_translation_test.py
do
    check_missing_path "$deleted"
done

exit "$fail"
