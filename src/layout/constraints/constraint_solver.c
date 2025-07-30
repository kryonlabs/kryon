/**
 * @file constraint_solver.c
 * @brief Kryon Constraint-Based Layout Solver Implementation
 */

#include "internal/layout.h"
#include "internal/memory.h"
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <float.h>

// =============================================================================
// CONSTRAINT TYPES AND STRUCTURES
// =============================================================================

typedef enum {
    KRYON_CONSTRAINT_EQUAL,        // var1 = var2 + constant
    KRYON_CONSTRAINT_LESS_EQUAL,   // var1 <= var2 + constant
    KRYON_CONSTRAINT_GREATER_EQUAL // var1 >= var2 + constant
} KryonConstraintRelation;

typedef enum {
    KRYON_VARIABLE_X,
    KRYON_VARIABLE_Y,
    KRYON_VARIABLE_WIDTH,
    KRYON_VARIABLE_HEIGHT,
    KRYON_VARIABLE_LEFT,
    KRYON_VARIABLE_TOP,
    KRYON_VARIABLE_RIGHT,
    KRYON_VARIABLE_BOTTOM,
    KRYON_VARIABLE_CENTER_X,
    KRYON_VARIABLE_CENTER_Y
} KryonVariableType;

typedef struct {
    KryonLayoutNode* node;
    KryonVariableType type;
    uint32_t id;
} KryonLayoutVariable;

typedef struct {
    uint32_t var1_id;
    uint32_t var2_id;
    float constant;
    float strength;
    KryonConstraintRelation relation;
    bool is_active;
} KryonLayoutConstraint;

typedef struct {
    uint32_t variable_id;
    float value;
    float edit_strength;
    bool is_external;
} KryonVariableValue;

typedef struct {
    // Variables
    KryonLayoutVariable* variables;
    size_t variable_count;
    size_t variable_capacity;
    uint32_t next_variable_id;
    
    // Constraints
    KryonLayoutConstraint* constraints;
    size_t constraint_count;
    size_t constraint_capacity;
    
    // Variable values
    KryonVariableValue* values;
    size_t value_count;
    size_t value_capacity;
    
    // Solver state
    bool needs_solving;
    uint32_t iteration_count;
    uint32_t max_iterations;
    float tolerance;
    
} KryonConstraintSolver;

static KryonConstraintSolver g_constraint_solver = {0};

// =============================================================================
// VARIABLE MANAGEMENT
// =============================================================================

static uint32_t add_variable(KryonLayoutNode* node, KryonVariableType type) {
    if (g_constraint_solver.variable_count >= g_constraint_solver.variable_capacity) {
        size_t new_capacity = g_constraint_solver.variable_capacity == 0 ? 32 : g_constraint_solver.variable_capacity * 2;
        KryonLayoutVariable* new_variables = kryon_alloc(sizeof(KryonLayoutVariable) * new_capacity);
        if (!new_variables) return 0;
        
        if (g_constraint_solver.variables) {
            memcpy(new_variables, g_constraint_solver.variables, 
                   sizeof(KryonLayoutVariable) * g_constraint_solver.variable_count);
            kryon_free(g_constraint_solver.variables);
        }
        
        g_constraint_solver.variables = new_variables;
        g_constraint_solver.variable_capacity = new_capacity;
    }
    
    KryonLayoutVariable* var = &g_constraint_solver.variables[g_constraint_solver.variable_count];
    var->node = node;
    var->type = type;
    var->id = ++g_constraint_solver.next_variable_id;
    
    g_constraint_solver.variable_count++;
    return var->id;
}

static KryonLayoutVariable* find_variable(uint32_t variable_id) {
    for (size_t i = 0; i < g_constraint_solver.variable_count; i++) {
        if (g_constraint_solver.variables[i].id == variable_id) {
            return &g_constraint_solver.variables[i];
        }
    }
    return NULL;
}

static uint32_t get_or_create_variable(KryonLayoutNode* node, KryonVariableType type) {
    // Check if variable already exists
    for (size_t i = 0; i < g_constraint_solver.variable_count; i++) {
        KryonLayoutVariable* var = &g_constraint_solver.variables[i];
        if (var->node == node && var->type == type) {
            return var->id;
        }
    }
    
    // Create new variable
    return add_variable(node, type);
}

// =============================================================================
// CONSTRAINT MANAGEMENT
// =============================================================================

