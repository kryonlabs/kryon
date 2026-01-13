/**
 * Documentation Template System Implementation
 */

#include "docs_template.h"
#include "../../../ir/ir_core.h"
#include "../../../ir/ir_builder.h"
#include "../../include/kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

// External function to parse HTML to KIR (from html_parser)
extern char* ir_html_file_to_kir(const char* filepath);
extern struct IRComponent* ir_deserialize_json(const char* json);
extern char* ir_serialize_json(struct IRComponent* root);

// ============================================================================
// Forward declarations
// ============================================================================

static void find_placeholders_recursive(struct IRComponent* comp,
                                        DocsTemplateContext* ctx);
static struct IRComponent* clone_component_recursive(struct IRComponent* source,
                                                     uint32_t* next_id);
static bool replace_placeholder(struct IRComponent* root,
                               uint32_t placeholder_id,
                               struct IRComponent* replacement);
static bool replace_placeholder_by_name(struct IRComponent* root,
                                        const char* name,
                                        struct IRComponent* replacement);

// ============================================================================
// Template Context Management
// ============================================================================

DocsTemplateContext* docs_template_create(const char* template_path) {
    if (!template_path) return NULL;

    // Check if file exists
    if (!file_exists(template_path)) {
        fprintf(stderr, "Template file not found: %s\n", template_path);
        return NULL;
    }

    // Allocate context
    DocsTemplateContext* ctx = calloc(1, sizeof(DocsTemplateContext));
    if (!ctx) return NULL;

    ctx->template_path = str_copy(template_path);

    // Parse HTML template to KIR JSON
    char* kir_json = ir_html_file_to_kir(template_path);
    if (!kir_json) {
        fprintf(stderr, "Failed to parse template HTML: %s\n", template_path);
        docs_template_free(ctx);
        return NULL;
    }

    // Deserialize KIR JSON to component tree
    ctx->template_kir = ir_deserialize_json(kir_json);
    free(kir_json);

    if (!ctx->template_kir) {
        fprintf(stderr, "Failed to deserialize template KIR: %s\n", template_path);
        docs_template_free(ctx);
        return NULL;
    }

    // Find all {{...}} placeholders in the template
    ctx->placeholders = NULL;
    ctx->placeholder_count = 0;
    find_placeholders_recursive(ctx->template_kir, ctx);

    fprintf(stderr, "Template loaded: %s (%d placeholders found)\n",
            template_path, ctx->placeholder_count);

    return ctx;
}

void docs_template_free(DocsTemplateContext* ctx) {
    if (!ctx) return;

    // Free template KIR
    if (ctx->template_kir) {
        // TODO: ir_destroy_component(ctx->template_kir);
    }

    // Free placeholders
    if (ctx->placeholders) {
        for (int i = 0; i < ctx->placeholder_count; i++) {
            free(ctx->placeholders[i].name);
        }
        free(ctx->placeholders);
    }

    // Free doc entries
    if (ctx->docs) {
        for (int i = 0; i < ctx->doc_count; i++) {
            free(ctx->docs[i].filename);
            free(ctx->docs[i].title);
            free(ctx->docs[i].route);
        }
        free(ctx->docs);
    }

    free(ctx->sidebar_title);
    free(ctx->template_path);
    free(ctx);
}

// ============================================================================
// Placeholder Detection
// ============================================================================

/**
 * Check if text contains {{name}} pattern and extract name
 */
static char* extract_placeholder_name(const char* text) {
    if (!text) return NULL;

    const char* start = strstr(text, "{{");
    if (!start) return NULL;

    const char* end = strstr(start + 2, "}}");
    if (!end) return NULL;

    // Extract name between {{ and }}
    size_t name_len = end - (start + 2);
    if (name_len == 0 || name_len > 64) return NULL;

    char* name = malloc(name_len + 1);
    if (!name) return NULL;

    // Copy and trim whitespace
    const char* src = start + 2;
    char* dst = name;
    while (src < end) {
        if (!isspace(*src)) {
            *dst++ = *src;
        }
        src++;
    }
    *dst = '\0';

    if (strlen(name) == 0) {
        free(name);
        return NULL;
    }

    return name;
}

/**
 * Convert text node with {{name}} to placeholder component
 */
