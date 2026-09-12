#include <assert.h>

#include "runtime/drag_drop.h"

int
main(void)
{
    assert(DragDropShouldClearSource(true, 7, 7, false, false));
    assert(!DragDropShouldClearSource(true, 7, 8, false, false));
    assert(!DragDropShouldClearSource(true, 7, 7, true, false));
    assert(!DragDropSourceValid(true, false, true, 4, 1024, true));
    assert(!DragDropSourceValid(false, false, false, 4, 1024, true));
    assert(!DragDropSourceValid(false, false, true, -1, 1024, true));
    assert(!DragDropSourceValid(false, false, true, 4, 1024, false));
    assert(DragDropSourceValid(false, false, true, 0, 1024, false));
    assert(DragDropSourceStarts(true, true, true));
    assert(!DragDropSourceStarts(true, false, true));
    assert(DragDropSourceReturnsActive(true, 7, 7, false, true));
    assert(!DragDropSourceReturnsActive(true, 7, 8, false, true));

    assert(DragDropTargetMatches(true, true, true));
    assert(!DragDropTargetMatches(true, true, false));
    assert(DragDropTargetHot(false, false, true));
    assert(!DragDropTargetHot(true, false, true));
    assert(DragDropTargetAccepts(false, false, true, true, true));
    assert(!DragDropTargetAccepts(false, false, true, true, false));
    assert(DragDropCopySize(4, 8) == 4);
    assert(DragDropCopySize(8, 4) == 4);
    assert(DragDropCopySize(-1, 4) == 0);
    assert(DragDropCopySize(4, -1) == 0);
    return 0;
}