static bool add_constraint(uint32_t var1_id, uint32_t var2_id, float constant, 
                          float strength, KryonConstraintRelation relation) {
    if (g_constraint_solver.constraint_count >= g_constraint_solver.constraint_capacity) {
        size_t new_capacity = g_constraint_solver.constraint_capacity == 0 ? 32 : g_constraint_solver.constraint_capacity * 2;
        KryonLayoutConstraint* new_constraints = kryon_alloc(sizeof(KryonLayoutConstraint) * new_capacity);
        if (!new_constraints) return false;
        
        if (g_constraint_solver.constraints) {
            memcpy(new_constraints, g_constraint_solver.constraints,
                   sizeof(KryonLayoutConstraint) * g_constraint_solver.constraint_count);
            kryon_free(g_constraint_solver.constraints);
        }
        
        g_constraint_solver.constraints = new_constraints;
        g_constraint_solver.constraint_capacity = new_capacity;
    }
    
    KryonLayoutConstraint* constraint = &g_constraint_solver.constraints[g_constraint_solver.constraint_count];
    constraint->var1_id = var1_id;
    constraint->var2_id = var2_id;
    constraint->constant = constant;
    constraint->strength = strength;
    constraint->relation = relation;
    constraint->is_active = true;
    
    g_constraint_solver.constraint_count++;
    g_constraint_solver.needs_solving = true;
    
    return true;
}

// =============================================================================
// VARIABLE VALUE MANAGEMENT
// =============================================================================

static KryonVariableValue* get_or_create_value(uint32_t variable_id) {
    // Check if value already exists
    for (size_t i = 0; i < g_constraint_solver.value_count; i++) {
        if (g_constraint_solver.values[i].variable_id == variable_id) {
            return &g_constraint_solver.values[i];
        }
    }
    
    // Create new value
    if (g_constraint_solver.value_count >= g_constraint_solver.value_capacity) {
        size_t new_capacity = g_constraint_solver.value_capacity == 0 ? 32 : g_constraint_solver.value_capacity * 2;
        KryonVariableValue* new_values = kryon_alloc(sizeof(KryonVariableValue) * new_capacity);
        if (!new_values) return NULL;
        
        if (g_constraint_solver.values) {
            memcpy(new_values, g_constraint_solver.values,
                   sizeof(KryonVariableValue) * g_constraint_solver.value_count);
            kryon_free(g_constraint_solver.values);
        }
        
        g_constraint_solver.values = new_values;
        g_constraint_solver.value_capacity = new_capacity;
    }
    
    KryonVariableValue* value = &g_constraint_solver.values[g_constraint_solver.value_count];
    value->variable_id = variable_id;
    value->value = 0.0f;
    value->edit_strength = 1.0f;
    value->is_external = false;
    
    g_constraint_solver.value_count++;
    return value;
}

static float get_variable_value(uint32_t variable_id) {
    for (size_t i = 0; i < g_constraint_solver.value_count; i++) {
        if (g_constraint_solver.values[i].variable_id == variable_id) {
            return g_constraint_solver.values[i].value;
        }
    }
    return 0.0f;
}

static void set_variable_value(uint32_t variable_id, float value) {
    KryonVariableValue* var_value = get_or_create_value(variable_id);
    if (var_value) {
        var_value->value = value;
    }
}

// =============================================================================
// CONSTRAINT SOLVING ALGORITHM
// =============================================================================

static float evaluate_constraint_error(const KryonLayoutConstraint* constraint) {
    float var1_value = get_variable_value(constraint->var1_id);
    float var2_value = get_variable_value(constraint->var2_id);
    float target_value = var2_value + constraint->constant;
    
    switch (constraint->relation) {
        case KRYON_CONSTRAINT_EQUAL:
            return var1_value - target_value;
            
        case KRYON_CONSTRAINT_LESS_EQUAL:
            return fmaxf(0.0f, var1_value - target_value);
            
        case KRYON_CONSTRAINT_GREATER_EQUAL:
            return fmaxf(0.0f, target_value - var1_value);
            
        default:
            return 0.0f;
    }
}