static void convert_to_placeholder(struct IRComponent* comp, const char* name) {
    if (!comp || !name) return;

    // Change type to placeholder
    comp->type = IR_COMPONENT_PLACEHOLDER;

    // Create placeholder data
    IRPlaceholderData* ph_data = calloc(1, sizeof(IRPlaceholderData));
    if (ph_data) {
        ph_data->name = str_copy(name);
        ph_data->preserve = false;  // Will be expanded during build
    }
    comp->custom_data = (char*)ph_data;

    // Clear text content
    free(comp->text_content);
    comp->text_content = NULL;
}

/**
 * Recursively find and convert {{...}} placeholders
 */
static void find_placeholders_recursive(struct IRComponent* comp,
                                        DocsTemplateContext* ctx) {
    if (!comp || !ctx) return;

    // Check text content for placeholder pattern
    if (comp->text_content) {
        char* name = extract_placeholder_name(comp->text_content);
        if (name) {
            // Add to placeholder list
            ctx->placeholder_count++;
            ctx->placeholders = realloc(ctx->placeholders,
                                       ctx->placeholder_count * sizeof(PlaceholderRef));
            PlaceholderRef* ref = &ctx->placeholders[ctx->placeholder_count - 1];
            ref->component_id = comp->id;
            ref->name = str_copy(name);

            // Convert component to placeholder type
            convert_to_placeholder(comp, name);
            free(name);
        }
    }

    // Recurse into children
    for (uint32_t i = 0; i < comp->child_count; i++) {
        find_placeholders_recursive(comp->children[i], ctx);
    }
}

// ============================================================================
// Document Scanning
// ============================================================================

char* docs_extract_title(const char* md_path) {
    char* content = file_read(md_path);
    if (!content) return NULL;

    char* title = NULL;

    // Check for YAML frontmatter: ---\n...\n---
    if (strncmp(content, "---", 3) == 0) {
        char* fm_end = strstr(content + 3, "\n---");
        if (fm_end) {
            // Look for title: in frontmatter
            char* title_key = strstr(content, "title:");
            if (title_key && title_key < fm_end) {
                char* title_start = title_key + 6;
                // Skip whitespace and quotes
                while (*title_start == ' ' || *title_start == '"' || *title_start == '\'')
                    title_start++;

                char* title_end = title_start;
                while (*title_end && *title_end != '\n' &&
                       *title_end != '"' && *title_end != '\'')
                    title_end++;

                size_t len = title_end - title_start;
                if (len > 0) {
                    title = malloc(len + 1);
                    memcpy(title, title_start, len);
                    title[len] = '\0';
                }
            }
        }
    }

    // Fallback: extract from first H1 heading (# ...)
    if (!title) {
        char* h1 = NULL;
        if (content[0] == '#' && content[1] == ' ') {
            h1 = content + 2;
        } else {
            h1 = strstr(content, "\n# ");
            if (h1) h1 += 3;
        }

        if (h1) {
            char* h1_end = strchr(h1, '\n');
            if (!h1_end) h1_end = h1 + strlen(h1);
            size_t len = h1_end - h1;
            if (len > 0) {
                title = malloc(len + 1);
                memcpy(title, h1, len);
                title[len] = '\0';
            }
        }
    }

    // Fallback: use filename
    if (!title) {
        const char* slash = strrchr(md_path, '/');
        const char* basename = slash ? slash + 1 : md_path;
        char* dot = strrchr(basename, '.');
        size_t len = dot ? (size_t)(dot - basename) : strlen(basename);

        title = malloc(len + 1);
        memcpy(title, basename, len);
        title[len] = '\0';

        // Convert dashes to spaces and capitalize first letter
        for (size_t i = 0; i < len; i++) {
            if (title[i] == '-') title[i] = ' ';
        }
        if (len > 0 && title[0] >= 'a' && title[0] <= 'z') {
            title[0] -= 32;
        }
    }

    free(content);
    return title;
}

