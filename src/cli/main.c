/**

 * @file main.c
 * @brief Kryon CLI Main Entry Point
 */
#include "lib9.h"
#include <stdio.h>


 #define KRYON_VERSION "alpha"
 
 extern int compile_command(int argc, char *argv[]);
 extern int decompile_command(int argc, char *argv[]);
 extern int print_command(int argc, char *argv[]);
 extern int kir_dump_command(int argc, char *argv[]);
 extern int kir_validate_command(int argc, char *argv[]);
 extern int kir_stats_command(int argc, char *argv[]);
 extern int kir_diff_command(int argc, char *argv[]);
 extern int run_command(int argc, char *argv[]);
 extern int dev_command(int argc, char *argv[]);
 extern int debug_command(int argc, char *argv[]);
 extern int package_command(int argc, char *argv[]);
 extern int targets_command(int argc, char *argv[]);
 
 static void print_usage(const char *program_name) {
     fprintf(stderr, "Kryon-C %s - Complete UI Framework\n", KRYON_VERSION);
     fprintf(stderr, "Usage: %s <command> [options] [arguments]\n\n", program_name);
     fprintf(stderr, "Commands:\n");
     fprintf(stderr, "  compile <file.kry>     Compile KRY file to KRB binary\n");
     fprintf(stderr, "  decompile <file.krb>   Decompile KRB binary to KIR format\n");
     fprintf(stderr, "  print <file.kir>       Generate readable .kry source from KIR\n");
     fprintf(stderr, "  run <file.krb>         Run KRB application\n");
     fprintf(stderr, "  dev <file.kry>         Development mode with hot reload\n");
     fprintf(stderr, "  debug <file.krb>       Debug KRB application\n");
     fprintf(stderr, "  package <project>      Package for distribution\n");
     fprintf(stderr, "  targets                List available targets\n");
     fprintf(stderr, "\n");
     fprintf(stderr, "KIR Utilities:\n");
     fprintf(stderr, "  kir-dump <file.kir>    Pretty-print KIR structure\n");
     fprintf(stderr, "  kir-validate <file>    Validate KIR file structure\n");
     fprintf(stderr, "  kir-stats <file>       Show KIR statistics\n");
     fprintf(stderr, "  kir-diff <f1> <f2>     Compare two KIR files\n");
     fprintf(stderr, "  --help, -h             Show this help message\n");
     fprintf(stderr, "  --version, -v          Show version information\n\n");
     fprintf(stderr, "Examples:\n");
     fprintf(stderr, "  %s compile hello-world.kry\n", program_name);
     fprintf(stderr, "  %s run hello-world.krb\n", program_name);
     fprintf(stderr, "  %s dev hello-world.kry --renderer=sdl2\n", program_name);
 }
 
 static void print_version(void) {
     fprintf(stderr, "Kryon-C %s\n", KRYON_VERSION);
     fprintf(stderr, "Platform: %s\n",
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
     fprintf(stderr, "Built: %s %s\n", __DATE__, __TIME__);
 }
 
 void main(int argc, char *argv[]) {
     if (argc < 2) {
         print_usage(argv[0]);
         exits("error");
     }
 
     const char *command = argv[1];
 
     if (strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
         print_usage(argv[0]);
         exits(nil);
     }
 
     if (strcmp(command, "--version") == 0 || strcmp(command, "-v") == 0) {
         print_version();
         exits(nil);
     }
 
    int result;
    if (strcmp(command, "compile") == 0) {
        result = compile_command(argc - 1, argv + 1);
   } else if (strcmp(command, "decompile") == 0) {
       result = decompile_command(argc - 1, argv + 1);
   } else if (strcmp(command, "print") == 0) {
       result = print_command(argc - 1, argv + 1);
   } else if (strcmp(command, "kir-dump") == 0) {
       result = kir_dump_command(argc - 1, argv + 1);
   } else if (strcmp(command, "kir-validate") == 0) {
       result = kir_validate_command(argc - 1, argv + 1);
   } else if (strcmp(command, "kir-stats") == 0) {
       result = kir_stats_command(argc - 1, argv + 1);
   } else if (strcmp(command, "kir-diff") == 0) {
       result = kir_diff_command(argc - 1, argv + 1);
    } else if (strcmp(command, "run") == 0) {
        result = run_command(argc - 1, argv + 1);
    } else if (strcmp(command, "dev") == 0) {
        result = dev_command(argc - 1, argv + 1);
    } else if (strcmp(command, "debug") == 0) {
        result = debug_command(argc - 1, argv + 1);
    } else if (strcmp(command, "package") == 0) {
        result = package_command(argc - 1, argv + 1);
    } else if (strcmp(command, "targets") == 0) {
        result = targets_command(argc - 1, argv + 1);
    } else {
        fprint(2, "Unknown command: %s\n", command);
        print_usage(argv[0]);
        exits("unknown command");
    }
    exits(result == 0 ? nil : "command failed");
}
