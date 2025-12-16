#include "ir_flowchart_parser.h"
#include "ir_builder.h"
#include "ir_serialization.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Test helper to print results
static void print_test_result(const char* test_name, bool passed) {
    printf("[%s] %s\n", passed ? "PASS" : "FAIL", test_name);
}

// Test: Check if source is recognized as Mermaid
static bool test_is_mermaid(void) {
    const char* valid1 = "flowchart TB\n  A --> B";
    const char* valid2 = "graph LR\n  A --> B";
    const char* invalid = "not a flowchart";

    bool ok = true;
    ok = ok && ir_flowchart_is_mermaid(valid1, strlen(valid1));
    ok = ok && ir_flowchart_is_mermaid(valid2, strlen(valid2));
    ok = ok && !ir_flowchart_is_mermaid(invalid, strlen(invalid));

    return ok;
}

// Test: Parse simple flowchart with nodes
static bool test_parse_simple_nodes(void) {
    const char* source =
        "flowchart TB\n"
        "    A[Start]\n"
        "    B{Decision}\n"
        "    C((End))\n";

    IRComponent* flowchart = ir_flowchart_parse(source, strlen(source));
    if (!flowchart) {
        printf("    Error: Parser returned NULL\n");
        return false;
    }

    if (flowchart->type != IR_COMPONENT_FLOWCHART) {
        printf("    Error: Root type is not FLOWCHART\n");
        ir_destroy_component(flowchart);
        return false;
    }

    IRFlowchartState* state = ir_get_flowchart_state(flowchart);
    if (!state) {
        printf("    Error: No flowchart state\n");
        ir_destroy_component(flowchart);
        return false;
    }

    if (state->direction != IR_FLOWCHART_DIR_TB) {
        printf("    Error: Direction is not TB\n");
        ir_destroy_component(flowchart);
        return false;
    }

    // Check that we have 3 registered nodes
    if (state->node_count != 3) {
        printf("    Error: Expected 3 nodes, got %u\n", state->node_count);
        ir_destroy_component(flowchart);
        return false;
    }

    ir_destroy_component(flowchart);
    return true;
}

// Test: Parse flowchart with edges
static bool test_parse_edges(void) {
    const char* source =
        "flowchart LR\n"
        "    A[Start] --> B[Process]\n"
        "    B --> C[End]\n"
        "    B -.-> D[Optional]\n";

    IRComponent* flowchart = ir_flowchart_parse(source, strlen(source));
    if (!flowchart) {
        printf("    Error: Parser returned NULL\n");
        return false;
    }

    IRFlowchartState* state = ir_get_flowchart_state(flowchart);
    if (!state) {
        printf("    Error: No flowchart state\n");
        ir_destroy_component(flowchart);
        return false;
    }

    if (state->direction != IR_FLOWCHART_DIR_LR) {
        printf("    Error: Direction is not LR\n");
        ir_destroy_component(flowchart);
        return false;
    }

    // Check edges
    if (state->edge_count != 3) {
        printf("    Error: Expected 3 edges, got %u\n", state->edge_count);
        ir_destroy_component(flowchart);
        return false;
    }

    ir_destroy_component(flowchart);
    return true;
}

// Test: Parse flowchart with edge labels
static bool test_parse_edge_labels(void) {
    const char* source =
        "flowchart TB\n"
        "    A --> |yes| B\n"
        "    A --> |no| C\n";

    IRComponent* flowchart = ir_flowchart_parse(source, strlen(source));
    if (!flowchart) {
        printf("    Error: Parser returned NULL\n");
        return false;
    }

    IRFlowchartState* state = ir_get_flowchart_state(flowchart);
    if (!state || state->edge_count != 2) {
        printf("    Error: Expected 2 edges\n");
        ir_destroy_component(flowchart);
        return false;
    }

    // Check that edges have labels
    bool found_yes = false, found_no = false;
    for (uint32_t i = 0; i < state->edge_count; i++) {
        if (state->edges[i]->label) {
            if (strcmp(state->edges[i]->label, "yes") == 0) found_yes = true;
            if (strcmp(state->edges[i]->label, "no") == 0) found_no = true;
        }
    }

    if (!found_yes || !found_no) {
        printf("    Error: Edge labels not found\n");
        ir_destroy_component(flowchart);
        return false;
    }

    ir_destroy_component(flowchart);
    return true;
}