static void apply_constraint_correction(const KryonLayoutConstraint* constraint, float error, float damping) {
    if (fabsf(error) < g_constraint_solver.tolerance) return;
    
    float correction = error * constraint->strength * damping;
    
    // Apply half correction to each variable (simple approach)
    float var1_value = get_variable_value(constraint->var1_id);
    float var2_value = get_variable_value(constraint->var2_id);
    
    set_variable_value(constraint->var1_id, var1_value - correction * 0.5f);
    set_variable_value(constraint->var2_id, var2_value + correction * 0.5f);
}

static bool solve_constraints_iteratively(void) {
    if (!g_constraint_solver.needs_solving) return true;
    
    float damping = 0.5f; // Damping factor to prevent oscillation
    bool converged = false;
    
    for (uint32_t iteration = 0; iteration < g_constraint_solver.max_iterations; iteration++) {
        float total_error = 0.0f;
        
        // Process all constraints
        for (size_t i = 0; i < g_constraint_solver.constraint_count; i++) {
            KryonLayoutConstraint* constraint = &g_constraint_solver.constraints[i];
            if (!constraint->is_active) continue;
            
            float error = evaluate_constraint_error(constraint);
            total_error += fabsf(error);
            
            apply_constraint_correction(constraint, error, damping);
        }
        
        // Check for convergence
        if (total_error < g_constraint_solver.tolerance) {
            converged = true;
            break;
        }
        
        // Reduce damping over iterations
        damping *= 0.99f;
    }
    
    g_constraint_solver.needs_solving = false;
    return converged;
}

// =============================================================================
// LAYOUT NODE INTEGRATION
// =============================================================================

static void sync_variables_to_nodes(void) {
    for (size_t i = 0; i < g_constraint_solver.variable_count; i++) {
        KryonLayoutVariable* var = &g_constraint_solver.variables[i];
        float value = get_variable_value(var->id);
        
        switch (var->type) {
            case KRYON_VARIABLE_X:
                var->node->computed.x = value;
                break;
            case KRYON_VARIABLE_Y:
                var->node->computed.y = value;
                break;
            case KRYON_VARIABLE_WIDTH:
                var->node->computed.width = fmaxf(0.0f, value);
                break;
            case KRYON_VARIABLE_HEIGHT:
                var->node->computed.height = fmaxf(0.0f, value);
                break;
            case KRYON_VARIABLE_LEFT:
                var->node->computed.x = value;
                break;
            case KRYON_VARIABLE_TOP:
                var->node->computed.y = value;
                break;
            case KRYON_VARIABLE_RIGHT:
                // Right = Left + Width, so this is derived
                break;
            case KRYON_VARIABLE_BOTTOM:
                // Bottom = Top + Height, so this is derived
                break;
            case KRYON_VARIABLE_CENTER_X:
                var->node->computed.x = value - var->node->computed.width / 2.0f;
                break;
            case KRYON_VARIABLE_CENTER_Y:
                var->node->computed.y = value - var->node->computed.height / 2.0f;
                break;
        }
    }
}

static void sync_nodes_to_variables(void) {
    for (size_t i = 0; i < g_constraint_solver.variable_count; i++) {
        KryonLayoutVariable* var = &g_constraint_solver.variables[i];
        float value = 0.0f;
        
        switch (var->type) {
            case KRYON_VARIABLE_X:
            case KRYON_VARIABLE_LEFT:
                value = var->node->computed.x;
                break;
            case KRYON_VARIABLE_Y:
            case KRYON_VARIABLE_TOP:
                value = var->node->computed.y;
                break;
            case KRYON_VARIABLE_WIDTH:
                value = var->node->computed.width;
                break;
            case KRYON_VARIABLE_HEIGHT:
                value = var->node->computed.height;
                break;
            case KRYON_VARIABLE_RIGHT:
                value = var->node->computed.x + var->node->computed.width;
                break;
            case KRYON_VARIABLE_BOTTOM:
                value = var->node->computed.y + var->node->computed.height;
                break;
            case KRYON_VARIABLE_CENTER_X:
                value = var->node->computed.x + var->node->computed.width / 2.0f;
                break;
            case KRYON_VARIABLE_CENTER_Y:
                value = var->node->computed.y + var->node->computed.height / 2.0f;
                break;
        }
        
        set_variable_value(var->id, value);
    }
}

// =============================================================================
// BUILT-IN CONSTRAINT GENERATORS
// =============================================================================

