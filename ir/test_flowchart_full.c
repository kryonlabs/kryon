/**
 * Full Flowchart Integration Test
 * Tests parsing, layout, serialization, and verifies the complete pipeline
 */

#include "ir_flowchart_parser.h"
#include "ir_builder.h"
#include "ir_serialization.h"
#include "ir_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Forward declaration
void ir_layout_compute_flowchart(IRComponent* flowchart, float available_width, float available_height);

static void print_separator(const char* title) {
    printf("\n========================================\n");
    printf("  %s\n", title);
    printf("========================================\n\n");
}

static bool test_simple_flowchart(void) {
    print_separator("Test 1: Simple Flowchart");

    const char* mermaid =
        "flowchart TB\n"
        "    A[Start] --> B{Is it working?}\n"
        "    B -->|Yes| C[Great!]\n"
        "    B -->|No| D[Debug]\n"
        "    D --> B\n";

    printf("Input Mermaid:\n%s\n", mermaid);

    IRComponent* flowchart = ir_flowchart_parse(mermaid, strlen(mermaid));
    if (!flowchart) {
        printf("ERROR: Failed to parse flowchart\n");
        return false;
    }

    IRFlowchartState* state = ir_get_flowchart_state(flowchart);
    if (!state) {
        printf("ERROR: No flowchart state\n");
        ir_destroy_component(flowchart);
        return false;
    }

    printf("Parsed successfully:\n");
    printf("  Direction: %s\n", ir_flowchart_direction_to_string(state->direction));
    printf("  Nodes: %u\n", state->node_count);
    printf("  Edges: %u\n", state->edge_count);

    // Print node details
    printf("\nNodes:\n");
    for (uint32_t i = 0; i < state->node_count; i++) {
        IRFlowchartNodeData* node = state->nodes[i];
        if (node) {
            printf("  [%s] shape=%s label=\"%s\"\n",
                   node->node_id ? node->node_id : "?",
                   ir_flowchart_shape_to_string(node->shape),
                   node->label ? node->label : "");
        }
    }

    // Print edge details
    printf("\nEdges:\n");
    for (uint32_t i = 0; i < state->edge_count; i++) {
        IRFlowchartEdgeData* edge = state->edges[i];
        if (edge) {
            printf("  %s --%s--> %s",
                   edge->from_id ? edge->from_id : "?",
                   ir_flowchart_edge_type_to_string(edge->type),
                   edge->to_id ? edge->to_id : "?");
            if (edge->label) {
                printf(" |%s|", edge->label);
            }
            printf("\n");
        }
    }

    // Test layout
    printf("\nComputing layout (800x600)...\n");
    ir_layout_compute_flowchart(flowchart, 800.0f, 600.0f);

    printf("\nNode positions after layout:\n");
    for (uint32_t i = 0; i < state->node_count; i++) {
        IRFlowchartNodeData* node = state->nodes[i];
        if (node) {
            printf("  [%s] at (%.1f, %.1f) size %.1fx%.1f\n",
                   node->node_id ? node->node_id : "?",
                   node->x, node->y, node->width, node->height);
        }
    }

    printf("\nEdge paths after layout:\n");
    for (uint32_t i = 0; i < state->edge_count; i++) {
        IRFlowchartEdgeData* edge = state->edges[i];
        if (edge && edge->path_points && edge->path_point_count >= 2) {
            printf("  %s -> %s: (%.1f,%.1f) -> (%.1f,%.1f)\n",
                   edge->from_id ? edge->from_id : "?",
                   edge->to_id ? edge->to_id : "?",
                   edge->path_points[0], edge->path_points[1],
                   edge->path_points[2], edge->path_points[3]);
        }
    }

    ir_destroy_component(flowchart);
    printf("\nTest 1 PASSED\n");
    return true;
}

