/**
 * @file package_command.c
 * @brief Kryon Package Command Implementation
 */

#include "internal/memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

static bool create_directory(const char *path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        return mkdir(path, 0755) == 0;
    }
    return S_ISDIR(st.st_mode);
}

static bool file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static bool copy_file(const char *src, const char *dst) {
    FILE *src_file = fopen(src, "rb");
    if (!src_file) return false;
    
    FILE *dst_file = fopen(dst, "wb");
    if (!dst_file) {
        fclose(src_file);
        return false;
    }
    
    char buffer[4096];
    size_t bytes;
    bool success = true;
    
    while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
        if (fwrite(buffer, 1, bytes, dst_file) != bytes) {
            success = false;
            break;
        }
    }
    
    fclose(src_file);
    fclose(dst_file);
    return success;
}

static bool create_launcher_script(const char *output_dir, const char *app_name, 
                                  const char *main_krb, const char *platform) {
    char script_path[512];
    const char *extension = "";
    
    if (strcmp(platform, "windows") == 0) {
        extension = ".bat";
        snprintf(script_path, sizeof(script_path), "%s/%s%s", output_dir, app_name, extension);
        
        FILE *script = fopen(script_path, "w");
        if (!script) return false;
        
        fprintf(script, "@echo off\n");
        fprintf(script, "rem Kryon Application Launcher\n");
        fprintf(script, "set KRYON_PATH=%%~dp0\n");
        fprintf(script, "\"%%KRYON_PATH%%\\kryon.exe\" run \"%%KRYON_PATH%%\\%s\" %%*\n", main_krb);
        fprintf(script, "pause\n");
        
        fclose(script);
    } else {
        snprintf(script_path, sizeof(script_path), "%s/%s", output_dir, app_name);
        
        FILE *script = fopen(script_path, "w");
        if (!script) return false;
        
        fprintf(script, "#!/bin/bash\n");
        fprintf(script, "# Kryon Application Launcher\n");
        fprintf(script, "KRYON_PATH=\"$(cd \"$(dirname \"${BASH_SOURCE[0]}\")\" && pwd)\"\n");
        fprintf(script, "\"$KRYON_PATH/kryon\" run \"$KRYON_PATH/%s\" \"$@\"\n", main_krb);
        
        fclose(script);
        
        // Make executable
        chmod(script_path, 0755);
    }
    
    return true;
}

static bool create_package_info(const char *output_dir, const char *app_name, 
                               const char *version, const char *description,
                               const char *platform) {
    char info_path[512];
    snprintf(info_path, sizeof(info_path), "%s/package.json", output_dir);
    
    FILE *info = fopen(info_path, "w");
    if (!info) return false;
    
    fprintf(info, "{\n");
    fprintf(info, "  \"name\": \"%s\",\n", app_name);
    fprintf(info, "  \"version\": \"%s\",\n", version);
    fprintf(info, "  \"description\": \"%s\",\n", description);
    fprintf(info, "  \"platform\": \"%s\",\n", platform);
    fprintf(info, "  \"engine\": \"kryon-c\",\n");
    fprintf(info, "  \"engine_version\": \"1.0.0\",\n");
    fprintf(info, "  \"package_time\": %ld,\n", time(NULL));
    fprintf(info, "  \"main\": \"app.krb\",\n");
    fprintf(info, "  \"launcher\": \"%s\"\n", app_name);
    fprintf(info, "}\n");
    
    fclose(info);
    return true;
}

static bool create_installer_script(const char *output_dir, const char *app_name, 
                                   const char *platform) {
    if (strcmp(platform, "linux") != 0) {
        return true; // Only create for Linux for now
    }
    
    char script_path[512];
    snprintf(script_path, sizeof(script_path), "%s/install.sh", output_dir);
    
    FILE *script = fopen(script_path, "w");
    if (!script) return false;
    
    fprintf(script, "#!/bin/bash\n");
    fprintf(script, "# Kryon Application Installer\n");
    fprintf(script, "set -e\n\n");
    
    fprintf(script, "APP_NAME=\"%s\"\n", app_name);
    fprintf(script, "INSTALL_DIR=\"/opt/kryon/$APP_NAME\"\n");
    fprintf(script, "DESKTOP_FILE=\"/usr/share/applications/$APP_NAME.desktop\"\n");
    fprintf(script, "BIN_LINK=\"/usr/local/bin/$APP_NAME\"\n\n");
    
    fprintf(script, "echo \"Installing $APP_NAME...\"\n\n");
    
    fprintf(script, "# Check for root privileges\n");
    fprintf(script, "if [ \"$EUID\" -ne 0 ]; then\n");
    fprintf(script, "    echo \"Please run as root (sudo $0)\"\n");
    fprintf(script, "    exit 1\n");
    fprintf(script, "fi\n\n");
    
    fprintf(script, "# Create installation directory\n");
    fprintf(script, "mkdir -p \"$INSTALL_DIR\"\n\n");
    
    fprintf(script, "# Copy files\n");
    fprintf(script, "cp -r * \"$INSTALL_DIR/\"\n\n");
    
    fprintf(script, "# Create binary symlink\n");
    fprintf(script, "ln -sf \"$INSTALL_DIR/$APP_NAME\" \"$BIN_LINK\"\n\n");
    
    fprintf(script, "# Create desktop entry\n");
    fprintf(script, "cat > \"$DESKTOP_FILE\" << EOF\n");
    fprintf(script, "[Desktop Entry]\n");
    fprintf(script, "Name=%s\n", app_name);
    fprintf(script, "Exec=$INSTALL_DIR/$APP_NAME\n");
    fprintf(script, "Icon=$INSTALL_DIR/icon.png\n");
    fprintf(script, "Type=Application\n");
    fprintf(script, "Categories=Application;\n");
    fprintf(script, "EOF\n\n");
    
    fprintf(script, "echo \"Installation complete!\"\n");
    fprintf(script, "echo \"Run '$APP_NAME' from terminal or find it in applications menu.\"\n");
    
    fclose(script);
    chmod(script_path, 0755);
    
    return true;
}

