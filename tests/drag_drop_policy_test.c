#include <assert.h>

#include "runtime/drag_drop.h"

int
main(void)
{
    DragDropSourceDecision source;
    DragDropTargetDecision target;

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

    source = DragDropSourceDecisionFor(false, 0, 7, false, false, true,
                                       4, 1024, true, true, true, true,
                                       false);
    assert(!source.clear_source);
    assert(source.valid);
    assert(source.start_source);
    assert(source.returns_active);
    source = DragDropSourceDecisionFor(true, 8, 7, false, false, true,
                                       4, 1024, true, true, false, true,
                                       false);
    assert(source.valid);
    assert(!source.start_source);
    assert(!source.returns_active);
    source = DragDropSourceDecisionFor(true, 7, 7, false, false, true,
                                       4, 1024, true, false, false, false,
                                       false);
    assert(source.clear_source);
    assert(!source.returns_active);
    source = DragDropSourceDecisionFor(false, 0, 7, false, false, true,
                                       2048, 1024, true, true, true, true,
                                       false);
    assert(!source.valid);
    assert(!source.start_source);

    target = DragDropTargetDecisionFor(false, false, true, true, true, true,
                                       true, 8, 4);
    assert(target.accepted);
    assert(target.copy_size == 4);
    assert(target.clear_source);
    assert(target.consume_release);
    target = DragDropTargetDecisionFor(false, false, true, true, false, true,
                                       true, 8, 4);
    assert(!target.accepted);
    assert(!target.clear_source);
    assert(!target.consume_release);
    return 0;
}
