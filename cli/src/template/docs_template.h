/**
 * Documentation Template System
 *
 * Supports HTML templates with placeholders ({{content}}, {{sidebar}})
 * for documentation pages with auto-generated navigation.
 */

#ifndef DOCS_TEMPLATE_H
#define DOCS_TEMPLATE_H

#include <stdbool.h>
#include <stdint.h>

// Forward declarations
struct IRComponent;

// Document entry for sidebar navigation
typedef struct {
    char* filename;     // Full path (e.g., "docs/getting-started.md")
    char* title;        // Extracted title (from H1 or frontmatter)
    char* route;        // URL route (e.g., "/docs/getting-started")
    int order;          // Sort order
} DocEntry;

// Placeholder location in template KIR
typedef struct {
    uint32_t component_id;  // ID of placeholder component
    char* name;             // Placeholder name (e.g., "content", "sidebar")
} PlaceholderRef;

// Template context (created once, reused for all docs)
typedef struct {
    struct IRComponent* template_kir;  // Parsed template as KIR tree
    PlaceholderRef* placeholders;      // Array of placeholder locations
    int placeholder_count;

    DocEntry* docs;                    // Array of document entries
    int doc_count;

    char* sidebar_title;               // Optional sidebar title
    char* template_path;               // Path to template file (for errors)
} DocsTemplateContext;

// ============================================================================
// Template Context Management
// ============================================================================

/**
 * Create template context from HTML file
 * Parses template, finds {{...}} placeholders, converts to KIR
 *
 * @param template_path Path to template HTML file
 * @return Template context or NULL on error
 */
DocsTemplateContext* docs_template_create(const char* template_path);

/**
 * Free template context and all associated resources
 */
void docs_template_free(DocsTemplateContext* ctx);

// ============================================================================
// Document Scanning
// ============================================================================

/**
 * Scan docs directory and extract titles from markdown files
 * Populates ctx->docs array with document entries
 *
 * @param ctx Template context
 * @param docs_dir Directory containing markdown files
 * @param base_path URL base path (e.g., "/docs")
 * @return true on success
 */
bool docs_template_scan_docs(DocsTemplateContext* ctx,
                             const char* docs_dir,
                             const char* base_path);

// ============================================================================
// Sidebar Generation
// ============================================================================

/**
 * Build sidebar KIR component with navigation links
 * Highlights the current page in the sidebar
 *
 * @param ctx Template context (must have docs populated)
 * @param current_route Current page route for active highlighting
 * @return Sidebar KIR component (caller owns)
 */
struct IRComponent* docs_template_build_sidebar(DocsTemplateContext* ctx,
                                                const char* current_route);

// ============================================================================
// Template Application
// ============================================================================

/**
 * Apply template to content - clone template and substitute placeholders
 *
 * @param ctx Template context
 * @param content_kir Content KIR to insert at {{content}}
 * @param current_route Current page route (for sidebar highlighting)
 * @return Combined KIR tree (caller owns), or NULL on error
 */
struct IRComponent* docs_template_apply(DocsTemplateContext* ctx,
                                        struct IRComponent* content_kir,
                                        const char* current_route);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Extract title from markdown file
 * Checks YAML frontmatter first, then H1 heading, then uses filename
 *
 * @param md_path Path to markdown file
 * @return Extracted title (caller must free) or NULL
 */
char* docs_extract_title(const char* md_path);

/**
 * Clone an IR component tree (deep copy)
 *
 * @param source Source component
 * @return Deep copy (caller owns) or NULL
 */
struct IRComponent* ir_component_clone(struct IRComponent* source);

#endif // DOCS_TEMPLATE_H