int package_command(int argc, char *argv[]) {
    const char *build_dir = "build";
    const char *output_dir = "dist";
    const char *app_name = "app";
    const char *version = "1.0.0";
    const char *description = "Kryon Application";
    const char *platform = 
#ifdef __linux__
        "linux"
#elif defined(__APPLE__)
        "macos"
#elif defined(_WIN32)
        "windows"
#else
        "unknown"
#endif
    ;
    bool verbose = false;
    bool create_installer = false;
    bool bundle_runtime = true;
    
    // Parse arguments
    static struct option long_options[] = {
        {"build", required_argument, 0, 'b'},
        {"output", required_argument, 0, 'o'},
        {"name", required_argument, 0, 'n'},
        {"version", required_argument, 0, 'V'},
        {"description", required_argument, 0, 'd'},
        {"platform", required_argument, 0, 'p'},
        {"installer", no_argument, 0, 'i'},
        {"no-runtime", no_argument, 0, 'R'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int c;
    while ((c = getopt_long(argc, argv, "b:o:n:V:d:p:iRvh", long_options, NULL)) != -1) {
        switch (c) {
            case 'b':
                build_dir = optarg;
                break;
            case 'o':
                output_dir = optarg;
                break;
            case 'n':
                app_name = optarg;
                break;
            case 'V':
                version = optarg;
                break;
            case 'd':
                description = optarg;
                break;
            case 'p':
                platform = optarg;
                break;
            case 'i':
                create_installer = true;
                break;
            case 'R':
                bundle_runtime = false;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                printf("Usage: kryon package [options]\n");
                printf("Options:\n");
                printf("  -b, --build <dir>       Build directory (default: build)\n");
                printf("  -o, --output <dir>      Output package directory (default: dist)\n");
                printf("  -n, --name <name>       Application name (default: app)\n");
                printf("  -V, --version <ver>     Version string (default: 1.0.0)\n");
                printf("  -d, --description <txt> Application description\n");
                printf("  -p, --platform <name>   Target platform (auto-detected)\n");
                printf("  -i, --installer         Create installer script\n");
                printf("  -R, --no-runtime        Don't bundle Kryon runtime\n");
                printf("  -v, --verbose           Verbose output\n");
                printf("  -h, --help              Show this help\n");
                printf("\nCreates a distributable package from built KRB files.\n");
                return 0;
            case '?':
                return 1;
            default:
                abort();
        }
    }
    
    printf("📦 Kryon Package Creator\n");
    printf("Build directory: %s\n", build_dir);
    printf("Output directory: %s\n", output_dir);
    printf("Application: %s v%s\n", app_name, version);
    printf("Platform: %s\n", platform);
    printf("Bundle runtime: %s\n", bundle_runtime ? "yes" : "no");
    
    // Check if build directory exists
    struct stat st;
    if (stat(build_dir, &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Error: Build directory '%s' does not exist\n", build_dir);
        fprintf(stderr, "Run 'kryon build' first to create built files.\n");
        return 1;
    }
    
    // Create output directory
    if (verbose) {
        printf("📁 Creating package directory...\n");
    }
    
    if (!create_directory(output_dir)) {
        fprintf(stderr, "Error: Cannot create output directory: %s\n", output_dir);
        return 1;
    }
    
    // Look for main KRB file
    char main_krb[256];
    const char *candidates[] = {"app.krb", "main.krb", "index.krb", NULL};
    bool found_main = false;
    
    for (int i = 0; candidates[i]; i++) {
        snprintf(main_krb, sizeof(main_krb), "%s/%s", build_dir, candidates[i]);
        if (file_exists(main_krb)) {
            strcpy(main_krb, candidates[i]);
            found_main = true;
            break;
        }
    }
    
    if (!found_main) {
        fprintf(stderr, "Error: No main KRB file found in build directory\n");
        fprintf(stderr, "Looking for: app.krb, main.krb, or index.krb\n");
        return 1;
    }
    
    if (verbose) {
        printf("📄 Main application: %s\n", main_krb);
    }
    
    // Copy build files to package
    if (verbose) {
        printf("📋 Copying application files...\n");
    }
    
    char copy_cmd[1024];
    snprintf(copy_cmd, sizeof(copy_cmd), "cp -r %s/* %s/", build_dir, output_dir);
    if (system(copy_cmd) != 0) {
        fprintf(stderr, "Error: Failed to copy build files\n");
        return 1;
    }
    
    // Bundle Kryon runtime if requested
    if (bundle_runtime) {
        if (verbose) {
            printf("🚀 Bundling Kryon runtime...\n");
        }
        
        // Copy kryon executable
        char kryon_src[256];
        char kryon_dst[512];
        
        // Try to find kryon executable
        const char *kryon_paths[] = {
            "/usr/local/bin/kryon",
            "/usr/bin/kryon",
            "./bin/kryon",
            "./kryon",
            NULL
        };
        
        bool found_kryon = false;
        for (int i = 0; kryon_paths[i]; i++) {
            if (file_exists(kryon_paths[i])) {
                strcpy(kryon_src, kryon_paths[i]);
                found_kryon = true;
                break;
            }
        }
        
        if (found_kryon) {
            snprintf(kryon_dst, sizeof(kryon_dst), "%s/kryon", output_dir);
            if (copy_file(kryon_src, kryon_dst)) {
                chmod(kryon_dst, 0755);
                if (verbose) {
                    printf("  ✅ Runtime copied: %s\n", kryon_src);
                }
            } else {
                fprintf(stderr, "Warning: Failed to copy Kryon runtime\n");
            }
        } else {
            fprintf(stderr, "Warning: Kryon runtime not found, package may not be self-contained\n");
        }
    }
    
    // Create launcher script
    if (verbose) {
        printf("🔧 Creating launcher script...\n");
    }
    
    if (!create_launcher_script(output_dir, app_name, main_krb, platform)) {
        fprintf(stderr, "Warning: Failed to create launcher script\n");
    }
    
    // Create package metadata
    if (verbose) {
        printf("📋 Creating package metadata...\n");
    }
    
    if (!create_package_info(output_dir, app_name, version, description, platform)) {
        fprintf(stderr, "Warning: Failed to create package metadata\n");
    }
    
    // Create installer if requested
    if (create_installer) {
        if (verbose) {
            printf("🔧 Creating installer script...\n");
        }
        
        if (!create_installer_script(output_dir, app_name, platform)) {
            fprintf(stderr, "Warning: Failed to create installer script\n");
        }
    }
    
    // Create README
    char readme_path[512];
    snprintf(readme_path, sizeof(readme_path), "%s/README.txt", output_dir);
    
    FILE *readme = fopen(readme_path, "w");
    if (readme) {
        fprintf(readme, "%s v%s\n", app_name, version);
        fprintf(readme, "%s\n", description);
        fprintf(readme, "\n");
        fprintf(readme, "This is a Kryon application package.\n");
        fprintf(readme, "\n");
        fprintf(readme, "To run:\n");
        if (strcmp(platform, "windows") == 0) {
            fprintf(readme, "  Double-click %s.bat\n", app_name);
        } else {
            fprintf(readme, "  ./%s\n", app_name);
        }
        fprintf(readme, "\n");
        if (create_installer) {
            fprintf(readme, "To install system-wide:\n");
            if (strcmp(platform, "linux") == 0) {
                fprintf(readme, "  sudo ./install.sh\n");
            }
            fprintf(readme, "\n");
        }
        fprintf(readme, "Generated by Kryon-C v1.0.0\n");
        fprintf(readme, "Platform: %s\n", platform);
        fprintf(readme, "Packaged: %s", ctime(&(time_t){time(NULL)}));
        
        fclose(readme);
    }
    
    // Calculate package size
    char size_cmd[512];
    snprintf(size_cmd, sizeof(size_cmd), "du -sh %s", output_dir);
    
    printf("\n✅ Package created successfully!\n");
    printf("📊 Package summary:\n");
    printf("  Name: %s v%s\n", app_name, version);
    printf("  Platform: %s\n", platform);
    printf("  Location: %s/\n", output_dir);
    printf("  Main file: %s\n", main_krb);
    printf("  Runtime bundled: %s\n", bundle_runtime ? "yes" : "no");
    printf("  Installer: %s\n", create_installer ? "yes" : "no");
    
    printf("\n📦 Package contents:\n");
    system(size_cmd);
    
    printf("\n🚀 To distribute:\n");
    printf("  tar -czf %s-%s-%s.tar.gz -C %s .\n", app_name, version, platform, output_dir);
    
    return 0;
}