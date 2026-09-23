#include "kryon_portable_host.h"

#include <string.h>

static int
slice_string(void *context, const char *module, const char *function,
             const VmHostValue *args, int arg_count, VmHostValue *result)
{
    (void)context;
    if(!((strcmp(module, "kss_parser") == 0 &&
          strcmp(function, "StringSlice") == 0) ||
         (strcmp(module, "text_widget") == 0 &&
          strcmp(function, "TextSlice") == 0)) || arg_count != 3 ||
       args[0].kind != VM_HOST_STRING ||
       args[0].type == NULL ||
       strcmp(args[0].type, "string") != 0 ||
       args[1].kind != VM_HOST_INTEGER ||
       args[1].type == NULL ||
       strcmp(args[1].type, "i32") != 0 ||
       args[2].kind != VM_HOST_INTEGER ||
       args[2].type == NULL ||
       strcmp(args[2].type, "i32") != 0 ||
       args[1].integer < 0 || args[2].integer < 0 ||
       (uint64_t)args[1].integer > args[0].length ||
       (uint64_t)args[2].integer >
           args[0].length - (size_t)args[1].integer ||
       (args[0].data == NULL && args[0].length != 0))
        return 0;
    result->kind = VM_HOST_STRING;
    result->type = "string";
    result->data = args[0].data == NULL ?
        (const unsigned char *)"" :
        args[0].data + (size_t)args[1].integer;
    result->length = (size_t)args[2].integer;
    return 1;
}

HostBinding
KssStringSliceBinding(void)
{
    return (HostBinding){"kss_parser", "StringSlice", slice_string, NULL};
}

HostBinding
TextSliceBinding(void)
{
    return (HostBinding){"text_widget", "TextSlice", slice_string, NULL};
}