bool docs_template_scan_docs(DocsTemplateContext* ctx,
                             const char* docs_dir,
                             const char* base_path) {
    if (!ctx || !docs_dir) return false;

    // List markdown files
    char** md_files = NULL;
    int md_count = 0;

    if (dir_list_files(docs_dir, ".md", &md_files, &md_count) != 0 || md_count == 0) {
        return false;
    }

    // Allocate doc entries
    ctx->docs = calloc(md_count, sizeof(DocEntry));
    ctx->doc_count = md_count;

    for (int i = 0; i < md_count; i++) {
        DocEntry* entry = &ctx->docs[i];
        entry->filename = str_copy(md_files[i]);
        entry->title = docs_extract_title(md_files[i]);

        // Extract basename without extension
        const char* slash = strrchr(md_files[i], '/');
        const char* basename = slash ? slash + 1 : md_files[i];
        char name[256];
        strncpy(name, basename, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        char* dot = strrchr(name, '.');
        if (dot) *dot = '\0';

        // Build route
        char route[512];
        snprintf(route, sizeof(route), "%s/%s",
                 base_path ? base_path : "/docs", name);
        entry->route = str_copy(route);
        entry->order = i;

        free(md_files[i]);
    }
    free(md_files);

    fprintf(stderr, "Scanned %d docs from %s\n", ctx->doc_count, docs_dir);
    return true;
}

// ============================================================================
// Sidebar Generation
// ============================================================================

/**
 * Get category for a doc entry based on its filename
 */
static const char* get_doc_category(const char* filename) {
    if (!filename) return "Other";

    // Extract basename from path (e.g., "docs/getting-started.md" -> "getting-started.md")
    const char* slash = strrchr(filename, '/');
    const char* basename = slash ? slash + 1 : filename;

    // Getting Started
    if (strcmp(basename, "getting-started.md") == 0) {
        return "Getting Started";
    }

    // Language Bindings
    if (strcmp(basename, "typescript.md") == 0 ||
        strcmp(basename, "lua-bindings.md") == 0 ||
        strcmp(basename, "nim-bindings.md") == 0 ||
        strcmp(basename, "js-bindings.md") == 0 ||
        strcmp(basename, "c-frontend.md") == 0) {
        return "Language Bindings";
    }

    // Core Concepts
    if (strcmp(basename, "architecture.md") == 0 ||
        strcmp(basename, "ir-pipeline.md") == 0 ||
        strcmp(basename, "kry-format.md") == 0 ||
        strcmp(basename, "codegens.md") == 0 ||
        strcmp(basename, "targets.md") == 0 ||
        strcmp(basename, "plugins.md") == 0) {
        return "Core Concepts";
    }

    // Reference
    if (strcmp(basename, "cli-reference.md") == 0) {
        return "Reference";
    }

    // Advanced
    if (strcmp(basename, "developer-guide.md") == 0 ||
        strcmp(basename, "examples.md") == 0 ||
        strcmp(basename, "testing.md") == 0) {
        return "Advanced";
    }

    return "Other";
}

struct IRComponent* docs_template_build_sidebar(DocsTemplateContext* ctx,
                                                const char* current_route) {
    if (!ctx || ctx->doc_count == 0) return NULL;

    // Create a simple div wrapper
    struct IRComponent* wrapper = ir_create_component(IR_COMPONENT_CONTAINER);
    if (!wrapper) return NULL;

    // Mark as element selector type to prevent auto-assignment of "container" class
    wrapper->selector_type = IR_SELECTOR_ELEMENT;
    wrapper->css_class = str_copy("docs-sidebar-content");

    // Add title if specified
    if (ctx->sidebar_title && strlen(ctx->sidebar_title) > 0) {
        struct IRComponent* title = ir_heading(3, ctx->sidebar_title);
        if (title) {
            title->css_class = str_copy("docs-sidebar-title");
            ir_add_child(wrapper, title);
        }
    }

    // Define category order
    const char* category_order[] = {
        "Getting Started",
        "Language Bindings",
        "Core Concepts",
        "Reference",
        "Advanced",
        NULL
    };

    // Add docs by category in order
    for (int cat_idx = 0; category_order[cat_idx] != NULL; cat_idx++) {
        const char* category = category_order[cat_idx];
        bool has_items = false;

        // First pass: check if this category has any items
        for (int i = 0; i < ctx->doc_count; i++) {
            DocEntry* doc = &ctx->docs[i];
            const char* doc_category = get_doc_category(doc->filename);
            if (strcmp(doc_category, category) == 0) {
                has_items = true;
                break;
            }
        }

        if (!has_items) continue;

        // Add category heading
        struct IRComponent* heading = ir_heading(4, category);
        if (heading) {
            heading->css_class = str_copy("docs-sidebar-category");
            ir_add_child(wrapper, heading);
        }

        // Second pass: add all docs in this category
        for (int i = 0; i < ctx->doc_count; i++) {
            DocEntry* doc = &ctx->docs[i];
            const char* doc_category = get_doc_category(doc->filename);

            if (strcmp(doc_category, category) != 0) continue;

            // Create link (without list item wrapper - no bullets)
            struct IRComponent* link = ir_create_component(IR_COMPONENT_LINK);
            if (!link) continue;

            // Set link data
            IRLinkData* link_data = calloc(1, sizeof(IRLinkData));
            if (link_data) {
                link_data->url = str_copy(doc->route);
                link->custom_data = (char*)link_data;
            }

            // Set link text
            link->text_content = str_copy(doc->title ? doc->title : doc->route);

            // Highlight current page
            bool is_current = current_route && strcmp(doc->route, current_route) == 0;
            if (is_current) {
                link->css_class = str_copy("docs-sidebar-link active");
            } else {
                link->css_class = str_copy("docs-sidebar-link");
            }

            ir_add_child(wrapper, link);
        }
    }

    return wrapper;
}

// ============================================================================
// Component Cloning
// ============================================================================

struct IRComponent* ir_component_clone(struct IRComponent* source) {
    if (!source) return NULL;

    uint32_t next_id = 1;
    return clone_component_recursive(source, &next_id);
}

static struct IRComponent* clone_component_recursive(struct IRComponent* source,
                                                     uint32_t* next_id) {
    if (!source) return NULL;

    struct IRComponent* clone = ir_create_component(source->type);
    if (!clone) return NULL;

    clone->id = (*next_id)++;

    // Copy basic fields
    if (source->tag) clone->tag = str_copy(source->tag);
    if (source->css_class) clone->css_class = str_copy(source->css_class);
    if (source->text_content) clone->text_content = str_copy(source->text_content);
    clone->selector_type = source->selector_type;

    // Clone style (deep copy important fields, null out complex pointers)
    if (source->style) {
        clone->style = calloc(1, sizeof(IRStyle));
        if (clone->style) {
            // Copy scalar fields
            clone->style->width = source->style->width;
            clone->style->height = source->style->height;
            clone->style->background = source->style->background;
            clone->style->text_fill_color = source->style->text_fill_color;
            clone->style->border_color = source->style->border_color;
            clone->style->border = source->style->border;
            clone->style->margin = source->style->margin;
            clone->style->padding = source->style->padding;
            clone->style->font = source->style->font;
            clone->style->transform = source->style->transform;
            clone->style->z_index = source->style->z_index;
            clone->style->visible = source->style->visible;
            clone->style->opacity = source->style->opacity;
            clone->style->position_mode = source->style->position_mode;
            clone->style->absolute_x = source->style->absolute_x;
            clone->style->absolute_y = source->style->absolute_y;
            clone->style->overflow_x = source->style->overflow_x;
            clone->style->overflow_y = source->style->overflow_y;
            clone->style->object_fit = source->style->object_fit;
            clone->style->grid_item = source->style->grid_item;
            clone->style->text_effect = source->style->text_effect;
            clone->style->box_shadow = source->style->box_shadow;
            clone->style->background_clip = source->style->background_clip;
            clone->style->container_type = source->style->container_type;
            clone->style->current_pseudo_states = source->style->current_pseudo_states;

            // Copy filter array (not pointers, just values)
            clone->style->filter_count = source->style->filter_count;
            memcpy(clone->style->filters, source->style->filters, sizeof(source->style->filters));

            // Deep copy string fields
            if (source->style->font.family) {
                clone->style->font.family = str_copy(source->style->font.family);
            }
            if (source->style->background_image) {
                clone->style->background_image = str_copy(source->style->background_image);
            }
            if (source->style->container_name) {
                clone->style->container_name = str_copy(source->style->container_name);
            }

            // Deep copy color var_name fields (pointers within struct)
            if (source->style->background.var_name) {
                clone->style->background.var_name = str_copy(source->style->background.var_name);
            }
            if (source->style->text_fill_color.var_name) {
                clone->style->text_fill_color.var_name = str_copy(source->style->text_fill_color.var_name);
            }
            if (source->style->border_color.var_name) {
                clone->style->border_color.var_name = str_copy(source->style->border_color.var_name);
            }
            if (source->style->font.color.var_name) {
                clone->style->font.color.var_name = str_copy(source->style->font.color.var_name);
            }

            // Null out complex pointers (animations, transitions) - not needed for templates
            clone->style->animations = NULL;
            clone->style->animation_count = 0;
            clone->style->transitions = NULL;
            clone->style->transition_count = 0;

            // Null out breakpoints and pseudo styles (keep simple for now)
            clone->style->breakpoint_count = 0;
            clone->style->pseudo_style_count = 0;
        }
    }

    // Clone layout
    if (source->layout) {
        clone->layout = malloc(sizeof(IRLayout));
        if (clone->layout) {
            memcpy(clone->layout, source->layout, sizeof(IRLayout));
        }
    }

    // Clone custom_data based on type
    if (source->custom_data) {
        switch (source->type) {
            case IR_COMPONENT_PLACEHOLDER: {
                IRPlaceholderData* src_ph = (IRPlaceholderData*)source->custom_data;
                IRPlaceholderData* dst_ph = calloc(1, sizeof(IRPlaceholderData));
                if (dst_ph && src_ph->name) {
                    dst_ph->name = str_copy(src_ph->name);
                    dst_ph->preserve = src_ph->preserve;
                }
                clone->custom_data = (char*)dst_ph;
                break;
            }
            case IR_COMPONENT_HEADING: {
                IRHeadingData* src = (IRHeadingData*)source->custom_data;
                IRHeadingData* dst = calloc(1, sizeof(IRHeadingData));
                if (dst) {
                    dst->level = src->level;
                    if (src->text) dst->text = str_copy(src->text);
                    if (src->id) dst->id = str_copy(src->id);
                }
                clone->custom_data = (char*)dst;
                break;
            }
            case IR_COMPONENT_LINK: {
                IRLinkData* src = (IRLinkData*)source->custom_data;
                IRLinkData* dst = calloc(1, sizeof(IRLinkData));
                if (dst) {
                    if (src->url) dst->url = str_copy(src->url);
                    if (src->title) dst->title = str_copy(src->title);
                    if (src->target) dst->target = str_copy(src->target);
                    if (src->rel) dst->rel = str_copy(src->rel);
                }
                clone->custom_data = (char*)dst;
                break;
            }
            case IR_COMPONENT_LIST: {
                IRListData* src = (IRListData*)source->custom_data;
                IRListData* dst = calloc(1, sizeof(IRListData));
                if (dst) {
                    memcpy(dst, src, sizeof(IRListData));
                }
                clone->custom_data = (char*)dst;
                break;
            }
            case IR_COMPONENT_LIST_ITEM: {
                IRListItemData* src = (IRListItemData*)source->custom_data;
                IRListItemData* dst = calloc(1, sizeof(IRListItemData));
                if (dst) {
                    dst->number = src->number;
                    dst->is_task_item = src->is_task_item;
                    dst->is_checked = src->is_checked;
                    if (src->marker) dst->marker = str_copy(src->marker);
                }
                clone->custom_data = (char*)dst;
                break;
            }
            case IR_COMPONENT_CODE_BLOCK: {
                IRCodeBlockData* src = (IRCodeBlockData*)source->custom_data;
                IRCodeBlockData* dst = calloc(1, sizeof(IRCodeBlockData));
                if (dst) {
                    if (src->language) dst->language = str_copy(src->language);
                    if (src->code) dst->code = str_copy(src->code);
                    dst->length = src->length;
                    dst->show_line_numbers = src->show_line_numbers;
                    dst->start_line = src->start_line;
                    // Syntax highlighting handled by plugins, no tokens to clone
                }
                clone->custom_data = (char*)dst;
                break;
            }
            case IR_COMPONENT_BLOCKQUOTE: {
                IRBlockquoteData* src = (IRBlockquoteData*)source->custom_data;
                IRBlockquoteData* dst = calloc(1, sizeof(IRBlockquoteData));
                if (dst) {
                    dst->depth = src->depth;
                }
                clone->custom_data = (char*)dst;
                break;
            }
            case IR_COMPONENT_IMAGE: {
                // For images, custom_data is the src path (string)
                clone->custom_data = str_copy(source->custom_data);
                break;
            }
            default:
                // For unknown types, set to NULL to avoid crashes
                // The serializer will handle missing custom_data
                clone->custom_data = NULL;
                break;
        }
    }

    // Null out complex pointers that shouldn't be copied for templates
    clone->events = NULL;
    clone->logic = NULL;
    clone->layout_state = NULL;
    clone->text_layout = NULL;
    clone->tab_data = NULL;
    clone->property_bindings = NULL;
    clone->property_binding_count = 0;

    // Clone children recursively
    for (uint32_t i = 0; i < source->child_count; i++) {
        struct IRComponent* child_clone = clone_component_recursive(source->children[i], next_id);
        if (child_clone) {
            ir_add_child(clone, child_clone);
        }
    }

    return clone;
}

// ============================================================================
// Template Application
// ============================================================================

/**
 * Find and replace a placeholder component with replacement content
 */
__attribute__((unused))
static bool replace_placeholder(struct IRComponent* root,
                               uint32_t placeholder_id,
                               struct IRComponent* replacement) {
    if (!root || !replacement) return false;

    for (uint32_t i = 0; i < root->child_count; i++) {
        struct IRComponent* child = root->children[i];

        if (child->id == placeholder_id) {
            // Found placeholder - replace it
            // Free the old placeholder component
            // TODO: ir_destroy_component(child);

            // Insert replacement
            root->children[i] = replacement;
            replacement->parent = root;
            return true;
        }

        // Recurse into children
        if (replace_placeholder(child, placeholder_id, replacement)) {
            return true;
        }
    }

    return false;
}

struct IRComponent* docs_template_apply(DocsTemplateContext* ctx,
                                        struct IRComponent* content_kir,
                                        const char* current_route) {
    if (!ctx || !ctx->template_kir) return NULL;

    // Clone the template
    struct IRComponent* result = ir_component_clone(ctx->template_kir);
    if (!result) return NULL;

    // Find and replace placeholders
    for (int i = 0; i < ctx->placeholder_count; i++) {
        PlaceholderRef* ref = &ctx->placeholders[i];

        if (strcmp(ref->name, "content") == 0 && content_kir) {
            // Clone content and insert at {{content}}
            struct IRComponent* content_clone = ir_component_clone(content_kir);
            if (content_clone) {
                // Find the placeholder in the cloned tree
                // Note: IDs might differ after cloning, need to find by type+name
                // For now, walk tree looking for placeholder with matching name
                // This is a simplified approach - could be optimized
                replace_placeholder_by_name(result, "content", content_clone);
            }
        } else if (strcmp(ref->name, "sidebar") == 0) {
            // Build and insert sidebar
            struct IRComponent* sidebar = docs_template_build_sidebar(ctx, current_route);
            if (sidebar) {
                replace_placeholder_by_name(result, "sidebar", sidebar);
            }
        }
    }

    return result;
}

/**
 * Find placeholder by name and replace it
 */
static bool replace_placeholder_by_name(struct IRComponent* root,
                                        const char* name,
                                        struct IRComponent* replacement) {
    if (!root || !name || !replacement) return false;

    // Check if this is the placeholder we're looking for
    if (root->type == IR_COMPONENT_PLACEHOLDER) {
        IRPlaceholderData* ph = (IRPlaceholderData*)root->custom_data;
        if (ph && ph->name && strcmp(ph->name, name) == 0) {
            // Found it - but we can't replace root directly
            // This case shouldn't happen if placeholder is always a child
            return false;
        }
    }

    // Check children
    for (uint32_t i = 0; i < root->child_count; i++) {
        struct IRComponent* child = root->children[i];

        if (child->type == IR_COMPONENT_PLACEHOLDER) {
            IRPlaceholderData* ph = (IRPlaceholderData*)child->custom_data;
            if (ph && ph->name && strcmp(ph->name, name) == 0) {
                // Found placeholder - replace it
                root->children[i] = replacement;
                replacement->parent = root;
                // TODO: free old child
                return true;
            }
        }

        // Recurse
        if (replace_placeholder_by_name(child, name, replacement)) {
            return true;
        }
    }

    return false;
}
