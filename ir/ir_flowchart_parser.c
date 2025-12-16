#define _POSIX_C_SOURCE 200809L
#include "ir_flowchart_parser.h"
#include "ir_serialization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

// Parser state
typedef struct {
    const char* source;
    size_t length;
    size_t pos;
    int line;
    int col;
    IRFlowchartDirection direction;
    IRComponent* flowchart;
    IRFlowchartState* state;

    // Current subgraph (NULL if not in subgraph)
    IRComponent* current_subgraph;
} FlowchartParser;

// Forward declarations
static void skip_whitespace(FlowchartParser* p);
static void skip_whitespace_and_newlines(FlowchartParser* p);
static bool is_at_end(FlowchartParser* p);
static char peek(FlowchartParser* p);
static char peek_next(FlowchartParser* p);
static char advance(FlowchartParser* p);
static bool match(FlowchartParser* p, const char* str);
static bool match_case_insensitive(FlowchartParser* p, const char* str);
static char* parse_identifier(FlowchartParser* p);
static char* parse_text_in_brackets(FlowchartParser* p, char open, char close);
static char* parse_quoted_string(FlowchartParser* p);
static void parse_node_definition(FlowchartParser* p, const char* node_id);
static void parse_edge(FlowchartParser* p, const char* from_id);
static void parse_subgraph(FlowchartParser* p);
static void parse_style(FlowchartParser* p);
static void parse_line(FlowchartParser* p);
static uint32_t parse_hex_color(const char* str);
static bool node_exists_in_children(IRComponent* parent, const char* node_id);

// Utility functions
static void skip_whitespace(FlowchartParser* p) {
    while (!is_at_end(p) && (peek(p) == ' ' || peek(p) == '\t')) {
        advance(p);
    }
}

static void skip_whitespace_and_newlines(FlowchartParser* p) {
    while (!is_at_end(p)) {
        char c = peek(p);
        if (c == ' ' || c == '\t') {
            advance(p);
        } else if (c == '\n') {
            advance(p);
            p->line++;
            p->col = 0;
        } else if (c == '\r') {
            advance(p);
            if (!is_at_end(p) && peek(p) == '\n') advance(p);
            p->line++;
            p->col = 0;
        } else if (c == '%' && peek_next(p) == '%') {
            // Skip comment (%%...)
            while (!is_at_end(p) && peek(p) != '\n') advance(p);
        } else {
            break;
        }
    }
}

static bool is_at_end(FlowchartParser* p) {
    return p->pos >= p->length;
}

static char peek(FlowchartParser* p) {
    if (is_at_end(p)) return '\0';
    return p->source[p->pos];
}

static char peek_next(FlowchartParser* p) {
    if (p->pos + 1 >= p->length) return '\0';
    return p->source[p->pos + 1];
}

static char advance(FlowchartParser* p) {
    if (is_at_end(p)) return '\0';
    p->col++;
    return p->source[p->pos++];
}

static bool match(FlowchartParser* p, const char* str) {
    size_t len = strlen(str);
    if (p->pos + len > p->length) return false;
    if (strncmp(p->source + p->pos, str, len) == 0) {
        for (size_t i = 0; i < len; i++) {
            advance(p);
        }
        return true;
    }
    return false;
}

static bool match_case_insensitive(FlowchartParser* p, const char* str) {
    size_t len = strlen(str);
    if (p->pos + len > p->length) return false;
    if (strncasecmp(p->source + p->pos, str, len) == 0) {
        for (size_t i = 0; i < len; i++) {
            advance(p);
        }
        return true;
    }
    return false;
}

static char* parse_identifier(FlowchartParser* p) {
    skip_whitespace(p);
    size_t start = p->pos;

    // First char must be letter or underscore
    if (!is_at_end(p) && (isalpha(peek(p)) || peek(p) == '_')) {
        advance(p);
        // Rest can be alphanumeric or underscore
        while (!is_at_end(p) && (isalnum(peek(p)) || peek(p) == '_')) {
            advance(p);
        }
    }

    if (p->pos == start) return NULL;

    size_t len = p->pos - start;
    char* id = (char*)malloc(len + 1);
    memcpy(id, p->source + start, len);
    id[len] = '\0';
    return id;
}