static bool test_all_shapes(void) {
    print_separator("Test 2: All Node Shapes");

    const char* mermaid =
        "flowchart LR\n"
        "    A[Rectangle]\n"
        "    B(Rounded)\n"
        "    C{Diamond}\n"
        "    D((Circle))\n"
        "    E>Asymmetric]\n"
        "    F[(Database)]\n"
        "    G[[Subroutine]]\n"
        "    H{{Hexagon}}\n"
        "    A --> B --> C --> D\n"
        "    E --> F --> G --> H\n";

    printf("Input Mermaid:\n%s\n", mermaid);

    IRComponent* flowchart = ir_flowchart_parse(mermaid, strlen(mermaid));
    if (!flowchart) {
        printf("ERROR: Failed to parse flowchart\n");
        return false;
    }

    IRFlowchartState* state = ir_get_flowchart_state(flowchart);
    printf("Parsed %u nodes with shapes:\n", state->node_count);

    for (uint32_t i = 0; i < state->node_count; i++) {
        IRFlowchartNodeData* node = state->nodes[i];
        if (node) {
            printf("  [%s] %s: \"%s\"\n",
                   node->node_id,
                   ir_flowchart_shape_to_string(node->shape),
                   node->label);
        }
    }

    // Verify we got all 8 shapes
    if (state->node_count != 8) {
        printf("ERROR: Expected 8 nodes, got %u\n", state->node_count);
        ir_destroy_component(flowchart);
        return false;
    }

    // Test layout in LR direction
    ir_layout_compute_flowchart(flowchart, 1200.0f, 400.0f);

    printf("\nLayout (LR direction) positions:\n");
    for (uint32_t i = 0; i < state->node_count; i++) {
        IRFlowchartNodeData* node = state->nodes[i];
        if (node) {
            printf("  [%s] at (%.1f, %.1f)\n",
                   node->node_id, node->x, node->y);
        }
    }

    ir_destroy_component(flowchart);
    printf("\nTest 2 PASSED\n");
    return true;
}

static bool test_edge_types(void) {
    print_separator("Test 3: All Edge Types");

    const char* mermaid =
        "flowchart TB\n"
        "    A[Start]\n"
        "    B[Arrow]\n"
        "    C[Open]\n"
        "    D[Dotted]\n"
        "    E[Thick]\n"
        "    F[Bidirectional]\n"
        "    A --> B\n"
        "    A --- C\n"
        "    A -.-> D\n"
        "    A ==> E\n"
        "    A <--> F\n";

    printf("Input Mermaid:\n%s\n", mermaid);

    IRComponent* flowchart = ir_flowchart_parse(mermaid, strlen(mermaid));
    if (!flowchart) {
        printf("ERROR: Failed to parse flowchart\n");
        return false;
    }

    IRFlowchartState* state = ir_get_flowchart_state(flowchart);
    printf("Parsed %u edges:\n", state->edge_count);

    for (uint32_t i = 0; i < state->edge_count; i++) {
        IRFlowchartEdgeData* edge = state->edges[i];
        if (edge) {
            printf("  %s -> %s: %s\n",
                   edge->from_id, edge->to_id,
                   ir_flowchart_edge_type_to_string(edge->type));
        }
    }

    // Verify we got all 5 edge types
    if (state->edge_count != 5) {
        printf("ERROR: Expected 5 edges, got %u\n", state->edge_count);
        ir_destroy_component(flowchart);
        return false;
    }

    ir_destroy_component(flowchart);
    printf("\nTest 3 PASSED\n");
    return true;
}

static bool test_subgraphs(void) {
    print_separator("Test 4: Subgraphs");

    const char* mermaid =
        "flowchart TB\n"
        "    subgraph Frontend[Frontend Layer]\n"
        "        A[React App]\n"
        "        B[Vue App]\n"
        "    end\n"
        "    subgraph Backend[Backend Layer]\n"
        "        C[API Server]\n"
        "        D[Database]\n"
        "    end\n"
        "    A --> C\n"
        "    B --> C\n"
        "    C --> D\n";

    printf("Input Mermaid:\n%s\n", mermaid);

    IRComponent* flowchart = ir_flowchart_parse(mermaid, strlen(mermaid));
    if (!flowchart) {
        printf("ERROR: Failed to parse flowchart\n");
        return false;
    }

    IRFlowchartState* state = ir_get_flowchart_state(flowchart);
    printf("Parsed:\n");
    printf("  Nodes: %u\n", state->node_count);
    printf("  Edges: %u\n", state->edge_count);
    printf("  Subgraphs: %u\n", state->subgraph_count);

    if (state->subgraph_count != 2) {
        printf("ERROR: Expected 2 subgraphs, got %u\n", state->subgraph_count);
        ir_destroy_component(flowchart);
        return false;
    }

    printf("\nSubgraphs:\n");
    for (uint32_t i = 0; i < state->subgraph_count; i++) {
        IRFlowchartSubgraphData* sg = state->subgraphs[i];
        if (sg) {
            printf("  [%s] title=\"%s\"\n",
                   sg->subgraph_id ? sg->subgraph_id : "?",
                   sg->title ? sg->title : "");
        }
    }

    ir_destroy_component(flowchart);
    printf("\nTest 4 PASSED\n");
    return true;
}

