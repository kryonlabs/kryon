/**
 * @file inferno_features.c
 * @brief Example demonstrating Inferno platform services
 *
 * This example shows how to use Inferno-specific features in a kryon
 * application while maintaining portability. The app detects if Inferno
 * services are available and uses them when present.
 *
 * Build:
 *   make -f Makefile.inferno
 *
 * Run:
 *   ./build/bin/kryon run examples/inferno_features.kry
 *
 * Features demonstrated:
 * - Service availability detection
 * - Extended file I/O (device files)
 * - Process enumeration
 * - Graceful fallback when services unavailable
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "platform_services.h"
#include "services/extended_file_io.h"
#include "services/process_control.h"
#include "services/namespace.h"

// =============================================================================
// DEMO: Extended File I/O
// =============================================================================

void demo_extended_file_io(void) {
    printf("\n=== Extended File I/O Demo ===\n");

    if (!kryon_services_available(KRYON_SERVICE_EXTENDED_FILE_IO)) {
        printf("Extended File I/O not available (not running on Inferno)\n");
        return;
    }

    KryonExtendedFileIO *xfio = kryon_services_get_interface(
        KRYON_SERVICE_EXTENDED_FILE_IO
    );

    // Demo 1: Device file detection
    printf("\nDevice file detection:\n");
    const char *paths[] = {
        "#c/cons",
        "/prog/self/status",
        "/tmp/test.txt",
        NULL
    };

    for (int i = 0; paths[i]; i++) {
        bool is_dev = xfio->is_device(paths[i]);
        bool is_special = xfio->is_special(paths[i]);
        const char *type = xfio->get_type(paths[i]);

        printf("  %s: device=%s, special=%s, type=%s\n",
               paths[i],
               is_dev ? "yes" : "no",
               is_special ? "yes" : "no",
               type ? type : "none");
    }

    // Demo 2: Read from device file (console)
    printf("\nReading from #c/cons:\n");
    KryonExtendedFile *cons = xfio->open("#c/cons", KRYON_O_RDWR);
    if (cons) {
        // Write prompt
        const char *prompt = "Type something (or press Enter to skip): ";
        xfio->write(cons, prompt, strlen(prompt));

        // Read response
        char buf[256];
        int n = xfio->read(cons, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("  You typed: %s", buf);
        }

        xfio->close(cons);
    } else {
        printf("  Failed to open #c/cons\n");
    }

    // Demo 3: Read process status
    printf("\nReading /prog/self/status:\n");
    KryonExtendedFile *status = xfio->open("/prog/self/status", KRYON_O_RDONLY);
    if (status) {
        char buf[512];
        int n = xfio->read(status, buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = '\0';
            printf("  Status: %s\n", buf);
        }
        xfio->close(status);
    } else {
        printf("  Failed to open /prog/self/status\n");
    }

    // Demo 4: Extended stat (Qid information)
    printf("\nExtended stat for #c/cons:\n");
    KryonExtendedFileStat stat;
    if (xfio->stat_extended("#c/cons", &stat)) {
        printf("  Size: %lu bytes\n", stat.size);
        printf("  Mode: 0%o\n", stat.mode);
        printf("  Device: %s\n", stat.is_device ? "yes" : "no");
        printf("  Qid type: 0x%02x\n", stat.qid_type);
        printf("  Qid version: %u\n", stat.qid_version);
        printf("  Qid path: 0x%lx\n", stat.qid_path);
    }
}

// =============================================================================
// DEMO: Process Control
// =============================================================================

void demo_process_control(void) {
    printf("\n=== Process Control Demo ===\n");

    if (!kryon_services_available(KRYON_SERVICE_PROCESS_CONTROL)) {
        printf("Process Control not available (not running on Inferno)\n");
        return;
    }

    KryonProcessControlService *pcs = kryon_services_get_interface(
        KRYON_SERVICE_PROCESS_CONTROL
    );

    // Demo 1: Get current process info
    printf("\nCurrent process:\n");
    int my_pid = pcs->get_current_pid();
    int parent_pid = pcs->get_parent_pid();
    printf("  PID: %d\n", my_pid);
    printf("  Parent PID: %d\n", parent_pid);

    KryonProcessInfo my_info;
    if (pcs->get_process_info(my_pid, &my_info)) {
        printf("  Name: %s\n", my_info.name);
        printf("  State: ");
        switch (my_info.state) {
            case KRYON_PROC_RUNNING:
                printf("Running\n");
                break;
            case KRYON_PROC_SLEEPING:
                printf("Sleeping\n");
                break;
            case KRYON_PROC_STOPPED:
                printf("Stopped\n");
                break;
            case KRYON_PROC_DEAD:
                printf("Dead\n");
                break;
            default:
                printf("Unknown\n");
                break;
        }
        printf("  Memory: %ld bytes\n", my_info.memory);

        pcs->free_process_info(&my_info);
    }

    // Demo 2: List all processes
    printf("\nAll processes (first 10):\n");
    KryonProcessInfo *procs;
    int count;

    if (pcs->list_processes(&procs, &count)) {
        printf("  Total processes: %d\n", count);

        int display_count = count < 10 ? count : 10;
        for (int i = 0; i < display_count; i++) {
            printf("  %5d  %-20s  %8ld bytes\n",
                   procs[i].pid,
                   procs[i].name,
                   procs[i].memory);
        }

        if (count > 10) {
            printf("  ... and %d more\n", count - 10);
        }

        pcs->free_process_list(procs, count);
    }

    // Demo 3: Read detailed status
    printf("\nDetailed status for current process:\n");
    char *status = pcs->read_status(my_pid);
    if (status) {
        printf("  %s\n", status);
        pcs->free_status(status);
    }
}

// =============================================================================
// DEMO: Namespace Operations
// =============================================================================

void demo_namespace(void) {
    printf("\n=== Namespace Operations Demo ===\n");

    if (!kryon_services_available(KRYON_SERVICE_NAMESPACE)) {
        printf("Namespace service not available (not running on Inferno)\n");
        return;
    }

    KryonNamespaceService *ns = kryon_services_get_interface(
        KRYON_SERVICE_NAMESPACE
    );

    printf("\nNamespace operations:\n");
    printf("  bind(), mount(), unmount() are available\n");
    printf("  9P server creation is available (implementation pending)\n");

    // Note: Actual mount/bind operations would require appropriate
    // permissions and setup. This demo just shows the API is available.
}

// =============================================================================
// MAIN
// =============================================================================

int main(int argc, char **argv) {
    printf("Kryon Inferno Platform Services Demo\n");
    printf("=====================================\n");

    // Initialize services
    kryon_services_init();

    // Check if any plugin registered
    KryonPlatformServices *plugin = kryon_services_get();
    if (plugin) {
        printf("\nPlatform plugin: %s v%s\n",
               plugin->name,
               plugin->version ? plugin->version : "unknown");
    } else {
        printf("\nNo platform plugin registered\n");
        printf("This demo requires Inferno-specific features.\n");
        printf("\nTo build with Inferno support:\n");
        printf("  make -f Makefile.inferno\n");
        printf("\nThe app will now demonstrate graceful fallback.\n");
    }

    // Run demos (each checks service availability)
    demo_extended_file_io();
    demo_process_control();
    demo_namespace();

    // Shutdown services
    kryon_services_shutdown();

    printf("\n=== Demo Complete ===\n");
    printf("\nKey Takeaways:\n");
    printf("- Services are detected at runtime\n");
    printf("- Apps gracefully handle missing services\n");
    printf("- Same code works on all platforms\n");
    printf("- Inferno features are opt-in, not required\n");

    return 0;
}
