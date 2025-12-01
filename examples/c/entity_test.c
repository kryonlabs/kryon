/*
 * Entity System Test
 *
 * Tests entity component system, scenes, hierarchies, and queries
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../ir/ir_entity.h"

// ============================================================================
// Custom Component Example
// ============================================================================

typedef struct {
    int health;
    int max_health;
    int armor;
} HealthComponent;

typedef struct {
    float damage;
    float range;
    float cooldown;
} WeaponComponent;

#define IR_COMPONENT_HEALTH (IR_COMPONENT_CUSTOM_BASE + 1)
#define IR_COMPONENT_WEAPON (IR_COMPONENT_CUSTOM_BASE + 2)

// ============================================================================
// Callbacks
// ============================================================================

static void on_entity_create(IREntityID entity_id, void* user_data) {
    (void)user_data;
    printf("  [Callback] Entity created: %u\n", entity_id);
}

static void on_entity_destroy(IREntityID entity_id, void* user_data) {
    (void)user_data;
    printf("  [Callback] Entity destroyed: %u\n", entity_id);
}

static void on_component_add(IREntityID entity_id, IRComponentType type, void* user_data) {
    (void)user_data;
    printf("  [Callback] Component added to entity %u: type=%d\n", entity_id, type);
}

static void on_scene_activate(IRSceneID scene_id, void* user_data) {
    (void)user_data;
    printf("  [Callback] Scene activated: %u\n", scene_id);
}

// ============================================================================
// Test Functions
// ============================================================================

static void test_basic_entity_creation(void) {
    printf("\n=== Test 1: Basic Entity Creation ===\n");

    IREntityID player = ir_entity_create(IR_INVALID_SCENE, "Player");
    printf("Created player entity: %u\n", player);

    IREntity* player_entity = ir_entity_get(player);
    if (player_entity) {
        printf("  Name: %s\n", player_entity->name);
        printf("  Active: %s\n", player_entity->active ? "true" : "false");
        printf("  Visible: %s\n", player_entity->visible ? "true" : "false");
    }

    IREntityID enemy = ir_entity_create(IR_INVALID_SCENE, "Enemy");
    printf("Created enemy entity: %u\n", enemy);

    IREntityID pickup = ir_entity_create(IR_INVALID_SCENE, "HealthPickup");
    printf("Created pickup entity: %u\n", pickup);

    ir_entity_print_stats();
}

static void test_components(void) {
    printf("\n=== Test 2: Component System ===\n");

    IREntityID player = ir_entity_find_by_name("Player");
    if (player == IR_INVALID_ENTITY) {
        printf("ERROR: Player entity not found\n");
        return;
    }

    // Add transform component
    printf("Adding transform component...\n");
    ir_entity_add_transform(player, 100.0f, 200.0f, 0.0f);

    IRTransform* transform = ir_entity_get_transform(player);
    if (transform) {
        printf("  Position: (%.1f, %.1f)\n", transform->x, transform->y);
        printf("  Rotation: %.1f\n", transform->rotation);
        printf("  Scale: (%.1f, %.1f)\n", transform->scale_x, transform->scale_y);
    }

    // Add health component
    printf("Adding health component...\n");
    HealthComponent health = {
        .health = 100,
        .max_health = 100,
        .armor = 50
    };
    ir_entity_add_component(player, IR_COMPONENT_HEALTH, &health, sizeof(HealthComponent));

    HealthComponent* player_health = ir_entity_get_component(player, IR_COMPONENT_HEALTH);
    if (player_health) {
        printf("  Health: %d/%d\n", player_health->health, player_health->max_health);
        printf("  Armor: %d\n", player_health->armor);
    }

    // Add weapon component
    printf("Adding weapon component...\n");
    WeaponComponent weapon = {
        .damage = 25.0f,
        .range = 100.0f,
        .cooldown = 0.5f
    };
    ir_entity_add_component(player, IR_COMPONENT_WEAPON, &weapon, sizeof(WeaponComponent));

    WeaponComponent* player_weapon = ir_entity_get_component(player, IR_COMPONENT_WEAPON);
    if (player_weapon) {
        printf("  Damage: %.1f\n", player_weapon->damage);
        printf("  Range: %.1f\n", player_weapon->range);
        printf("  Cooldown: %.2f\n", player_weapon->cooldown);
    }

    printf("Player has %d components\n", ir_entity_get(player)->component_count);

    // Check component existence
    printf("Has transform: %s\n", ir_entity_has_component(player, IR_COMPONENT_TRANSFORM) ? "yes" : "no");
    printf("Has health: %s\n", ir_entity_has_component(player, IR_COMPONENT_HEALTH) ? "yes" : "no");
    printf("Has sprite: %s\n", ir_entity_has_component(player, IR_COMPONENT_SPRITE) ? "yes" : "no");
}

static void test_hierarchy(void) {
    printf("\n=== Test 3: Entity Hierarchy ===\n");

    // Create parent entity (spaceship)
    IREntityID spaceship = ir_entity_create(IR_INVALID_SCENE, "Spaceship");
    ir_entity_add_transform(spaceship, 400.0f, 300.0f, 0.0f);
    printf("Created spaceship: %u at (400, 300)\n", spaceship);

    // Create child entities (weapons)
    IREntityID left_gun = ir_entity_create(IR_INVALID_SCENE, "LeftGun");
    ir_entity_add_transform(left_gun, -20.0f, 0.0f, 0.0f);  // Relative position
    ir_entity_set_parent(left_gun, spaceship);
    printf("Created left gun: %u (child of spaceship)\n", left_gun);

    IREntityID right_gun = ir_entity_create(IR_INVALID_SCENE, "RightGun");
    ir_entity_add_transform(right_gun, 20.0f, 0.0f, 0.0f);  // Relative position
    ir_entity_set_parent(right_gun, spaceship);
    printf("Created right gun: %u (child of spaceship)\n", right_gun);

    IREntityID engine = ir_entity_create(IR_INVALID_SCENE, "Engine");
    ir_entity_add_transform(engine, 0.0f, 30.0f, 0.0f);  // Relative position
    ir_entity_set_parent(engine, spaceship);
    printf("Created engine: %u (child of spaceship)\n", engine);

    // Query hierarchy
    uint32_t child_count = ir_entity_get_child_count(spaceship);
    printf("\nSpaceship has %u children:\n", child_count);

    IREntityID children[IR_MAX_CHILDREN_PER_ENTITY];
    uint32_t count = ir_entity_get_children(spaceship, children, IR_MAX_CHILDREN_PER_ENTITY);
    for (uint32_t i = 0; i < count; i++) {
        IREntity* child = ir_entity_get(children[i]);
        if (child) {
            printf("  - %s (ID: %u)\n", child->name, child->id);
        }
    }

    printf("\nDestroying spaceship (should destroy children)...\n");
    ir_entity_destroy(spaceship);

    ir_entity_print_stats();
}

static void test_scenes(void) {
    printf("\n=== Test 4: Scene Management ===\n");

    // Create main menu scene
    IRSceneID menu_scene = ir_scene_create("MainMenu");
    printf("Created menu scene: %u\n", menu_scene);

    IREntityID play_button = ir_entity_create(menu_scene, "PlayButton");
    ir_entity_add_transform(play_button, 640.0f, 300.0f, 0.0f);
    printf("Created play button in menu scene\n");

    IREntityID quit_button = ir_entity_create(menu_scene, "QuitButton");
    ir_entity_add_transform(quit_button, 640.0f, 400.0f, 0.0f);
    printf("Created quit button in menu scene\n");

    // Create gameplay scene
    IRSceneID game_scene = ir_scene_create("Gameplay");
    printf("Created gameplay scene: %u\n", game_scene);

    // Switch to gameplay scene
    printf("\nSwitching to gameplay scene...\n");
    ir_scene_set_active(game_scene);

    IREntityID player = ir_entity_create(IR_INVALID_SCENE, "Player");
    ir_entity_add_transform(player, 400.0f, 500.0f, 0.0f);
    printf("Created player in gameplay scene\n");

    IREntityID enemy1 = ir_entity_create(IR_INVALID_SCENE, "Enemy1");
    IREntityID enemy2 = ir_entity_create(IR_INVALID_SCENE, "Enemy2");
    IREntityID enemy3 = ir_entity_create(IR_INVALID_SCENE, "Enemy3");
    printf("Created 3 enemies in gameplay scene\n");

    // Query entities per scene
    printf("\nEntities in menu scene:\n");
    IREntityID menu_entities[IR_MAX_ENTITIES];
    uint32_t menu_count = ir_scene_get_entities(menu_scene, menu_entities, IR_MAX_ENTITIES);
    for (uint32_t i = 0; i < menu_count; i++) {
        IREntity* entity = ir_entity_get(menu_entities[i]);
        if (entity) {
            printf("  - %s (ID: %u)\n", entity->name, entity->id);
        }
    }

    printf("\nEntities in gameplay scene:\n");
    IREntityID game_entities[IR_MAX_ENTITIES];
    uint32_t game_count = ir_scene_get_entities(game_scene, game_entities, IR_MAX_ENTITIES);
    for (uint32_t i = 0; i < game_count; i++) {
        IREntity* entity = ir_entity_get(game_entities[i]);
        if (entity) {
            printf("  - %s (ID: %u)\n", entity->name, entity->id);
        }
    }

    printf("\nActive scene: %u\n", ir_scene_get_active());
    ir_entity_print_stats();
}

static void test_queries(void) {
    printf("\n=== Test 5: Entity Queries ===\n");

    // Add transform components to some entities
    IREntityID player = ir_entity_find_by_name("Player");
    if (player != IR_INVALID_ENTITY) {
        ir_entity_add_transform(player, 100.0f, 100.0f, 0.0f);
    }

    IREntityID enemy1 = ir_entity_find_by_name("Enemy1");
    if (enemy1 != IR_INVALID_ENTITY) {
        ir_entity_add_transform(enemy1, 200.0f, 150.0f, 0.0f);
    }

    IREntityID enemy2 = ir_entity_find_by_name("Enemy2");
    if (enemy2 != IR_INVALID_ENTITY) {
        ir_entity_add_transform(enemy2, 300.0f, 200.0f, 0.0f);
    }

    // Query by component type
    printf("Querying entities with transform component...\n");
    IREntityQuery query;
    ir_entity_query_by_component(IR_COMPONENT_TRANSFORM, &query);
    printf("Found %u entities with transform:\n", query.count);
    for (uint32_t i = 0; i < query.count; i++) {
        IREntity* entity = ir_entity_get(query.entities[i]);
        if (entity) {
            IRTransform* transform = ir_entity_get_transform(entity->id);
            printf("  - %s at (%.1f, %.1f)\n", entity->name, transform->x, transform->y);
        }
    }

    // Query active entities
    printf("\nQuerying active entities...\n");
    ir_entity_query_active(&query);
    printf("Found %u active entities\n", query.count);

    // Query visible entities
    printf("\nQuerying visible entities...\n");
    ir_entity_query_visible(&query);
    printf("Found %u visible entities\n", query.count);

    // Set some entities inactive and query again
    if (enemy1 != IR_INVALID_ENTITY) {
        ir_entity_set_active(enemy1, false);
        printf("\nSet Enemy1 inactive\n");
    }

    ir_entity_query_active(&query);
    printf("Active entities after change: %u\n", query.count);
}

static void test_find_by_name(void) {
    printf("\n=== Test 6: Find by Name ===\n");

    const char* names[] = {"Player", "Enemy1", "PlayButton", "QuitButton", "NonExistent"};
    for (int i = 0; i < 5; i++) {
        IREntityID id = ir_entity_find_by_name(names[i]);
        if (id != IR_INVALID_ENTITY) {
            IREntity* entity = ir_entity_get(id);
            printf("Found '%s': ID=%u, Scene=%u\n", names[i], id, entity->scene_id);
        } else {
            printf("Entity '%s' not found\n", names[i]);
        }
    }
}

// ============================================================================
// Main
// ============================================================================

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("=== Kryon Entity System Test ===\n\n");

    // Initialize entity system
    printf("Initializing entity system...\n");
    if (!ir_entity_init()) {
        fprintf(stderr, "Failed to initialize entity system\n");
        return 1;
    }
    printf("✓ Entity system initialized\n");

    // Set up callbacks
    printf("Setting up callbacks...\n");
    ir_entity_set_create_callback(on_entity_create, NULL);
    ir_entity_set_destroy_callback(on_entity_destroy, NULL);
    ir_entity_set_component_add_callback(on_component_add, NULL);
    ir_entity_set_scene_activate_callback(on_scene_activate, NULL);
    printf("✓ Callbacks registered\n");

    // Create default scene
    IRSceneID default_scene = ir_scene_create("DefaultScene");
    printf("✓ Default scene created: %u\n", default_scene);

    // Run tests
    test_basic_entity_creation();
    test_components();
    test_hierarchy();
    test_scenes();
    test_queries();
    test_find_by_name();

    // Final statistics
    printf("\n=== Final Statistics ===\n");
    ir_entity_print_stats();

    // Cleanup
    printf("\nShutting down entity system...\n");
    ir_entity_shutdown();
    printf("✓ Entity system shutdown complete\n");

    printf("\n=== Test completed successfully ===\n");
    return 0;
}
