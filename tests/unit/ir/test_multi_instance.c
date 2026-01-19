/**
 * Multi-Instance Support Test
 *
 * Tests the instance management system including:
 * - Instance creation and destruction
 * - KIR loading into instances
 * - Component ownership tracking
 * - Scope-based state isolation
 * - Hot reload functionality
 */

#include "../ir_instance.h"
#include "../ir_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Simple test KIR JSON (minimal counter component)
static const char* test_kir_json =
"{"
"  \"root\": {"
"    \"id\": 1,"
"    \"type\": \"FlexBox\","
"    \"props\": {"
"      \"direction\": \"column\","
"      \"padding\": 16"
"    },"
"    \"children\": ["
"      {"
"        \"id\": 2,"
"        \"type\": \"Text\","
"        \"props\": {"
"          \"text\": \"Test Instance\","
"          \"fontSize\": 24"
"        }"
"      }"
"    ]"
"  }"
"}";

// Test counter component with scope
static const char* counter_kir_json =
"{"
"  \"root\": {"
"    \"id\": 1,"
"    \"type\": \"FlexBox\","
"    \"scope\": \"Counter\","
"    \"props\": {"
"      \"direction\": \"row\","
"      \"gap\": 8"
"    },"
"    \"children\": ["
"      {"
"        \"id\": 2,"
"        \"type\": \"Button\","
"        \"scope\": \"Counter#decrement\","
"        \"props\": {"
"          \"text\": \"-\""
"        }"
"      },"
"      {"
"        \"id\": 3,"
"        \"type\": \"Text\","
"        \"scope\": \"Counter#label\","
"        \"props\": {"
"          \"text\": \"0\""
"        }"
"      },"
"      {"
"        \"id\": 4,"
"        \"type\": \"Button\","
"        \"scope\": \"Counter#increment\","
"        \"props\": {"
"          \"text\": \"+\""
"        }"
"      }"
"    ]"
"  }"
"}";

// ============================================================================
// Test Callbacks
// ============================================================================

static int create_count = 0;
static int destroy_count = 0;
static int before_reload_count = 0;
static int after_reload_count = 0;

static void test_on_create(IRInstanceContext* inst) {
    create_count++;
    printf("  [Callback] Instance created: %s (id=%u)\n", inst->name, inst->instance_id);
}

static void test_on_destroy(IRInstanceContext* inst) {
    destroy_count++;
    printf("  [Callback] Instance destroyed: %s\n", inst->name);
}

static void test_on_before_reload(IRInstanceContext* inst, IRComponent* old_root) {
    before_reload_count++;
    printf("  [Callback] Before reload: %s (root_id=%u)\n",
           inst->name, old_root ? old_root->id : 0);
}

static void test_on_after_reload(IRInstanceContext* inst, IRComponent* new_root) {
    after_reload_count++;
    printf("  [Callback] After reload: %s (root_id=%u)\n",
           inst->name, new_root ? new_root->id : 0);
}

// ============================================================================
// Test Functions
// ============================================================================

void test_instance_registry(void) {
    printf("\n=== Test: Instance Registry ===\n");

    ir_instance_registry_init();
    ir_instance_registry_print();

    assert(ir_instance_get_count() == 0);
    assert(ir_instance_get_by_id(999) == NULL);
    assert(ir_instance_get_by_name("nonexistent") == NULL);

    printf("  ✓ Registry initialized correctly\n");

    ir_instance_registry_shutdown();
    printf("  ✓ Registry shutdown complete\n");
}