static char* parse_text_in_brackets(FlowchartParser* p, char open, char close) {
    if (peek(p) != open) return NULL;
    advance(p);  // Skip opening bracket

    size_t start = p->pos;
    int depth = 1;

    while (!is_at_end(p) && depth > 0) {
        char c = peek(p);
        if (c == open) depth++;
        else if (c == close) depth--;
        if (depth > 0) advance(p);
    }

    size_t len = p->pos - start;
    char* text = (char*)malloc(len + 1);
    memcpy(text, p->source + start, len);
    text[len] = '\0';

    if (peek(p) == close) advance(p);  // Skip closing bracket

    // Trim whitespace
    while (len > 0 && isspace(text[len - 1])) text[--len] = '\0';
    char* trimmed = text;
    while (*trimmed && isspace(*trimmed)) trimmed++;
    if (trimmed != text) {
        memmove(text, trimmed, strlen(trimmed) + 1);
    }

    return text;
}

static char* parse_quoted_string(FlowchartParser* p) {
    if (peek(p) != '"') return NULL;
    advance(p);  // Skip opening quote

    size_t start = p->pos;
    while (!is_at_end(p) && peek(p) != '"') {
        if (peek(p) == '\\' && peek_next(p) == '"') {
            advance(p);  // Skip backslash
        }
        advance(p);
    }

    size_t len = p->pos - start;
    char* str = (char*)malloc(len + 1);
    memcpy(str, p->source + start, len);
    str[len] = '\0';

    if (peek(p) == '"') advance(p);  // Skip closing quote

    return str;
}

// Check if a node with given ID already exists as a child of the parent component
static bool node_exists_in_children(IRComponent* parent, const char* node_id) {
    if (!parent || !node_id) return false;

    for (uint32_t i = 0; i < parent->child_count; i++) {
        IRComponent* child = parent->children[i];
        if (child && child->type == IR_COMPONENT_FLOWCHART_NODE) {
            IRFlowchartNodeData* data = ir_get_flowchart_node_data(child);
            if (data && data->node_id && strcmp(data->node_id, node_id) == 0) {
                return true;
            }
        }
        // Also check subgraph children
        if (child && child->type == IR_COMPONENT_FLOWCHART_SUBGRAPH) {
            if (node_exists_in_children(child, node_id)) {
                return true;
            }
        }
    }
    return false;
}

static uint32_t parse_hex_color(const char* str) {
    if (!str || str[0] != '#') return 0xE0E0E0FF;  // Default gray

    unsigned int r = 0, g = 0, b = 0, a = 255;
    size_t len = strlen(str);

    if (len == 4) {  // #RGB
        sscanf(str, "#%1x%1x%1x", &r, &g, &b);
        r = r * 17; g = g * 17; b = b * 17;
    } else if (len == 7) {  // #RRGGBB
        sscanf(str, "#%2x%2x%2x", &r, &g, &b);
    } else if (len == 9) {  // #RRGGBBAA
        sscanf(str, "#%2x%2x%2x%2x", &r, &g, &b, &a);
    }

    return (r << 24) | (g << 16) | (b << 8) | a;
}