static void add_size_constraints(KryonLayoutNode* node) {
    // Width and height should be non-negative
    uint32_t width_var = get_or_create_variable(node, KRYON_VARIABLE_WIDTH);
    uint32_t height_var = get_or_create_variable(node, KRYON_VARIABLE_HEIGHT);
    
    // Create a zero constant variable
    static uint32_t zero_var = 0;
    if (zero_var == 0) {
        zero_var = ++g_constraint_solver.next_variable_id;
        set_variable_value(zero_var, 0.0f);
    }
    
    add_constraint(width_var, zero_var, 0.0f, 1000.0f, KRYON_CONSTRAINT_GREATER_EQUAL);
    add_constraint(height_var, zero_var, 0.0f, 1000.0f, KRYON_CONSTRAINT_GREATER_EQUAL);
    
    // Add fixed size constraints if specified
    if (node->style.width != KRYON_UNDEFINED) {
        add_constraint(width_var, zero_var, node->style.width, 1000.0f, KRYON_CONSTRAINT_EQUAL);
    }
    if (node->style.height != KRYON_UNDEFINED) {
        add_constraint(height_var, zero_var, node->style.height, 1000.0f, KRYON_CONSTRAINT_EQUAL);
    }
    
    // Min/max size constraints
    if (node->style.min_width != KRYON_UNDEFINED) {
        add_constraint(width_var, zero_var, node->style.min_width, 1000.0f, KRYON_CONSTRAINT_GREATER_EQUAL);
    }
    if (node->style.max_width != KRYON_UNDEFINED) {
        add_constraint(width_var, zero_var, node->style.max_width, 1000.0f, KRYON_CONSTRAINT_LESS_EQUAL);
    }
    if (node->style.min_height != KRYON_UNDEFINED) {
        add_constraint(height_var, zero_var, node->style.min_height, 1000.0f, KRYON_CONSTRAINT_GREATER_EQUAL);
    }
    if (node->style.max_height != KRYON_UNDEFINED) {
        add_constraint(height_var, zero_var, node->style.max_height, 1000.0f, KRYON_CONSTRAINT_LESS_EQUAL);
    }
}

static void add_position_constraints(KryonLayoutNode* node, KryonLayoutNode* parent) {
    if (!parent) return;
    
    uint32_t x_var = get_or_create_variable(node, KRYON_VARIABLE_X);
    uint32_t y_var = get_or_create_variable(node, KRYON_VARIABLE_Y);
    uint32_t parent_x_var = get_or_create_variable(parent, KRYON_VARIABLE_X);
    uint32_t parent_y_var = get_or_create_variable(parent, KRYON_VARIABLE_Y);
    
    // Position relative to parent
    if (node->style.left != KRYON_UNDEFINED) {
        add_constraint(x_var, parent_x_var, node->style.left, 1000.0f, KRYON_CONSTRAINT_EQUAL);
    }
    if (node->style.top != KRYON_UNDEFINED) {
        add_constraint(y_var, parent_y_var, node->style.top, 1000.0f, KRYON_CONSTRAINT_EQUAL);
    }
    
    // Right and bottom positioning
    if (node->style.right != KRYON_UNDEFINED) {
        uint32_t parent_width_var = get_or_create_variable(parent, KRYON_VARIABLE_WIDTH);
        uint32_t width_var = get_or_create_variable(node, KRYON_VARIABLE_WIDTH);
        
        // x + width = parent_x + parent_width - right
        // x = parent_x + parent_width - right - width
        // This is complex for the simple solver, so we'll approximate
        add_constraint(x_var, parent_x_var, parent->computed.width - node->style.right - node->computed.width, 
                      100.0f, KRYON_CONSTRAINT_EQUAL);
    }
    if (node->style.bottom != KRYON_UNDEFINED) {
        uint32_t parent_height_var = get_or_create_variable(parent, KRYON_VARIABLE_HEIGHT);
        uint32_t height_var = get_or_create_variable(node, KRYON_VARIABLE_HEIGHT);
        
        add_constraint(y_var, parent_y_var, parent->computed.height - node->style.bottom - node->computed.height,
                      100.0f, KRYON_CONSTRAINT_EQUAL);
    }
}

// =============================================================================
// PUBLIC API
// =============================================================================

bool kryon_constraint_solver_init(void) {
    memset(&g_constraint_solver, 0, sizeof(g_constraint_solver));
    
    g_constraint_solver.next_variable_id = 1;
    g_constraint_solver.max_iterations = 100;
    g_constraint_solver.tolerance = 0.01f;
    
    return true;
}

