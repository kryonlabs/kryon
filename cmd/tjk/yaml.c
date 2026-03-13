/*
 * Kryon CLI - YAML Parser
 * C89 compliant
 *
 * Simple line-based YAML parser for kryon.yaml configuration
 * This is NOT a full YAML parser - it handles only the specific format
 * used by Kryon project configuration.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "cli.h"

/*
 * Trim leading whitespace from string
 */
static char* ltrim(char *str)
{
    while (isspace((unsigned char)*str)) {
        str++;
    }
    return str;
}

/*
 * Trim trailing whitespace from string
 */
static char* rtrim(char *str)
{
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        end--;
    }
    *(end + 1) = '\0';
    return str;
}

/*
 * Trim whitespace from both ends
 */
static char* trim(char *str)
{
    return rtrim(ltrim(str));
}

/*
 * Parse target string to flag
 */
static int parse_target(const char *str)
{
    if (strcmp(str, "linux") == 0) {
        return TARGET_LINUX;
    } else if (strcmp(str, "appimage") == 0) {
        return TARGET_APPIMAGE;
    } else if (strcmp(str, "wasm") == 0) {
        return TARGET_WASM;
    } else if (strcmp(str, "android") == 0) {
        return TARGET_ANDROID;
    }
    return 0;
}

/*
 * Parse kryon.yaml file
 * Returns 0 on success, -1 on error
 */
int cli_yaml_parse(const char *path, CLIKryonYaml *yaml)
{
    FILE *fp;
    char line[CLI_MAX_LINE];
    int line_num = 0;
    int in_targets = 0;
    int in_assets = 0;
    char *trimmed;
    char *colon;
    char *key;
    char *value;
    char *comment;
    char *item;

    /* Initialize output */
    memset(yaml, 0, sizeof(CLIKryonYaml));

    fp = fopen(path, "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(line, sizeof(line), fp) != NULL) {
        line_num++;

        /* Remove comments */
        comment = strchr(line, '#');
        if (comment != NULL) {
            *comment = '\0';
        }

        trimmed = trim(line);

        /* Skip empty lines */
        if (strlen(trimmed) == 0) {
            continue;
        }

        /* Check for section start */
        if (trimmed[0] == '-' && trimmed[1] == ' ') {
            /* List item */
            item = trim(trimmed + 2);

            if (in_targets) {
                int target = parse_target(item);
                if (target != 0) {
                    yaml->targets[yaml->target_count++] = target;
                }
            }
            continue;
        }

        /* Check for section headers */
        if (strcmp(trimmed, "targets:") == 0) {
            in_targets = 1;
            in_assets = 0;
            continue;
        } else if (strcmp(trimmed, "assets:") == 0) {
            in_assets = 1;
            in_targets = 0;
            continue;
        }

        /* Parse key: value pairs */
        colon = strchr(trimmed, ':');
        if (colon == NULL) {
            continue;  /* Skip lines without colon */
        }

        *colon = '\0';
        key = trim(trimmed);
        value = trim(colon + 1);

        /* Remove quotes from value if present */
        if (value[0] == '"' && value[strlen(value) - 1] == '"') {
            value++;
            value[strlen(value) - 1] = '\0';
            value = trim(value);
        }

        /* Parse known keys */
        if (strcmp(key, "name") == 0) {
            strncpy(yaml->name, value, sizeof(yaml->name) - 1);
        } else if (strcmp(key, "version") == 0) {
            strncpy(yaml->version, value, sizeof(yaml->version) - 1);
        } else if (strcmp(key, "kryon_version") == 0) {
            strncpy(yaml->kryon_version, value, sizeof(yaml->kryon_version) - 1);
        } else if (strcmp(key, "entry_point") == 0) {
            strncpy(yaml->entry_point, value, sizeof(yaml->entry_point) - 1);
        }
    }

    fclose(fp);

    /* Validate required fields */
    if (strlen(yaml->name) == 0) {
        return -1;  /* name is required */
    }

    return 0;
}

/*
 * Print default kryon.yaml content
 */
void cli_yaml_print_default(const char *project_name)
{
    printf("# Kryon Project Configuration\n");
    printf("name: %s\n", project_name ? project_name : "myapp");
    printf("version: 1.0.0\n");
    printf("kryon_version: \">=0.1.0\"\n");
    printf("entry_point: src/main.kry\n");
    printf("targets:\n");
    printf("  - linux\n");
    printf("assets:\n");
    printf("  - assets/images/*\n");
    printf("  - assets/fonts/*\n");
}