static bool test_json_roundtrip(void) {
    print_separator("Test 5: JSON Roundtrip");

    const char* mermaid =
        "flowchart TB\n"
        "    A[Start] --> B{Decision}\n"
        "    B -->|yes| C[Yes Path]\n"
        "    B -->|no| D[No Path]\n";

    printf("Input Mermaid:\n%s\n", mermaid);

    // Parse to IR
    IRComponent* flowchart = ir_flowchart_parse(mermaid, strlen(mermaid));
    if (!flowchart) {
        printf("ERROR: Failed to parse flowchart\n");
        return false;
    }

    // Compute layout before serialization
    ir_layout_compute_flowchart(flowchart, 600.0f, 400.0f);

    // Serialize to JSON
    char* json = ir_flowchart_to_kir(mermaid, strlen(mermaid));
    if (!json) {
        printf("ERROR: Failed to serialize to JSON\n");
        ir_destroy_component(flowchart);
        return false;
    }

    printf("JSON output (first 500 chars):\n%.500s...\n", json);

    // Verify JSON contains expected elements
    bool has_flowchart = strstr(json, "Flowchart") != NULL;
    bool has_node = strstr(json, "FlowchartNode") != NULL;
    bool has_edge = strstr(json, "FlowchartEdge") != NULL;

    printf("\nJSON verification:\n");
    printf("  Contains 'Flowchart': %s\n", has_flowchart ? "YES" : "NO");
    printf("  Contains 'FlowchartNode': %s\n", has_node ? "YES" : "NO");
    printf("  Contains 'FlowchartEdge': %s\n", has_edge ? "YES" : "NO");

    free(json);
    ir_destroy_component(flowchart);

    if (!has_flowchart || !has_node || !has_edge) {
        printf("\nTest 5 FAILED - missing JSON elements\n");
        return false;
    }

    printf("\nTest 5 PASSED\n");
    return true;
}

static bool test_complex_flowchart(void) {
    print_separator("Test 6: Complex Flowchart (CI/CD Pipeline)");

    const char* mermaid =
        "flowchart LR\n"
        "    A[Code Push] --> B{Tests Pass?}\n"
        "    B -->|Yes| C[Build Docker]\n"
        "    B -->|No| D[Notify Dev]\n"
        "    D --> A\n"
        "    C --> E{Deploy to?}\n"
        "    E -->|Staging| F[(Staging DB)]\n"
        "    E -->|Prod| G[(Prod DB)]\n"
        "    F --> H{{Monitor}}\n"
        "    G --> H\n"
        "    H --> I((Done))\n";

    printf("Input Mermaid:\n%s\n", mermaid);

    IRComponent* flowchart = ir_flowchart_parse(mermaid, strlen(mermaid));
    if (!flowchart) {
        printf("ERROR: Failed to parse flowchart\n");
        return false;
    }

    IRFlowchartState* state = ir_get_flowchart_state(flowchart);
    printf("Parsed complex flowchart:\n");
    printf("  Direction: %s\n", ir_flowchart_direction_to_string(state->direction));
    printf("  Nodes: %u\n", state->node_count);
    printf("  Edges: %u\n", state->edge_count);

    // Verify counts
    if (state->node_count != 9) {
        printf("ERROR: Expected 9 nodes, got %u\n", state->node_count);
        ir_destroy_component(flowchart);
        return false;
    }

    if (state->edge_count != 10) {
        printf("ERROR: Expected 10 edges, got %u\n", state->edge_count);
        ir_destroy_component(flowchart);
        return false;
    }

    // Compute layout
    ir_layout_compute_flowchart(flowchart, 1000.0f, 600.0f);

    // Verify all nodes have valid positions
    bool all_positioned = true;
    for (uint32_t i = 0; i < state->node_count; i++) {
        IRFlowchartNodeData* node = state->nodes[i];
        if (node) {
            if (node->width <= 0 || node->height <= 0) {
                printf("ERROR: Node %s has invalid size\n", node->node_id);
                all_positioned = false;
            }
        }
    }

    // Verify all edges have paths
    bool all_paths = true;
    for (uint32_t i = 0; i < state->edge_count; i++) {
        IRFlowchartEdgeData* edge = state->edges[i];
        if (edge) {
            if (!edge->path_points || edge->path_point_count < 2) {
                printf("ERROR: Edge %s->%s has no path\n", edge->from_id, edge->to_id);
                all_paths = false;
            }
        }
    }

    ir_destroy_component(flowchart);

    if (!all_positioned || !all_paths) {
        printf("\nTest 6 FAILED\n");
        return false;
    }

    printf("\nTest 6 PASSED\n");
    return true;
}