// Parse node shape from syntax like A[text], A{text}, A((text)), etc.
static void parse_node_definition(FlowchartParser* p, const char* node_id) {
    skip_whitespace(p);

    IRFlowchartShape shape = IR_FLOWCHART_SHAPE_RECTANGLE;
    char* label = NULL;

    char c = peek(p);
    char c2 = peek_next(p);

    if (c == '[') {
        if (c2 == '[') {  // [[subroutine]]
            advance(p);
            label = parse_text_in_brackets(p, '[', ']');
            if (peek(p) == ']') advance(p);  // Extra ]
            shape = IR_FLOWCHART_SHAPE_SUBROUTINE;
        } else if (c2 == '(') {  // [(database)]
            advance(p);
            label = parse_text_in_brackets(p, '(', ')');
            if (peek(p) == ']') advance(p);
            shape = IR_FLOWCHART_SHAPE_CYLINDER;
        } else if (c2 == '/') {  // [/parallelogram/] or [/trapezoid\]
            advance(p); advance(p);  // Skip [/
            size_t start = p->pos;
            while (!is_at_end(p) && peek(p) != '/' && peek(p) != '\\') {
                advance(p);
            }
            size_t len = p->pos - start;
            label = (char*)malloc(len + 1);
            memcpy(label, p->source + start, len);
            label[len] = '\0';

            if (peek(p) == '/') {
                shape = IR_FLOWCHART_SHAPE_PARALLELOGRAM;
                advance(p);
            } else if (peek(p) == '\\') {
                shape = IR_FLOWCHART_SHAPE_TRAPEZOID;
                advance(p);
            }
            if (peek(p) == ']') advance(p);
        } else {  // [rectangle]
            label = parse_text_in_brackets(p, '[', ']');
            shape = IR_FLOWCHART_SHAPE_RECTANGLE;
        }
    } else if (c == '(') {
        if (c2 == '(') {  // ((circle))
            advance(p);
            label = parse_text_in_brackets(p, '(', ')');
            if (peek(p) == ')') advance(p);
            shape = IR_FLOWCHART_SHAPE_CIRCLE;
        } else if (c2 == '[') {  // ([stadium])
            advance(p);
            label = parse_text_in_brackets(p, '[', ']');
            if (peek(p) == ')') advance(p);
            shape = IR_FLOWCHART_SHAPE_STADIUM;
        } else {  // (rounded)
            label = parse_text_in_brackets(p, '(', ')');
            shape = IR_FLOWCHART_SHAPE_ROUNDED;
        }
    } else if (c == '{') {
        if (c2 == '{') {  // {{hexagon}}
            advance(p);
            label = parse_text_in_brackets(p, '{', '}');
            if (peek(p) == '}') advance(p);
            shape = IR_FLOWCHART_SHAPE_HEXAGON;
        } else {  // {diamond}
            label = parse_text_in_brackets(p, '{', '}');
            shape = IR_FLOWCHART_SHAPE_DIAMOND;
        }
    } else if (c == '>') {  // >asymmetric]
        advance(p);  // Skip >
        size_t start = p->pos;
        while (!is_at_end(p) && peek(p) != ']') {
            advance(p);
        }
        size_t len = p->pos - start;
        label = (char*)malloc(len + 1);
        memcpy(label, p->source + start, len);
        label[len] = '\0';
        if (peek(p) == ']') advance(p);
        shape = IR_FLOWCHART_SHAPE_ASYMMETRIC;
    }

    // If no label parsed, use node_id as label
    if (!label) {
        label = strdup(node_id);
    }

    // Check if node already exists - skip creating duplicate
    bool already_exists = node_exists_in_children(p->flowchart, node_id);
    if (p->current_subgraph && !already_exists) {
        already_exists = node_exists_in_children(p->current_subgraph, node_id);
    }

    if (!already_exists) {
        // Create the node component
        IRComponent* node = ir_flowchart_node(node_id, shape, label);

        if (node) {
            // Add to current subgraph or flowchart
            if (p->current_subgraph) {
                ir_add_child(p->current_subgraph, node);
            } else {
                ir_add_child(p->flowchart, node);
            }
        }
    }

    free(label);
}

