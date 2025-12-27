/**
 * Inspect Command
 * Inspect and analyze KIR (Kryon Intermediate Representation) files
 */

#include "kryon_cli.h"
#include "../../ir/ir_core.h"
#include "../../ir/ir_builder.h"
#include "../../ir/ir_serialization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// External IR functions
extern IRComponent* ir_read_json_v2_file(const char* filename);
extern char* ir_serialize_json_v2(IRComponent* root);
extern void ir_print_component_info(IRComponent* component, int depth);
extern bool ir_validate_component(IRComponent* component);
extern void ir_pool_free_component(IRComponent* component);

static void print_usage(void) {
    printf("Usage: kryon inspect [options] <file.kir>\n\n");
    printf("Options:\n");
    printf("  --tree, -t         Show component tree structure\n");
    printf("  --json, -j         Output as formatted JSON\n");
    printf("  --validate, -v     Validate IR structure\n");
    printf("  --stats, -s        Show statistics\n");
    printf("  --help, -h         Show this help\n\n");
    printf("Examples:\n");
    printf("  kryon inspect app.kir              # Basic inspection\n");
    printf("  kryon inspect --tree app.kir       # Show tree structure\n");
    printf("  kryon inspect --validate app.kir   # Validate IR\n");
}

static int count_components(IRComponent* component) {
    if (!component) return 0;

    int count = 1;
    for (uint32_t i = 0; i < component->child_count; i++) {
        count += count_components(component->children[i]);
    }
    return count;
}

static int get_max_depth(IRComponent* component, int current_depth) {
    if (!component) return current_depth;

    int max = current_depth;
    for (uint32_t i = 0; i < component->child_count; i++) {
        int child_depth = get_max_depth(component->children[i], current_depth + 1);
        if (child_depth > max) max = child_depth;
    }
    return max;
}

static void print_stats(IRComponent* root) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  KIR Statistics\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    int total_components = count_components(root);
    int max_depth = get_max_depth(root, 0);

    printf("  Total components: %d\n", total_components);
    printf("  Maximum depth:    %d\n", max_depth);
    printf("  Root type:        %s\n", ir_component_type_to_string(root->type));

    if (root->id > 0) {
        printf("  Root ID:          %u\n", root->id);
    }

    if (root->style) {
        printf("\n  Root styling:\n");
        if (root->style->width.value > 0) {
            printf("    Width:  %.0f (type %d)\n", root->style->width.value, root->style->width.type);
        }
        if (root->style->height.value > 0) {
            printf("    Height: %.0f (type %d)\n", root->style->height.value, root->style->height.type);
        }
    }

    printf("\n");
}

int cmd_inspect(int argc, char** argv) {
    bool show_tree = false;
    bool show_json = false;
    bool validate = false;
    bool show_stats = false;
    const char* file_path = NULL;

    // Parse arguments
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        } else if (strcmp(argv[i], "--tree") == 0 || strcmp(argv[i], "-t") == 0) {
            show_tree = true;
        } else if (strcmp(argv[i], "--json") == 0 || strcmp(argv[i], "-j") == 0) {
            show_json = true;
        } else if (strcmp(argv[i], "--validate") == 0 || strcmp(argv[i], "-v") == 0) {
            validate = true;
        } else if (strcmp(argv[i], "--stats") == 0 || strcmp(argv[i], "-s") == 0) {
            show_stats = true;
        } else if (argv[i][0] != '-') {
            file_path = argv[i];
        } else {
            fprintf(stderr, "Unknown option: %s\n\n", argv[i]);
            print_usage();
            return 1;
        }
    }

    // Check if file is provided
    if (!file_path) {
        fprintf(stderr, "Error: No file specified\n\n");
        print_usage();
        return 1;
    }

    // Check if file exists
    if (!file_exists(file_path)) {
        fprintf(stderr, "Error: File not found: %s\n", file_path);
        return 1;
    }

    // Load KIR file
    printf("Loading KIR file: %s\n", file_path);
    IRComponent* root = ir_read_json_v2_file(file_path);

    if (!root) {
        fprintf(stderr, "Error: Failed to load KIR file\n");
        return 1;
    }

    printf("✓ Successfully loaded KIR file\n");

    // Validate if requested
    if (validate) {
        printf("\nValidating IR structure...\n");
        bool is_valid = ir_validate_component(root);

        if (is_valid) {
            printf("✓ IR structure is valid\n");
        } else {
            fprintf(stderr, "✗ IR structure validation failed\n");
            ir_pool_free_component(root);
            return 1;
        }
    }

    // Show stats if requested or as default
    if (show_stats || (!show_tree && !show_json)) {
        print_stats(root);
    }

    // Show tree if requested
    if (show_tree) {
        printf("\n");
        printf("═══════════════════════════════════════════════════════════\n");
        printf("  Component Tree\n");
        printf("═══════════════════════════════════════════════════════════\n\n");
        ir_print_component_info(root, 0);
        printf("\n");
    }

    // Show JSON if requested
    if (show_json) {
        char* json = ir_serialize_json_v2(root);
        if (json) {
            printf("\n");
            printf("═══════════════════════════════════════════════════════════\n");
            printf("  JSON Output\n");
            printf("═══════════════════════════════════════════════════════════\n\n");
            printf("%s\n", json);
            free(json);
        } else {
            fprintf(stderr, "Error: Failed to serialize to JSON\n");
        }
    }

    ir_pool_free_component(root);
    return 0;
}
