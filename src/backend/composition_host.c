#include "kryon_portable_host.h"
#include "composition_queue.h"

#include <stdlib.h>
#include <string.h>

struct CompositionQueue {
    CompositionQueueState state;
    VmHostField pinned_fields[16][5];
    VmHostField empty_fields[5];
};

CompositionQueue *
CompositionQueueCreate(void)
{
    return calloc(1, sizeof(CompositionQueue));
}

void
CompositionQueueDestroy(CompositionQueue *queue)
{
    free(queue);
}

void
CompositionQueueBeginFrame(CompositionQueue *queue)
{
    if(queue != NULL)
        BeginCompositionFrame(&queue->state);
}

int
CompositionQueueSubmit(CompositionQueue *queue, int phase,
                       const char *text, int cursor,
                       int selection_length)
{
    size_t length = 0;
    if(queue == NULL)
        return 0;
    if(text == NULL)
        text = "";
    while(length < 255 && text[length] != '\0')
        length++;
    return SubmitComposition(&queue->state, phase,
                             StringView(text, length),
                             cursor, selection_length);
}

int
CompositionQueueTake(CompositionQueue *queue,
                     CompositionInputEvent *event)
{
    int slot;
    QueuedComposition *pinned;
    if(event == NULL)
        return 0;
    *event = (CompositionInputEvent){0, 0, "", 0, 0, 0};
    if(queue == NULL)
        return 0;
    slot = TakeComposition(&queue->state);
    if(slot < 0)
        return 0;
    pinned = &queue->state.pinned[slot];
    event->available = 1;
    event->phase = pinned->phase;
    event->text = (const char *)pinned->text;
    event->length = (size_t)pinned->length;
    event->cursor = pinned->cursor;
    event->selection_length = pinned->selection_length;
    return 1;
}

static void
sample_fields(VmHostField fields[5],
              const CompositionInputEvent *event)
{
    fields[0] = (VmHostField){"available",
        {.kind = VM_HOST_INTEGER, .type = "bool",
         .integer = event->available}};
    fields[1] = (VmHostField){"phase",
        {.kind = VM_HOST_INTEGER, .type = "CompositionPhase",
         .integer = event->phase}};
    fields[2] = (VmHostField){"text",
        {.kind = VM_HOST_STRING, .type = "string",
         .data = (const unsigned char *)event->text,
         .length = event->length}};
    fields[3] = (VmHostField){"cursor",
        {.kind = VM_HOST_INTEGER, .type = "i32",
         .integer = event->cursor}};
    fields[4] = (VmHostField){"selection_length",
        {.kind = VM_HOST_INTEGER, .type = "i32",
         .integer = event->selection_length}};
}

static int
poll_composition(void *context, const char *module,
                 const char *function, const VmHostValue *args,
                 int arg_count, VmHostValue *result)
{
    CompositionQueue *queue = context;
    CompositionInputEvent event;
    VmHostField *fields;
    (void)args;
    if(queue == NULL || strcmp(module, "composition_input") != 0 ||
       strcmp(function, "PollComposition") != 0 ||
       arg_count != 0)
        return 0;
    if(CompositionQueueTake(queue, &event))
        fields = queue->pinned_fields[queue->state.pinned_count - 1];
    else
        fields = queue->empty_fields;
    sample_fields(fields, &event);
    result->kind = VM_HOST_RECORD;
    result->type = "CompositionSample";
    result->fields = fields;
    result->field_count = 5;
    return 1;
}

HostBinding
PollCompositionBinding(CompositionQueue *queue)
{
    return (HostBinding){"composition_input", "PollComposition",
                         poll_composition, queue};
}