// Parse edge: A --> B, A -->|label| B, A -- label --> B
static void parse_edge(FlowchartParser* p, const char* from_id) {
    skip_whitespace(p);

    IRFlowchartEdgeType type = IR_FLOWCHART_EDGE_ARROW;
    char* label = NULL;

    // Determine edge type
    if (match(p, "<-->")) {
        type = IR_FLOWCHART_EDGE_BIDIRECTIONAL;
    } else if (match(p, "-.->") || match(p, "-..->")) {
        type = IR_FLOWCHART_EDGE_DOTTED;
    } else if (match(p, "==>")) {
        type = IR_FLOWCHART_EDGE_THICK;
    } else if (match(p, "-->")) {
        type = IR_FLOWCHART_EDGE_ARROW;
    } else if (match(p, "---")) {
        type = IR_FLOWCHART_EDGE_OPEN;
    } else if (match(p, "--")) {
        // Could be "-- label -->" or just "--"
        skip_whitespace(p);

        // Check if there's a label before arrow
        if (peek(p) != '-' && peek(p) != '=' && peek(p) != '.') {
            // Parse label text
            size_t start = p->pos;
            while (!is_at_end(p) && !match(p, "-->") && !match(p, "--->")
                   && peek(p) != '\n') {
                if (strncmp(p->source + p->pos, "-->", 3) == 0) break;
                advance(p);
            }
            size_t len = p->pos - start;
            if (len > 0) {
                label = (char*)malloc(len + 1);
                memcpy(label, p->source + start, len);
                label[len] = '\0';
                // Trim whitespace
                while (len > 0 && isspace(label[len - 1])) label[--len] = '\0';
            }
            // Now consume the arrow
            skip_whitespace(p);
            match(p, "-->");
            type = IR_FLOWCHART_EDGE_ARROW;
        } else {
            type = IR_FLOWCHART_EDGE_OPEN;
        }
    } else {
        return;  // Not an edge
    }

    // Check for |label| syntax after arrow
    skip_whitespace(p);
    if (peek(p) == '|') {
        advance(p);  // Skip |
        size_t start = p->pos;
        while (!is_at_end(p) && peek(p) != '|') {
            advance(p);
        }
        size_t len = p->pos - start;
        if (label) free(label);  // Replace any existing label
        label = (char*)malloc(len + 1);
        memcpy(label, p->source + start, len);
        label[len] = '\0';
        if (peek(p) == '|') advance(p);
    }

    // Parse target node ID
    skip_whitespace(p);
    char* to_id = parse_identifier(p);
    if (!to_id) {
        if (label) free(label);
        return;
    }

    // Create edge component
    IRComponent* edge = ir_flowchart_edge(from_id, to_id, type);
    if (edge && label) {
        IRFlowchartEdgeData* data = ir_get_flowchart_edge_data(edge);
        if (data) {
            ir_flowchart_edge_set_label(data, label);
        }
    }

    if (edge) {
        ir_add_child(p->flowchart, edge);
    }

    // Check if the target node needs definition
    skip_whitespace(p);
    if (peek(p) == '[' || peek(p) == '(' || peek(p) == '{' || peek(p) == '>') {
        parse_node_definition(p, to_id);
    }

    if (label) free(label);
    free(to_id);
}

static void parse_subgraph(FlowchartParser* p) {
    skip_whitespace(p);

    // Parse subgraph ID
    char* subgraph_id = parse_identifier(p);

    // Parse optional title in brackets
    skip_whitespace(p);
    char* title = NULL;
    if (peek(p) == '[') {
        title = parse_text_in_brackets(p, '[', ']');
    }

    // Create subgraph component
    IRComponent* subgraph = ir_flowchart_subgraph(subgraph_id, title ? title : subgraph_id);

    if (subgraph) {
        ir_add_child(p->flowchart, subgraph);
        p->current_subgraph = subgraph;
    }

    if (subgraph_id) free(subgraph_id);
    if (title) free(title);
}

