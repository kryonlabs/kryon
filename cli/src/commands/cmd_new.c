/**
 * New Command
 * Creates new Kryon projects from templates
 */

#include "kryon_cli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Template: Basic Kry project
static const char* TEMPLATE_KRYON_TOML =
"[project]\n"
"name = \"%s\"\n"
"version = \"0.1.0\"\n"
"author = \"Your Name\"\n"
"description = \"A Kryon application\"\n"
"\n"
"[build]\n"
"target = \"web\"\n"
"output_dir = \"dist\"\n"
"entry = \"main.kry\"\n"
"frontend = \"kry\"\n"
"\n"
"[optimization]\n"
"enabled = true\n"
"minify_css = true\n"
"minify_js = true\n"
"tree_shake = true\n"
"\n"
"[dev]\n"
"hot_reload = true\n"
"port = 3000\n"
"auto_open = true\n";

static const char* TEMPLATE_MAIN_KRY =
"import { Text, Column, Button } from 'kryon';\n"
"\n"
"component App {\n"
"  Column(style = { padding: '20px', gap: '10px' }) {\n"
"    Text(style = { fontSize: '24px', fontWeight: 'bold' }) {\n"
"      \"Welcome to Kryon!\"\n"
"    }\n"
"    Text {\n"
"      \"Edit this file to get started.\"\n"
"    }\n"
"    Button(onClick = { || print(\"Hello!\") }) {\n"
"      \"Click me\"\n"
"    }\n"
"  }\n"
"}\n";

static const char* TEMPLATE_README =
"# %s\n"
"\n"
"A Kryon application.\n"
"\n"
"## Getting Started\n"
"\n"
"```bash\n"
"# Build the project\n"
"kryon build\n"
"\n"
"# Run development server\n"
"kryon dev main.kry\n"
"\n"
"# Build for production\n"
"kryon build --target=web\n"
"```\n"
"\n"
"## Project Structure\n"
"\n"
"- `main.kry` - Main application entry point\n"
"- `kryon.toml` - Project configuration\n"
"- `dist/` - Build output directory\n"
"\n"
"## Learn More\n"
"\n"
"- [Kryon Documentation](https://kryon.dev/docs)\n"
"- [Examples](https://github.com/kryonlabs/kryon/tree/main/examples)\n";

static const char* TEMPLATE_GITIGNORE =
"# Build outputs\n"
"dist/\n"
"build/\n"
"\n"
"# Cache\n"
".kryon_cache/\n"
"\n"
"# Dependencies\n"
"node_modules/\n"
"\n"
"# Editor\n"
".vscode/\n"
".idea/\n"
"*.swp\n"
"*~\n"
"\n"
"# OS\n"
".DS_Store\n"
"Thumbs.db\n";

int cmd_new(int argc, char** argv) {
    if (argc < 1) {
        fprintf(stderr, "Error: No project name specified\n");
        fprintf(stderr, "Usage: kryon new <project-name>\n");
        return 1;
    }

    const char* project_name = argv[0];

    // Validate project name
    if (strlen(project_name) == 0) {
        fprintf(stderr, "Error: Project name cannot be empty\n");
        return 1;
    }

    // Check if directory already exists
    if (file_exists(project_name)) {
        fprintf(stderr, "Error: Directory '%s' already exists\n", project_name);
        return 1;
    }

    printf("Creating new Kryon project: %s\n", project_name);

    // Create project directory
    if (!dir_create(project_name)) {
        fprintf(stderr, "Error: Failed to create project directory\n");
        return 1;
    }

    // Create kryon.toml
    char* toml_path = path_join(project_name, "kryon.toml");
    char toml_content[2048];
    snprintf(toml_content, sizeof(toml_content), TEMPLATE_KRYON_TOML, project_name);

    if (!file_write(toml_path, toml_content)) {
        fprintf(stderr, "Error: Failed to create kryon.toml\n");
        free(toml_path);
        return 1;
    }
    printf("  ✓ Created kryon.toml\n");
    free(toml_path);

    // Create main.kry
    char* kry_path = path_join(project_name, "main.kry");
    if (!file_write(kry_path, TEMPLATE_MAIN_KRY)) {
        fprintf(stderr, "Error: Failed to create main.kry\n");
        free(kry_path);
        return 1;
    }
    printf("  ✓ Created main.kry\n");
    free(kry_path);

    // Create README.md
    char* readme_path = path_join(project_name, "README.md");
    char readme_content[2048];
    snprintf(readme_content, sizeof(readme_content), TEMPLATE_README, project_name);

    if (!file_write(readme_path, readme_content)) {
        fprintf(stderr, "Error: Failed to create README.md\n");
        free(readme_path);
        return 1;
    }
    printf("  ✓ Created README.md\n");
    free(readme_path);

    // Create .gitignore
    char* gitignore_path = path_join(project_name, ".gitignore");
    if (!file_write(gitignore_path, TEMPLATE_GITIGNORE)) {
        fprintf(stderr, "Error: Failed to create .gitignore\n");
        free(gitignore_path);
        return 1;
    }
    printf("  ✓ Created .gitignore\n");
    free(gitignore_path);

    // Initialize git repository
    char git_cmd[1024];
    snprintf(git_cmd, sizeof(git_cmd), "cd \"%s\" && git init -q", project_name);
    if (system(git_cmd) == 0) {
        printf("  ✓ Initialized git repository\n");
    }

    printf("\n✓ Project created successfully!\n\n");
    printf("Next steps:\n");
    printf("  cd %s\n", project_name);
    printf("  kryon build        # Build the project\n");
    printf("  kryon dev main.kry  # Start development server\n");

    return 0;
}
