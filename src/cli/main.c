/**
 * @file main.c
 * @brief Kryon CLI Main Entry Point
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 
 #define KRYON_VERSION "v1.0.0"
 
 extern int compile_command(int argc, char *argv[]);
 extern int decompile_command(int argc, char *argv[]);
 extern int run_command(int argc, char *argv[]);
 extern int dev_command(int argc, char *argv[]);
 extern int debug_command(int argc, char *argv[]);
 extern int package_command(int argc, char *argv[]);
 
 static void print_usage(const char *program_name) {
     printf("Kryon-C %s - Complete UI Framework\n", KRYON_VERSION);
     printf("Usage: %s <command> [options] [arguments]\n\n", program_name);
     printf("Commands:\n");
     printf("  compile <file.kry>     Compile KRY file to KRB binary\n");
     printf("  decompile <file.krb>   Decompile KRB binary to KIR format\n");
     printf("  run <file.krb>         Run KRB application\n");
     printf("  dev <file.kry>         Development mode with hot reload\n");
     printf("  debug <file.krb>       Debug KRB application\n");
     printf("  package <project>      Package for distribution\n");
     printf("  --help, -h             Show this help message\n");
     printf("  --version, -v          Show version information\n\n");
     printf("Examples:\n");
     printf("  %s compile hello-world.kry\n", program_name);
     printf("  %s run hello-world.krb\n", program_name);
     printf("  %s dev hello-world.kry --renderer=sdl2\n", program_name);
 }
 
 static void print_version(void) {
     printf("Kryon-C %s\n", KRYON_VERSION);
     printf("Platform: %s\n",
 #ifdef __linux__
            "Linux"
 #elif defined(__APPLE__)
            "macOS"
 #elif defined(_WIN32)
            "Windows"
 #elif defined(EMSCRIPTEN)
            "Web"
 #else
            "Unknown"
 #endif
     );
     printf("Built: %s %s\n", __DATE__, __TIME__);
 }
 
 int main(int argc, char *argv[]) {
     if (argc < 2) {
         print_usage(argv[0]);
         return 1;
     }
 
     const char *command = argv[1];
 
     if (strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
         print_usage(argv[0]);
         return 0;
     }
 
     if (strcmp(command, "--version") == 0 || strcmp(command, "-v") == 0) {
         print_version();
         return 0;
     }
 
     if (strcmp(command, "compile") == 0) {
         return compile_command(argc - 1, argv + 1);
    } else if (strcmp(command, "decompile") == 0) {
        return decompile_command(argc - 1, argv + 1);
     } else if (strcmp(command, "run") == 0) {
         return run_command(argc - 1, argv + 1);
     } else if (strcmp(command, "dev") == 0) {
         return dev_command(argc - 1, argv + 1);
     } else if (strcmp(command, "debug") == 0) {
         return debug_command(argc - 1, argv + 1);
     } else if (strcmp(command, "package") == 0) {
         return package_command(argc - 1, argv + 1);
     } else {
         fprintf(stderr, "Unknown command: %s\n", command);
         print_usage(argv[0]);
         return 1;
     }
 }
 