static void parse_style(FlowchartParser* p) {
    skip_whitespace(p);

    char* node_id = parse_identifier(p);
    if (!node_id) return;

    skip_whitespace(p);

    // Parse style properties (basic support for fill: and stroke:)
    while (!is_at_end(p) && peek(p) != '\n') {
        skip_whitespace(p);

        if (match_case_insensitive(p, "fill:")) {
            skip_whitespace(p);
            // Parse color value
            size_t start = p->pos;
            while (!is_at_end(p) && peek(p) != ',' && peek(p) != '\n' && !isspace(peek(p))) {
                advance(p);
            }
            size_t len = p->pos - start;
            char* color_str = (char*)malloc(len + 1);
            memcpy(color_str, p->source + start, len);
            color_str[len] = '\0';

            // Find the node and set fill color
            IRFlowchartNodeData* node = ir_flowchart_find_node(p->state, node_id);
            if (node) {
                node->fill_color = parse_hex_color(color_str);
            }
            free(color_str);
        } else if (match_case_insensitive(p, "stroke:")) {
            skip_whitespace(p);
            size_t start = p->pos;
            while (!is_at_end(p) && peek(p) != ',' && peek(p) != '\n' && !isspace(peek(p))) {
                advance(p);
            }
            size_t len = p->pos - start;
            char* color_str = (char*)malloc(len + 1);
            memcpy(color_str, p->source + start, len);
            color_str[len] = '\0';

            IRFlowchartNodeData* node = ir_flowchart_find_node(p->state, node_id);
            if (node) {
                node->stroke_color = parse_hex_color(color_str);
            }
            free(color_str);
        } else if (match_case_insensitive(p, "stroke-width:")) {
            skip_whitespace(p);
            float width = 2.0f;
            sscanf(p->source + p->pos, "%f", &width);
            while (!is_at_end(p) && (isdigit(peek(p)) || peek(p) == '.')) {
                advance(p);
            }

            IRFlowchartNodeData* node = ir_flowchart_find_node(p->state, node_id);
            if (node) {
                node->stroke_width = width;
            }
        } else {
            // Skip unknown property
            while (!is_at_end(p) && peek(p) != ',' && peek(p) != '\n') {
                advance(p);
            }
        }

        // Skip comma separator
        if (peek(p) == ',') advance(p);
    }

    free(node_id);
}

static void parse_line(FlowchartParser* p) {
    skip_whitespace(p);

    // Skip empty lines and comments
    if (is_at_end(p) || peek(p) == '\n') return;
    if (peek(p) == '%') {
        while (!is_at_end(p) && peek(p) != '\n') advance(p);
        return;
    }

    // Check for keywords
    if (match_case_insensitive(p, "subgraph")) {
        parse_subgraph(p);
        return;
    }

    if (match_case_insensitive(p, "end")) {
        // End of subgraph
        p->current_subgraph = NULL;
        return;
    }

    if (match_case_insensitive(p, "style")) {
        parse_style(p);
        return;
    }

    if (match_case_insensitive(p, "classDef") || match_case_insensitive(p, "class")) {
        // Skip class definitions for now
        while (!is_at_end(p) && peek(p) != '\n') advance(p);
        return;
    }

    if (match_case_insensitive(p, "linkStyle")) {
        // Skip link styles for now
        while (!is_at_end(p) && peek(p) != '\n') advance(p);
        return;
    }

    // Parse node ID
    char* node_id = parse_identifier(p);
    if (!node_id) {
        // Skip to end of line
        while (!is_at_end(p) && peek(p) != '\n') advance(p);
        return;
    }

    skip_whitespace(p);

    // Check what follows the identifier
    char c = peek(p);

    if (c == '[' || c == '(' || c == '{' || c == '>') {
        // Node definition
        parse_node_definition(p, node_id);
        skip_whitespace(p);

        // Check for chained edge after node definition
        c = peek(p);
        if (c == '-' || c == '=' || c == '<') {
            parse_edge(p, node_id);
        }
    } else if (c == '-' || c == '=' || c == '<') {
        // Edge (node might be defined elsewhere or implicitly)
        // First ensure node exists (check children, not state which isn't finalized yet)
        bool node_exists = node_exists_in_children(p->flowchart, node_id);
        if (p->current_subgraph && !node_exists) {
            node_exists = node_exists_in_children(p->current_subgraph, node_id);
        }
        if (!node_exists) {
            // Create implicit node with default shape
            IRComponent* node = ir_flowchart_node(node_id, IR_FLOWCHART_SHAPE_RECTANGLE, node_id);
            if (node) {
                if (p->current_subgraph) {
                    ir_add_child(p->current_subgraph, node);
                } else {
                    ir_add_child(p->flowchart, node);
                }
            }
        }
        parse_edge(p, node_id);
    } else if (c == '&') {
        // Node chain: A & B & C (multiple nodes, often styled together)
        // For now, just create the first node
        bool node_exists = node_exists_in_children(p->flowchart, node_id);
        if (p->current_subgraph && !node_exists) {
            node_exists = node_exists_in_children(p->current_subgraph, node_id);
        }
        if (!node_exists) {
            IRComponent* node = ir_flowchart_node(node_id, IR_FLOWCHART_SHAPE_RECTANGLE, node_id);
            if (node) {
                if (p->current_subgraph) {
                    ir_add_child(p->current_subgraph, node);
                } else {
                    ir_add_child(p->flowchart, node);
                }
            }
        }
        // Skip the rest of the chain for now
        while (!is_at_end(p) && peek(p) != '\n') advance(p);
    }

    free(node_id);
}

