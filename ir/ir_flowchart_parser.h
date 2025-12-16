#ifndef IR_FLOWCHART_PARSER_H
#define IR_FLOWCHART_PARSER_H

#include "ir_core.h"
#include "ir_builder.h"

/**
 * Parse Mermaid flowchart syntax to IR components
 *
 * Supported syntax:
 *   flowchart TB       (or graph TB, flowchart LR, etc.)
 *   A[Rectangle]       Node with rectangle shape
 *   B(Rounded)         Node with rounded shape
 *   C{Diamond}         Node with diamond shape
 *   D((Circle))        Node with circle shape
 *   E>Asymmetric]      Node with asymmetric shape
 *   F[(Database)]      Node with cylinder shape
 *   G[[Subroutine]]    Node with subroutine shape
 *   H{{Hexagon}}       Node with hexagon shape
 *   A --> B            Arrow edge
 *   A --- B            Open edge (no arrow)
 *   A -.-> B           Dotted arrow
 *   A ==> B            Thick arrow
 *   A <--> B           Bidirectional arrow
 *   A -->|label| B     Edge with label
 *   A -- label --> B   Edge with label (alternative)
 *   subgraph id[title] Subgraph start
 *   end                Subgraph end
 *   style A fill:#f9f  Style definition (basic support)
 *
 * @param source Mermaid flowchart source code
 * @param length Length of source string
 * @return IRComponent* Root Flowchart component, or NULL on error
 */
IRComponent* ir_flowchart_parse(const char* source, size_t length);

/**
 * Convert Mermaid flowchart to KIR JSON string
 *
 * Convenience function that combines parsing and serialization.
 *
 * @param source Mermaid flowchart source code
 * @param length Length of source string
 * @return char* JSON string (caller must free), or NULL on error
 */
char* ir_flowchart_to_kir(const char* source, size_t length);

/**
 * Check if source looks like a Mermaid flowchart
 *
 * @param source Source string to check
 * @param length Length of source string
 * @return true if source starts with "flowchart" or "graph" keyword
 */
bool ir_flowchart_is_mermaid(const char* source, size_t length);

#endif // IR_FLOWCHART_PARSER_H
