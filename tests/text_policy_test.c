#include "runtime/text.h"
#include <assert.h>
#include <string.h>

int main(void)
{
    TextAppearance inherited = ResolveTextStyle(0, 27, 16, 0, 0x11223380,
        0xffffffff, true, false, false, false, -2);
    assert(inherited.font == 27 && inherited.color == 0x11223380);
    assert(inherited.letter_spacing == 0);
    assert(ResolveTextStyle(0, 27, 16, 0, 0x11223380, 0xffffffff,
        true, false, false, true, 0).color == 0x11223380);
    assert(ResolveTextStyle(0, 27, 16, 0, 0x11223380, 0xffffffff,
        true, false, true, true, 0).color == 0x11223380);
    assert(ResolveTextStyle(0, 27, 16, 0, 0, 0xffffff80,
        false, false, false, true, 0).color == 0xffffff39);
    TextAppearance explicit = ResolveTextStyle(19, 27, 16, 0x445566ff, 0x11223380,
        0xffffffff, true, true, true, true, 3);
    assert(explicit.font == 19 && explicit.color == 0x44556672);
    assert(explicit.letter_spacing == 3);
    assert(ResolveTextStyle(0, 0, 20, 0, 0, 0xabcdef80,
        false, false, false, false, 0).font == 20);
    assert(ResolveTextStyle(0, 0, 0, 0, 0, 0xabcdef80,
        false, false, false, false, 0).font == 16);
    assert(ResolveTextStyle(0, 27, 16, 0, 0x11223300, 0xffffffff,
        true, false, false, false, 0).color == 0x11223300);
    assert(ResolveTextStyle(0, 27, 16, 0, 0x11223300, 0xffffffff,
        false, false, false, false, 0).color == 0xffffffff);
    assert(ResolveTextStyle(0, 27, 16, 0, 0x112233ff, 0xffffffff,
        true, true, false, false, 0).color == 0);
    assert(TextExtent(24, 100) == 24 && TextExtent(0, 100) == 100);
    assert(TextExtent(-1, 0) == 0);
    assert(TextWrapPolicy(0, 0) == 1 && TextWrapPolicy(-1, 0) == 1);
    assert(TextWrapPolicy(100, 0) == 0 && TextWrapPolicy(100, 1) == 1);
    assert(TextDoubleClickSlopFor(2.0f) == 12);
    assert(TextDoubleClickSlopFor(0.0f) == 6);
    assert(TextDoubleClickShouldSelectLine(true, true, 1.0f, 1.30f,
                                           5.0f, -5.0f, 1.0f));
    assert(!TextDoubleClickShouldSelectLine(false, true, 1.0f, 1.30f,
                                            5.0f, -5.0f, 1.0f));
    assert(!TextDoubleClickShouldSelectLine(true, false, 1.0f, 1.30f,
                                            5.0f, -5.0f, 1.0f));
    assert(!TextDoubleClickShouldSelectLine(true, true, -1.0f, 1.30f,
                                            5.0f, -5.0f, 1.0f));
    assert(!TextDoubleClickShouldSelectLine(true, true, 1.0f, 1.50f,
                                            5.0f, -5.0f, 1.0f));
    assert(!TextDoubleClickShouldSelectLine(true, true, 1.0f, 1.30f,
                                            7.0f, -5.0f, 1.0f));
    assert(strcmp(TextControlBaselineSample(), "Hg") == 0);
    assert(TextControlClipGuardFor(0.0f) == 1);
    assert(TextControlClipGuardFor(2.0f) == 2);
    Rectangle clip = TextControlClipBounds((Rectangle){10, 20, 30, 40}, 2.0f);
    assert(clip.x == 10 && clip.y == 18 && clip.width == 30 && clip.height == 44);
    assert(TextCenteredY((Rectangle){10, 20, 30, 40}, 16) == 32);
    assert(TextCenteredY((Rectangle){10, 20, 30, 15}, 16) == 20);
    assert(TextCenteredX((Rectangle){10, 20, 30, 40}, 12) == 19);
    assert(TextCenteredX((Rectangle){10, 20, 11, 40}, 12) == 9);
    assert(TextBaselineYFor(10, 40, 16, false, 0.0f, 0.0f) == 22);
    assert(TextBaselineYFor(10, 40, 16, true, -3.0f, 17.0f) == 23);
    assert(TextHeightFor(16, false, 0.0f, 0.0f) == 16);
    assert(TextHeightFor(16, true, -3.0f, 17.0f) == 20);
    assert(TextLineHeightFor(13, 16, 2.0f) == 26);
    assert(TextLineHeightFor(0, 16, 1.5f) == 24);
    assert(TextLineHeightFor(13, 16, 0.0f) == 13);
    assert(TextSelectionLineEndPaddingFor(2.0f) == 12);
    assert(TextSelectionMinWidthFor(2.0f) == 8);
    assert(TextSelectionDefaultAlpha() == 88);
    assert(TextSelectionHighlightEndX(10, 20, true, 1.0f) == 26);
    assert(TextSelectionHighlightEndX(10, 10, false, 1.0f) == 14);
    assert(TextSelectionHighlightEndX(10, 2, true, 1.0f) == 14);
    assert(TextAlignmentOffset(100, 40, 0) == 0);
    assert(TextAlignmentOffset(100, 40, 1) == 30);
    assert(TextAlignmentOffset(100, 40, 2) == 60);
    assert(TextAlignmentOffset(20, 41, 1) == -10.5f);
    assert(TextAlignmentOffset(20, 41, 2) == -21);
    return 0;
}