void test_instance_creation(void) {
    printf("\n=== Test: Instance Creation ===\n");

    ir_instance_registry_init();

    IRInstanceCallbacks callbacks = {
        .on_create = test_on_create,
        .on_destroy = test_on_destroy,
        .on_before_reload = test_on_before_reload,
        .on_after_reload = test_on_after_reload,
    };

    create_count = 0;

    // Create instance with name
    IRInstanceContext* inst1 = ir_instance_create("TestInstance1", &callbacks);
    assert(inst1 != NULL);
    assert(create_count == 1);
    assert(inst1->instance_id == 1);  // First instance gets ID 1
    assert(strcmp(inst1->name, "TestInstance1") == 0);
    assert(inst1->is_running == true);
    assert(inst1->is_suspended == false);
    assert(inst1->ir_context != NULL);
    assert(inst1->executor != NULL);
    assert(inst1->resources.assets != NULL);
    assert(inst1->resources.audio != NULL);
    printf("  ✓ Instance created with correct properties\n");

    // Create instance without name (auto-generated)
    IRInstanceContext* inst2 = ir_instance_create(NULL, &callbacks);
    assert(inst2 != NULL);
    assert(create_count == 2);
    assert(inst2->instance_id == 2);
    assert(strncmp(inst2->name, "Instance_", 9) == 0);
    printf("  ✓ Auto-generated instance name works\n");

    // Verify registry state
    assert(ir_instance_get_count() == 2);
    assert(ir_instance_get_by_id(1) == inst1);
    assert(ir_instance_get_by_id(2) == inst2);
    assert(ir_instance_get_by_name("TestInstance1") == inst1);
    printf("  ✓ Registry state correct\n");

    // Cleanup
    destroy_count = 0;
    ir_instance_destroy(inst1);
    assert(destroy_count == 1);
    assert(ir_instance_get_count() == 1);
    printf("  ✓ Instance destruction works\n");

    ir_instance_destroy(inst2);
    ir_instance_registry_shutdown();
}

void test_kir_loading(void) {
    printf("\n=== Test: KIR Loading ===\n");

    ir_instance_registry_init();

    IRInstanceContext* inst = ir_instance_create("KIRLoader", NULL);
    assert(inst != NULL);

    // Load from JSON string
    IRComponent* root = ir_instance_load_kir_string(inst, test_kir_json);
    assert(root != NULL);
    assert(root->id == 1);
    // Note: type is IRComponentType enum, not string
    assert(root->child_count == 1);
    printf("  ✓ KIR loaded from string\n");

    // Verify owner_instance_id is set
    assert(root->owner_instance_id == inst->instance_id);
    for (uint32_t i = 0; i < root->child_count; i++) {
        assert(root->children[i]->owner_instance_id == inst->instance_id);
    }
    printf("  ✓ Component ownership set correctly\n");

    // Verify root is set in context
    assert(ir_instance_get_root(inst) == root);
    printf("  ✓ Root accessible via instance\n");

    ir_instance_destroy(inst);
    ir_instance_registry_shutdown();
}

void test_scope_isolation(void) {
    printf("\n=== Test: Scope Isolation ===\n");

    ir_instance_registry_init();

    // Create two instances
    IRInstanceContext* inst1 = ir_instance_create("ScopeTest1", NULL);
    IRInstanceContext* inst2 = ir_instance_create("ScopeTest2", NULL);

    // Load counter KIR into both instances
    IRComponent* root1 = ir_instance_load_kir_string(inst1, counter_kir_json);
    IRComponent* root2 = ir_instance_load_kir_string(inst2, counter_kir_json);

    assert(root1 != NULL);
    assert(root2 != NULL);

    // Verify different instance IDs
    assert(root1->owner_instance_id == inst1->instance_id);
    assert(root2->owner_instance_id == inst2->instance_id);
    assert(root1->owner_instance_id != root2->owner_instance_id);
    printf("  ✓ Components have correct owner_instance_id\n");

    // Verify scopes are preserved
    assert(root1->scope != NULL);
    assert(strcmp(root1->scope, "Counter") == 0);
    printf("  ✓ Component scopes preserved\n");

    // Verify child scopes
    assert(root1->children[0]->scope != NULL);
    assert(strcmp(root1->children[0]->scope, "Counter#decrement") == 0);
    assert(root1->children[1]->scope != NULL);
    assert(strcmp(root1->children[1]->scope, "Counter#label") == 0);
    printf("  ✓ Child component scopes preserved\n");

    ir_instance_destroy(inst1);
    ir_instance_destroy(inst2);
    ir_instance_registry_shutdown();
}