void kryon_constraint_solver_shutdown(void) {
    kryon_free(g_constraint_solver.variables);
    kryon_free(g_constraint_solver.constraints);
    kryon_free(g_constraint_solver.values);
    
    memset(&g_constraint_solver, 0, sizeof(g_constraint_solver));
}

bool kryon_layout_constraints(KryonLayoutNode* container) {
    if (!container) return false;
    
    // Clear existing constraints for this layout pass
    g_constraint_solver.constraint_count = 0;
    g_constraint_solver.variable_count = 0;
    g_constraint_solver.value_count = 0;
    g_constraint_solver.needs_solving = true;
    
    // Generate constraints for container and children
    add_size_constraints(container);
    
    // Process children recursively
    for (size_t i = 0; i < container->child_count; i++) {
        KryonLayoutNode* child = container->children[i];
        
        add_size_constraints(child);
        add_position_constraints(child, container);
        
        // Add constraints between siblings if needed
        // This would be where we add relative positioning constraints
    }
    
    // Initialize variable values from current node states
    sync_nodes_to_variables();
    
    // Solve constraints
    bool converged = solve_constraints_iteratively();
    
    // Apply solved values back to nodes
    sync_variables_to_nodes();
    
    return converged;
}

uint32_t kryon_constraint_add_equal(KryonLayoutNode* node1, KryonVariableType type1,
                                   KryonLayoutNode* node2, KryonVariableType type2,
                                   float constant, float strength) {
    uint32_t var1 = get_or_create_variable(node1, type1);
    uint32_t var2 = get_or_create_variable(node2, type2);
    
    if (add_constraint(var1, var2, constant, strength, KRYON_CONSTRAINT_EQUAL)) {
        return g_constraint_solver.constraint_count; // Return constraint ID
    }
    
    return 0;
}

uint32_t kryon_constraint_add_less_equal(KryonLayoutNode* node1, KryonVariableType type1,
                                        KryonLayoutNode* node2, KryonVariableType type2,
                                        float constant, float strength) {
    uint32_t var1 = get_or_create_variable(node1, type1);
    uint32_t var2 = get_or_create_variable(node2, type2);
    
    if (add_constraint(var1, var2, constant, strength, KRYON_CONSTRAINT_LESS_EQUAL)) {
        return g_constraint_solver.constraint_count;
    }
    
    return 0;
}

uint32_t kryon_constraint_add_greater_equal(KryonLayoutNode* node1, KryonVariableType type1,
                                           KryonLayoutNode* node2, KryonVariableType type2,
                                           float constant, float strength) {
    uint32_t var1 = get_or_create_variable(node1, type1);
    uint32_t var2 = get_or_create_variable(node2, type2);
    
    if (add_constraint(var1, var2, constant, strength, KRYON_CONSTRAINT_GREATER_EQUAL)) {
        return g_constraint_solver.constraint_count;
    }
    
    return 0;
}

bool kryon_constraint_remove(uint32_t constraint_id) {
    if (constraint_id == 0 || constraint_id > g_constraint_solver.constraint_count) {
        return false;
    }
    
    KryonLayoutConstraint* constraint = &g_constraint_solver.constraints[constraint_id - 1];
    constraint->is_active = false;
    g_constraint_solver.needs_solving = true;
    
    return true;
}

void kryon_constraint_set_variable_value(KryonLayoutNode* node, KryonVariableType type, float value) {
    uint32_t var_id = get_or_create_variable(node, type);
    set_variable_value(var_id, value);
    g_constraint_solver.needs_solving = true;
}

float kryon_constraint_get_variable_value(KryonLayoutNode* node, KryonVariableType type) {
    uint32_t var_id = get_or_create_variable(node, type);
    return get_variable_value(var_id);
}

void kryon_constraint_solver_set_max_iterations(uint32_t max_iterations) {
    g_constraint_solver.max_iterations = max_iterations;
}

void kryon_constraint_solver_set_tolerance(float tolerance) {
    g_constraint_solver.tolerance = tolerance;
}

size_t kryon_constraint_solver_get_constraint_count(void) {
    return g_constraint_solver.constraint_count;
}

size_t kryon_constraint_solver_get_variable_count(void) {
    return g_constraint_solver.variable_count;
}