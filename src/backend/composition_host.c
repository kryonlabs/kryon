#include "kryon_portable_host.h"

#include <stdlib.h>
#include <string.h>

#define COMPOSITION_QUEUE_CAP 16
#define COMPOSITION_TEXT_CAP 256

typedef struct RawCompositionEvent {
    int phase;
    int cursor;
    int selection_length;
    size_t length;
    char text[COMPOSITION_TEXT_CAP];
} RawCompositionEvent;

typedef struct PinnedCompositionEvent {
    RawCompositionEvent event;
    VmHostField fields[5];
} PinnedCompositionEvent;

struct CompositionQueue {
    RawCompositionEvent pending[COMPOSITION_QUEUE_CAP];
    PinnedCompositionEvent pinned[COMPOSITION_QUEUE_CAP];
    VmHostField empty_fields[5];
    size_t head;
    size_t count;
    size_t pinned_count;
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
        queue->pinned_count = 0;
}

static size_t
trim_incomplete_utf8(const char *text, size_t length)
{
    size_t start;
    unsigned char lead;
    size_t expected = 1;
    if(length == 0)
        return 0;
    start = length - 1;
    while(start > 0 &&
          (((unsigned char)text[start] & 0xc0) == 0x80))
        start--;
    lead = (unsigned char)text[start];
    if((lead & 0xe0) == 0xc0)
        expected = 2;
    else if((lead & 0xf0) == 0xe0)
        expected = 3;
    else if((lead & 0xf8) == 0xf0)
        expected = 4;
    if(start + expected > length)
        return start;
    return length;
}

int
CompositionQueueSubmit(CompositionQueue *queue, int phase,
                       const char *text, int cursor,
                       int selection_length)
{
    RawCompositionEvent *event;
    size_t tail;
    size_t length = 0;
    if(queue == NULL || phase < 1 || phase > 4 ||
       queue->count >= COMPOSITION_QUEUE_CAP)
        return 0;
    tail = (queue->head + queue->count) % COMPOSITION_QUEUE_CAP;
    event = &queue->pending[tail];
    memset(event, 0, sizeof(*event));
    event->phase = phase;
    event->cursor = cursor > 0 ? cursor : 0;
    event->selection_length = selection_length > 0 ? selection_length : 0;
    if(text != NULL) {
        while(length + 1 < COMPOSITION_TEXT_CAP && text[length] != '\0')
            length++;
        memcpy(event->text, text, length);
        length = trim_incomplete_utf8(event->text, length);
        event->text[length] = '\0';
    }
    event->length = length;
    queue->count++;
    return 1;
}

int
CompositionQueueTake(CompositionQueue *queue,
                     CompositionInputEvent *event)
{
    PinnedCompositionEvent *pinned;
    if(event == NULL)
        return 0;
    *event = (CompositionInputEvent){0, 0, "", 0, 0, 0};
    if(queue == NULL || queue->count == 0 ||
       queue->pinned_count >= COMPOSITION_QUEUE_CAP)
        return 0;
    pinned = &queue->pinned[queue->pinned_count++];
    pinned->event = queue->pending[queue->head];
    queue->head = (queue->head + 1) % COMPOSITION_QUEUE_CAP;
    queue->count--;
    event->available = 1;
    event->phase = pinned->event.phase;
    event->text = pinned->event.text;
    event->length = pinned->event.length;
    event->cursor = pinned->event.cursor;
    event->selection_length = pinned->event.selection_length;
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
        fields = queue->pinned[queue->pinned_count - 1].fields;
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