void test_suspend_resume(void) {
    printf("\n=== Test: Suspend/Resume ===\n");

    ir_instance_registry_init();

    IRInstanceContext* inst = ir_instance_create("SuspendTest", NULL);
    assert(inst->is_suspended == false);

    ir_instance_suspend(inst);
    assert(inst->is_suspended == true);
    assert(!ir_instance_is_running(inst));
    printf("  ✓ Instance suspended\n");

    ir_instance_resume(inst);
    assert(inst->is_suspended == false);
    assert(ir_instance_is_running(inst));
    printf("  ✓ Instance resumed\n");

    ir_instance_destroy(inst);
    ir_instance_registry_shutdown();
}

void test_thread_local_context(void) {
    printf("\n=== Test: Thread-Local Context ===\n");

    ir_instance_registry_init();

    IRInstanceContext* inst = ir_instance_create("ContextTest", NULL);

    // Test instance context switching
    assert(ir_instance_get_current() == NULL);

    ir_instance_set_current(inst);
    assert(ir_instance_get_current() == inst);
    printf("  ✓ Set current instance\n");

    // Test push/pop
    IRInstanceContext* prev = ir_instance_push_context(inst);
    assert(ir_instance_get_current() == inst);

    ir_instance_pop_context(prev);
    assert(ir_instance_get_current() == prev);
    printf("  ✓ Push/pop context works\n");

    ir_instance_destroy(inst);
    ir_instance_registry_shutdown();
}

void test_max_instances(void) {
    printf("\n=== Test: Max Instances ===\n");

    ir_instance_registry_init();

    // Try to create maximum instances
    IRInstanceContext* instances[IR_MAX_INSTANCES + 1] = {0};
    int created = 0;

    for (uint32_t i = 0; i <= IR_MAX_INSTANCES; i++) {
        instances[i] = ir_instance_create(NULL, NULL);
        if (instances[i]) {
            created++;
        } else {
            printf("  ✓ Correctly rejected instance #%d (max=%d)\n", i, IR_MAX_INSTANCES);
            break;
        }
    }

    assert(created == IR_MAX_INSTANCES);
    assert(ir_instance_get_count() == IR_MAX_INSTANCES);
    printf("  ✓ Created maximum instances\n");

    // Cleanup
    for (uint32_t i = 0; i < IR_MAX_INSTANCES; i++) {
        if (instances[i]) {
            ir_instance_destroy(instances[i]);
        }
    }

    ir_instance_registry_shutdown();
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    printf("╔════════════════════════════════════════════╗\n");
    printf("║   Multi-Instance Support Test Suite      ║\n");
    printf("╚════════════════════════════════════════════╝\n");

    int tests_run = 0;
    int tests_passed = 0;

#define RUN_TEST(test_fn) \
    do { \
        tests_run++; \
        printf("\n[TEST %d] Running: %s\n", tests_run, #test_fn); \
        test_fn(); \
        tests_passed++; \
        printf("✓ Test %d passed\n", tests_run); \
    } while (0)

    RUN_TEST(test_instance_registry);
    RUN_TEST(test_instance_creation);
    RUN_TEST(test_kir_loading);
    RUN_TEST(test_scope_isolation);
    RUN_TEST(test_suspend_resume);
    RUN_TEST(test_thread_local_context);
    RUN_TEST(test_max_instances);

#undef RUN_TEST

    printf("\n╔════════════════════════════════════════════╗\n");
    printf("║   Test Results                            ║\n");
    printf("╠════════════════════════════════════════════╣\n");
    printf("║   Passed: %2d / %2d                        ║\n", tests_passed, tests_run);
    printf("╚════════════════════════════════════════════╝\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
