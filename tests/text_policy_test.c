#include "runtime/text.h"
#include <assert.h>

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
    assert(TextAlignmentOffset(100, 40, 0) == 0);
    assert(TextAlignmentOffset(100, 40, 1) == 30);
    assert(TextAlignmentOffset(100, 40, 2) == 60);
    assert(TextAlignmentOffset(20, 41, 1) == -10.5f);
    assert(TextAlignmentOffset(20, 41, 2) == -21);
    return 0;
}
