/**
 * @file ir.h
 * @brief Kryon IR - Master public API header
 *
 * This is the master include for the Kryon Intermediate Representation (IR) library.
 * Include this header to access the complete public API.
 *
 * @copyright Kryon UI Framework
 * @version 1.0.0
 */

#ifndef IR_H
#define IR_H

// ============================================================================
// Core Data Structures
// ============================================================================

#include "ir_core.h"
#include "ir_types.h"
#include "ir_properties.h"

// ============================================================================
// Component Building API
// ============================================================================

#include "ir_builder.h"

// ============================================================================
// Style System
// ============================================================================

#include "ir_style.h"
#include "ir_layout.h"

// ============================================================================
// Logic and Execution
// ============================================================================

#include "ir_logic.h"
#include "ir_executor.h"
#include "ir_expression.h"

// ============================================================================
// Events
// ============================================================================

#include "ir_events.h"

// ============================================================================
// State Management
// ============================================================================

#include "ir_state.h"
#include "ir_instance.h"

// ============================================================================
// Serialization
// ============================================================================

#include "ir_serialization.h"

#endif // IR_H