bool ir_flowchart_is_mermaid(const char* source, size_t length) {
    if (!source || length == 0) return false;

    // Skip leading whitespace
    size_t i = 0;
    while (i < length && isspace(source[i])) i++;

    // Check for "flowchart" or "graph" keyword
    if (i + 9 <= length && strncasecmp(source + i, "flowchart", 9) == 0) {
        return true;
    }
    if (i + 5 <= length && strncasecmp(source + i, "graph", 5) == 0) {
        return true;
    }

    return false;
}

IRComponent* ir_flowchart_parse(const char* source, size_t length) {
    if (!source || length == 0) return NULL;

    FlowchartParser parser = {0};
    parser.source = source;
    parser.length = length;
    parser.pos = 0;
    parser.line = 1;
    parser.col = 0;
    parser.direction = IR_FLOWCHART_DIR_TB;
    parser.current_subgraph = NULL;

    skip_whitespace_and_newlines(&parser);

    // Parse header: "flowchart TB" or "graph LR" etc.
    if (match_case_insensitive(&parser, "flowchart") || match_case_insensitive(&parser, "graph")) {
        skip_whitespace(&parser);

        // Parse direction
        if (match_case_insensitive(&parser, "TB") || match_case_insensitive(&parser, "TD")) {
            parser.direction = IR_FLOWCHART_DIR_TB;
        } else if (match_case_insensitive(&parser, "LR")) {
            parser.direction = IR_FLOWCHART_DIR_LR;
        } else if (match_case_insensitive(&parser, "BT")) {
            parser.direction = IR_FLOWCHART_DIR_BT;
        } else if (match_case_insensitive(&parser, "RL")) {
            parser.direction = IR_FLOWCHART_DIR_RL;
        }
    } else {
        // Not a valid flowchart header
        return NULL;
    }

    // Create flowchart component
    parser.flowchart = ir_flowchart(parser.direction);
    if (!parser.flowchart) return NULL;

    parser.state = ir_get_flowchart_state(parser.flowchart);

    // Parse body lines
    while (!is_at_end(&parser)) {
        skip_whitespace_and_newlines(&parser);
        if (!is_at_end(&parser)) {
            parse_line(&parser);
            // Skip to next line
            while (!is_at_end(&parser) && peek(&parser) != '\n') {
                advance(&parser);
            }
        }
    }

    // Finalize flowchart (register all nodes/edges)
    ir_flowchart_finalize(parser.flowchart);

    return parser.flowchart;
}

char* ir_flowchart_to_kir(const char* source, size_t length) {
    IRComponent* flowchart = ir_flowchart_parse(source, length);
    if (!flowchart) return NULL;

    // Serialize to KIR JSON
    char* json = ir_serialize_json_v2(flowchart);

    // Clean up
    ir_destroy_component(flowchart);

    return json;
}
