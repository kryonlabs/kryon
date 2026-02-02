/**
 * @file codegen_options.c
 * @brief Unified Codegen Options Implementation
 */

#define _POSIX_C_SOURCE 200809L

#include "codegen_options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Base Options
 * ============================================================================ */

void codegen_options_init_base(CodegenOptionsBase* opts) {
    if (!opts) return;

    opts->include_comments = true;
    opts->verbose = false;
    opts->indent_size = 4;
    opts->indent_string = NULL;
}

bool codegen_options_parse_base(CodegenOptionsBase* opts, int argc, char** argv) {
    if (!opts) return false;

    for (int i = 0; i < argc; i++) {
        const char* arg = argv[i];

        if (strcmp(arg, "--comments") == 0) {
            opts->include_comments = true;
        } else if (strcmp(arg, "--no-comments") == 0) {
            opts->include_comments = false;
        } else if (strcmp(arg, "--verbose") == 0) {
            opts->verbose = true;
        } else if (strncmp(arg, "--indent=", 9) == 0) {
            const char* value = arg + 9;
            if (strcmp(value, "tabs") == 0) {
                opts->indent_string = strdup("\t");
                opts->indent_size = 1;
            } else if (strcmp(value, "spaces") == 0) {
                if (opts->indent_string) free(opts->indent_string);
                opts->indent_string = NULL;  // Use default spaces
            } else {
                // Parse number
                int size = atoi(value);
                if (size > 0) {
                    opts->indent_size = size;
                }
            }
        } else {
            // Unknown option
            fprintf(stderr, "Warning: Unknown codegen option: %s\n", arg);
            return false;
        }
    }

    return true;
}

void codegen_options_free_base(CodegenOptionsBase* opts) {
    if (!opts) return;

    if (opts->indent_string) {
        free(opts->indent_string);
        opts->indent_string = NULL;
    }
}

/* ============================================================================
 * C Codegen Options
 * ============================================================================ */

void codegen_options_init_c(CCodegenOptions* opts) {
    if (!opts) return;

    codegen_options_init_base(&opts->base);
    opts->generate_types = true;
    opts->include_headers = true;
    opts->generate_main = false;
}

void codegen_options_free_c(CCodegenOptions* opts) {
    if (!opts) return;

    codegen_options_free_base(&opts->base);
}

/* ============================================================================
 * Limbo Codegen Options
 * ============================================================================ */

void codegen_options_init_limbo(LimboCodegenOptions* opts) {
    if (!opts) return;

    codegen_options_init_base(&opts->base);
    opts->generate_types = true;
    opts->module_name = NULL;
}

void codegen_options_free_limbo(LimboCodegenOptions* opts) {
    if (!opts) return;

    if (opts->module_name) {
        free(opts->module_name);
        opts->module_name = NULL;
    }

    codegen_options_free_base(&opts->base);
}

/* ============================================================================
 * Lua Codegen Options
 * ============================================================================ */

void codegen_options_init_lua(LuaCodegenOptions* opts) {
    if (!opts) return;

    codegen_options_init_base(&opts->base);
    opts->format = false;
    opts->optimize = false;
}

void codegen_options_free_lua(LuaCodegenOptions* opts) {
    if (!opts) return;

    codegen_options_free_base(&opts->base);
}

/* ============================================================================
 * Markdown Codegen Options
 * ============================================================================ */

void codegen_options_init_markdown(MarkdownCodegenOptions* opts) {
    if (!opts) return;

    codegen_options_init_base(&opts->base);
    opts->include_toc = false;
    opts->include_metadata = false;
}

void codegen_options_free_markdown(MarkdownCodegenOptions* opts) {
    if (!opts) return;

    codegen_options_free_base(&opts->base);
}

/* ============================================================================
 * Tcl Codegen Options
 * ============================================================================ */

void codegen_options_init_tcl(TclCodegenOptions* opts) {
    if (!opts) return;

    codegen_options_init_base(&opts->base);
    opts->use_tk_namespace = false;
    opts->pack_geometry = true;
}

void codegen_options_free_tcl(TclCodegenOptions* opts) {
    if (!opts) return;

    codegen_options_free_base(&opts->base);
}