static bool test_programmatic_api(void) {
    print_separator("Test 7: Programmatic API");

    printf("Building flowchart programmatically...\n");

    // Create flowchart
    IRComponent* flowchart = ir_flowchart(IR_FLOWCHART_DIR_TB);
    if (!flowchart) {
        printf("ERROR: Failed to create flowchart\n");
        return false;
    }

    // Create nodes
    IRComponent* node_a = ir_flowchart_node("start", IR_FLOWCHART_SHAPE_ROUNDED, "Start");
    IRComponent* node_b = ir_flowchart_node("process", IR_FLOWCHART_SHAPE_RECTANGLE, "Process");
    IRComponent* node_c = ir_flowchart_node("decision", IR_FLOWCHART_SHAPE_DIAMOND, "OK?");
    IRComponent* node_d = ir_flowchart_node("end", IR_FLOWCHART_SHAPE_CIRCLE, "End");

    // Create edges
    IRComponent* edge1 = ir_flowchart_edge("start", "process", IR_FLOWCHART_EDGE_ARROW);
    IRComponent* edge2 = ir_flowchart_edge("process", "decision", IR_FLOWCHART_EDGE_ARROW);
    IRComponent* edge3 = ir_flowchart_edge("decision", "end", IR_FLOWCHART_EDGE_ARROW);

    // Add to flowchart
    ir_add_child(flowchart, node_a);
    ir_add_child(flowchart, node_b);
    ir_add_child(flowchart, node_c);
    ir_add_child(flowchart, node_d);
    ir_add_child(flowchart, edge1);
    ir_add_child(flowchart, edge2);
    ir_add_child(flowchart, edge3);

    // Finalize to register nodes/edges
    ir_flowchart_finalize(flowchart);

    IRFlowchartState* state = ir_get_flowchart_state(flowchart);
    printf("Created flowchart:\n");
    printf("  Nodes: %u\n", state->node_count);
    printf("  Edges: %u\n", state->edge_count);

    if (state->node_count != 4 || state->edge_count != 3) {
        printf("ERROR: Wrong counts\n");
        ir_destroy_component(flowchart);
        return false;
    }

    // Compute layout
    ir_layout_compute_flowchart(flowchart, 400.0f, 500.0f);

    printf("\nNode positions:\n");
    for (uint32_t i = 0; i < state->node_count; i++) {
        IRFlowchartNodeData* node = state->nodes[i];
        if (node) {
            printf("  [%s] at (%.1f, %.1f)\n", node->node_id, node->x, node->y);
        }
    }

    ir_destroy_component(flowchart);
    printf("\nTest 7 PASSED\n");
    return true;
}

int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║   FLOWCHART FULL INTEGRATION TEST SUITE          ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");

    int passed = 0, failed = 0;

    if (test_simple_flowchart()) passed++; else failed++;
    if (test_all_shapes()) passed++; else failed++;
    if (test_edge_types()) passed++; else failed++;
    if (test_subgraphs()) passed++; else failed++;
    if (test_json_roundtrip()) passed++; else failed++;
    if (test_complex_flowchart()) passed++; else failed++;
    if (test_programmatic_api()) passed++; else failed++;

    printf("\n");
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║   RESULTS: %d PASSED, %d FAILED                    ║\n", passed, failed);
    printf("╚══════════════════════════════════════════════════╝\n\n");

    return failed > 0 ? 1 : 0;
}