// Test: Generate KIR JSON
static bool test_to_kir_json(void) {
    const char* source =
        "flowchart TB\n"
        "    A[Start] --> B[End]\n";

    char* json = ir_flowchart_to_kir(source, strlen(source));
    if (!json) {
        printf("    Error: ir_flowchart_to_kir returned NULL\n");
        return false;
    }

    // Check that JSON contains expected fields
    bool ok = true;
    ok = ok && strstr(json, "\"type\"") != NULL;
    ok = ok && strstr(json, "Flowchart") != NULL;
    ok = ok && strstr(json, "FlowchartNode") != NULL;
    ok = ok && strstr(json, "FlowchartEdge") != NULL;

    if (!ok) {
        printf("    Error: JSON missing expected fields\n");
        printf("    JSON output:\n%s\n", json);
    }

    free(json);
    return ok;
}

// Test: All node shapes
static bool test_all_shapes(void) {
    const char* source =
        "flowchart TB\n"
        "    A[Rectangle]\n"
        "    B(Rounded)\n"
        "    C{Diamond}\n"
        "    D((Circle))\n"
        "    E>Asymmetric]\n"
        "    F[(Cylinder)]\n"
        "    G[[Subroutine]]\n"
        "    H{{Hexagon}}\n";

    IRComponent* flowchart = ir_flowchart_parse(source, strlen(source));
    if (!flowchart) {
        printf("    Error: Parser returned NULL\n");
        return false;
    }

    IRFlowchartState* state = ir_get_flowchart_state(flowchart);
    if (!state || state->node_count != 8) {
        printf("    Error: Expected 8 nodes, got %u\n", state ? state->node_count : 0);
        ir_destroy_component(flowchart);
        return false;
    }

    ir_destroy_component(flowchart);
    return true;
}

// Test: Subgraphs
static bool test_subgraphs(void) {
    const char* source =
        "flowchart TB\n"
        "    subgraph group1[My Group]\n"
        "        A[Node A]\n"
        "        B[Node B]\n"
        "    end\n"
        "    C[Outside]\n";

    IRComponent* flowchart = ir_flowchart_parse(source, strlen(source));
    if (!flowchart) {
        printf("    Error: Parser returned NULL\n");
        return false;
    }

    IRFlowchartState* state = ir_get_flowchart_state(flowchart);
    if (!state) {
        printf("    Error: No flowchart state\n");
        ir_destroy_component(flowchart);
        return false;
    }

    // Should have subgraph registered
    if (state->subgraph_count != 1) {
        printf("    Error: Expected 1 subgraph, got %u\n", state->subgraph_count);
        ir_destroy_component(flowchart);
        return false;
    }

    ir_destroy_component(flowchart);
    return true;
}

int main(void) {
    printf("=== Flowchart Parser Tests ===\n\n");

    int passed = 0, failed = 0;

    #define RUN_TEST(fn, name) do { \
        bool result = fn(); \
        print_test_result(name, result); \
        if (result) passed++; else failed++; \
    } while(0)

    RUN_TEST(test_is_mermaid, "Is Mermaid detection");
    RUN_TEST(test_parse_simple_nodes, "Parse simple nodes");
    RUN_TEST(test_parse_edges, "Parse edges");
    RUN_TEST(test_parse_edge_labels, "Parse edge labels");
    RUN_TEST(test_to_kir_json, "Generate KIR JSON");
    RUN_TEST(test_all_shapes, "All node shapes");
    RUN_TEST(test_subgraphs, "Subgraphs");

    printf("\n=== Results: %d passed, %d failed ===\n", passed, failed);

    return failed > 0 ? 1 : 0;
}
