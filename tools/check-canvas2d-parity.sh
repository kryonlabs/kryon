#!/bin/sh
# Check that the Canvas2D backend owns the 2D/browser symbols expected to be
# real implementations instead of generated weak null fallback stubs.
set -eu

root=${1:-.}

if ! command -v rg >/dev/null 2>&1; then
    echo "canvas2d parity check: rg not found" >&2
    exit 1
fi

symbols='
IsWindowReady
IsWindowFullscreen
IsWindowFocused
IsWindowResized
IsWindowState
SetWindowState
ClearWindowState
ToggleFullscreen
SetWindowTitle
SetWindowSize
GetScreenWidth
GetScreenHeight
GetRenderWidth
GetRenderHeight
GetWindowScaleDPI
SetMouseCursor
SetExitKey
WaitTime
BackendRaw_IsKeyPressed
BackendRaw_IsKeyPressedRepeat
BackendRaw_IsKeyDown
BackendRaw_IsKeyReleased
BackendRaw_GetKeyPressed
BackendRaw_GetCharPressed
BackendRaw_IsMouseButtonPressed
BackendRaw_IsMouseButtonDown
BackendRaw_IsMouseButtonReleased
BackendRaw_IsMouseButtonUp
BackendRaw_GetMousePosition
BackendRaw_GetMouseDelta
BackendRaw_GetMouseWheelMove
BackendRaw_GetMouseWheelMoveV
SetMousePosition
GetTouchPosition
GetTouchPointCount
IsGamepadAvailable
GetGamepadAxisMovement
ClearBackground
DrawPixel
DrawLine
DrawLineV
DrawLineEx
DrawLineStrip
DrawRectangle
DrawRectangleV
DrawRectangleRec
DrawRectanglePro
DrawRectangleGradientV
DrawRectangleGradientH
DrawRectangleGradientEx
DrawRectangleLines
DrawRectangleLinesEx
DrawRectangleRounded
DrawRectangleRoundedLines
DrawRectangleRoundedLinesEx
DrawCircle
DrawCircleV
DrawCircleLines
DrawCircleLinesV
DrawCircleLinesEx
DrawRing
DrawRingLines
DrawTriangle
DrawTriangleLines
DrawTriangleFan
DrawTriangleStrip
BeginScissorMode
EndScissorMode
BeginMode2D
EndMode2D
LoadImage
LoadImageFromMemory
LoadImageFromTexture
IsImageValid
UnloadImage
ImageFlipVertical
LoadTexture
LoadTextureFromImage
IsTextureValid
UnloadTexture
LoadRenderTexture
IsRenderTextureValid
BeginTextureMode
EndTextureMode
DrawTexture
DrawTextureV
DrawTextureEx
DrawTextureRec
DrawTexturePro
LoadImageFromScreen
ExportImage
GetFontDefault
LoadFont
LoadFontEx
LoadFontFromMemory
IsFontValid
UnloadFont
DrawText
DrawTextEx
DrawTextPro
DrawTextCodepoint
DrawTextCodepoints
MeasureText
MeasureTextEx
MeasureTextCodepoints
GetGlyphInfo
GetGlyphAtlasRec
GetClipboardText
SetClipboardText
FileExists
DirectoryExists
MakeDirectory
LoadFileData
SaveFileData
LoadDroppedFiles
IsFileDropped
UnloadDroppedFiles
'

missing=0
for sym in $symbols; do
    if [ "$sym" = "LoadImageFromScreen" ]; then
        if ! rg -q "kry_backend_capture_screen[[:space:]]*\\(" "$root/src/backend/canvas_texture.c"; then
            echo "canvas2d parity check: missing capture hook for $sym" >&2
            missing=1
        fi
        continue
    fi
    if ! rg -q "^[A-Za-z_][A-Za-z0-9_[:space:]\\*]*[[:space:]\\*]$sym[[:space:]]*\\(" "$root/src/backend/canvas_"*.c; then
        echo "canvas2d parity check: missing Canvas2D implementation for $sym" >&2
        missing=1
    fi
done

if [ "$missing" -ne 0 ]; then
    exit 1
fi

echo "canvas2d parity check ok"
