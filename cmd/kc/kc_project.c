#include "kc_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

enum { KC_PROJECT_SCREEN_MAX = 512 };

typedef struct KcProjectRoute {
    char id[KC_NAME_MAX];
    char title[KC_NAME_MAX];
    char group[KC_NAME_MAX];
    char source_path[KC_PATH_MAX];
    const KryFile *page_file;
    const KryFunction *page;
} KcProjectRoute;

static void
kc_path_join(char *dst, size_t dst_size, const char *a, const char *b)
{
    if(dst_size == 0)
        return;
    if(a == NULL || a[0] == '\0') {
        snprintf(dst, dst_size, "%s", b != NULL ? b : "");
        return;
    }
    if(b == NULL || b[0] == '\0') {
        snprintf(dst, dst_size, "%s", a);
        return;
    }
    snprintf(dst, dst_size, "%s%s%s", a, a[strlen(a) - 1] == "/"[0] ? "" : "/",
             b);
}

static int
kc_mkdir_parent(const char *path)
{
    char tmp[KC_PATH_MAX];
    char *slash;

    if(path == NULL || path[0] == '\0')
        return 0;
    snprintf(tmp, sizeof(tmp), "%s", path);
    slash = strrchr(tmp, '/');
    if(slash == NULL)
        return 1;
    *slash = '\0';
    for(char *p = tmp + 1; *p != '\0'; p++) {
        if(*p == '/') {
            *p = '\0';
            if(mkdir(tmp, 0755) != 0 && errno != EEXIST)
                return 0;
            *p = '/';
        }
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

static void
kc_c_string(char *dst, size_t dst_size, const char *src)
{
    size_t n = 0;

    if(dst_size == 0)
        return;
    dst[n++] = '"';
    if(src != NULL) {
        for(const unsigned char *p = (const unsigned char *)src;
            *p != '\0' && n + 3 < dst_size; p++) {
            if(*p == '"' || *p == '\\') {
                dst[n++] = '\\';
                dst[n++] = (char)*p;
            } else if(*p == '\n') {
                dst[n++] = '\\';
                dst[n++] = 'n';
            } else {
                dst[n++] = (char)*p;
            }
        }
    }
    if(n + 1 < dst_size)
        dst[n++] = '"';
    dst[n] = '\0';
}

void
kc_function_base_name(char *dst, size_t dst_size, const KryFunction *fn)
{
    if(fn->exact_name)
        snprintf(dst, dst_size, "%s", fn->screen);
    else
        snprintf(dst, dst_size, "%s_kry_draw", fn->screen);
}

void
kc_function_name(char *dst, size_t dst_size, const KryFile *file,
                 const KryFunction *fn)
{
    char base[KC_NAME_MAX + 16];

    kc_function_base_name(base, sizeof(base), fn);
    if(file->module[0] != '\0' && !fn->global_name) {
        snprintf(dst, dst_size, "%s_%s", file->module, base);
        return;
    }
    snprintf(dst, dst_size, "%s", base);
}

static int
kc_function_takes_rectangle(const KryFunction *fn)
{
    return fn != NULL && strstr(fn->args, "Rectangle") != NULL;
}

static int
kc_function_takes_app_pointer(const KryFunction *fn)
{
    return fn != NULL && fn->args[0] != '\0' &&
           strstr(fn->args, "Rectangle") == NULL;
}

static int
kc_collect_project_routes(KryFile **files, int file_count,
                          KcProjectRoute *routes, int max_routes)
{
    int count = 0;
    int explicit_routes = 0;

    for(int i = 0; i < file_count; i++) {
        KryFile *file = files[i];

        if(file == NULL)
            continue;
        explicit_routes += file->route_count;
    }
    if(explicit_routes > 0) {
        for(int i = 0; i < file_count; i++) {
            KryFile *file = files[i];

            if(file == NULL)
                continue;
            for(int j = 0; j < file->route_count; j++) {
                const KryRoute *route = &file->routes[j];
                const KryFile *page_file = NULL;
                const KryFunction *page = NULL;

                for(int k = 0; k < file_count && page == NULL; k++) {
                    KryFile *candidate_file = files[k];

                    if(candidate_file == NULL)
                        continue;
                    for(int l = 0; l < candidate_file->function_count; l++) {
                        KryFunction *fn = &candidate_file->functions[l];

                        if(fn->is_public && !fn->exact_name &&
                           strcmp(fn->screen, route->page) == 0) {
                            page_file = candidate_file;
                            page = fn;
                            break;
                        }
                    }
                }
                if(page == NULL) {
                    fprintf(stderr, "%s: route '%s' references missing page '%s'\n",
                            file->display_path, route->id, route->page);
                    exit(1);
                }
                if(count >= max_routes) {
                    fprintf(stderr, "kc: too many project routes\n");
                    exit(1);
                }
                snprintf(routes[count].id, sizeof(routes[count].id), "%s",
                         route->id);
                snprintf(routes[count].title, sizeof(routes[count].title), "%s",
                         route->title);
                snprintf(routes[count].group, sizeof(routes[count].group), "%s",
                         route->group);
                snprintf(routes[count].source_path,
                         sizeof(routes[count].source_path), "%s",
                         route->source_path);
                routes[count].page_file = page_file;
                routes[count].page = page;
                count++;
            }
        }
        return count;
    }
    for(int i = 0; i < file_count; i++) {
        KryFile *file = files[i];

        if(file == NULL)
            continue;
        for(int j = 0; j < file->function_count; j++) {
            KryFunction *fn = &file->functions[j];

            if(!fn->is_public || fn->exact_name || fn->screen[0] == '\0')
                continue;
            if(count >= max_routes) {
                fprintf(stderr, "kc: too many project routes\n");
                exit(1);
            }
            snprintf(routes[count].id, sizeof(routes[count].id), "%s",
                     fn->screen);
            snprintf(routes[count].title, sizeof(routes[count].title), "%s",
                     fn->screen);
            snprintf(routes[count].group, sizeof(routes[count].group),
                     "Project");
            snprintf(routes[count].source_path,
                     sizeof(routes[count].source_path), "%s",
                     file->display_path);
            routes[count].page_file = file;
            routes[count].page = fn;
            count++;
        }
    }
    return count;
}

void
write_project_source(KryFile **files, int file_count, const char *root,
                     const char *out_dir)
{
    KcProjectRoute routes[KC_PROJECT_SCREEN_MAX];
    char path[KC_PATH_MAX];
    FILE *out;
    int count;

    (void)root;
    if(file_count <= 0)
        return;
    count = kc_collect_project_routes(files, file_count, routes,
                                      KC_PROJECT_SCREEN_MAX);
    if(count <= 0)
        return;
    kc_path_join(path, sizeof(path), out_dir, "kryon_project.c");
    if(!kc_mkdir_parent(path)) {
        fprintf(stderr, "%s: mkdir failed: %s\n", path, strerror(errno));
        exit(1);
    }
    out = fopen(path, "wb");
    if(out == NULL) {
        fprintf(stderr, "%s: open failed: %s\n", path, strerror(errno));
        exit(1);
    }

    fprintf(out, "/* Generated by kc from project Kry files. */\n");
    fprintf(out, "#include \"kryon_project.h\"\n");
    fprintf(out, "#include \"app_runtime.h\"\n\n");
    fprintf(out, "#if defined(__GNUC__) || defined(__clang__)\n");
    fprintf(out, "void *CreateApp(const char *project_path) __attribute__((weak));\n");
    fprintf(out, "void DestroyApp(void *app) __attribute__((weak));\n");
    fprintf(out, "void ApplyRoute(void *app, const AppRouteInfo *route) __attribute__((weak));\n");
    fprintf(out, "void BeginScreenDraw(void *app, Rectangle viewport) __attribute__((weak));\n");
    fprintf(out, "#else\n");
    fprintf(out, "void *CreateApp(const char *project_path);\n");
    fprintf(out, "void DestroyApp(void *app);\n");
    fprintf(out, "void ApplyRoute(void *app, const AppRouteInfo *route);\n");
    fprintf(out, "void BeginScreenDraw(void *app, Rectangle viewport);\n");
    fprintf(out, "#endif\n\n");

    for(int i = 0; i < count; i++) {
        char wrapper[KC_NAME_MAX + 32];
        char draw_name[KC_NAME_MAX + 16];

        snprintf(wrapper, sizeof(wrapper), "kryon_project_draw_%d", i);
        kc_function_name(draw_name, sizeof(draw_name), routes[i].page_file,
                         routes[i].page);
        fprintf(out, "static void\n%s(void *app, Rectangle viewport)\n{\n",
                wrapper);
        fprintf(out, "    if(BeginScreenDraw != 0)\n");
        fprintf(out, "        BeginScreenDraw(app, viewport);\n");
        if(kc_function_takes_rectangle(routes[i].page))
            fprintf(out, "    (void)app;\n    %s(viewport);\n", draw_name);
        else if(kc_function_takes_app_pointer(routes[i].page))
            fprintf(out, "    (void)viewport;\n    %s(app);\n", draw_name);
        else
            fprintf(out, "    (void)app;\n    (void)viewport;\n    %s();\n",
                    draw_name);
        fprintf(out, "}\n\n");
    }

    fprintf(out, "static const AppRouteInfo kryon_project_routes[%d];\n\n",
            count);
    fprintf(out, "static void\nkryon_project_enter(void *app, int index)\n{\n");
    fprintf(out, "    if(ApplyRoute == 0 || index < 0 || index >= %d)\n", count);
    fprintf(out, "        return;\n");
    fprintf(out, "    ApplyRoute(app, &kryon_project_routes[index]);\n");
    fprintf(out, "}\n\n");

    fprintf(out, "static const AppRouteInfo kryon_project_routes[] = {\n");
    for(int i = 0; i < count; i++) {
        char id[KC_NAME_MAX * 2];
        char title[KC_NAME_MAX * 2];
        char group[KC_NAME_MAX * 2];
        char source[KC_PATH_MAX * 2];
        char wrapper[KC_NAME_MAX + 32];

        kc_c_string(id, sizeof(id), routes[i].id);
        kc_c_string(title, sizeof(title), routes[i].title);
        kc_c_string(group, sizeof(group), routes[i].group);
        kc_c_string(source, sizeof(source), routes[i].source_path);
        snprintf(wrapper, sizeof(wrapper), "kryon_project_draw_%d", i);
        fprintf(out, "    {%s, %s, %s, %s, kryon_project_enter, %s},\n",
                id, group, title, source, wrapper);
    }
    fprintf(out, "};\n\n");
    fprintf(out, "static App kryon_project_runtime;\n");
    fprintf(out, "static AppHost kryon_project_host;\n\n");
    fprintf(out, "AppHost *\nCreateAppHost(int abi_version, const char *project_path)\n{\n");
    fprintf(out, "    void *app = 0;\n");
    fprintf(out, "    if(abi_version != APP_HOST_ABI_VERSION || CreateApp == 0)\n");
    fprintf(out, "        return 0;\n");
    fprintf(out, "    app = CreateApp(project_path);\n");
    fprintf(out, "    if(app == 0)\n");
    fprintf(out, "        return 0;\n");
    fprintf(out, "    kryon_project_runtime.app = app;\n");
    fprintf(out, "    kryon_project_runtime.routes = kryon_project_routes;\n");
    fprintf(out, "    kryon_project_runtime.route_count = (int)(sizeof(kryon_project_routes) / sizeof(kryon_project_routes[0]));\n");
    fprintf(out, "    kryon_project_runtime.selected_route = 0;\n");
    fprintf(out, "    BindAppHost(&kryon_project_runtime, &kryon_project_host);\n");
    fprintf(out, "    SetAppScreen(&kryon_project_host, 0);\n");
    fprintf(out, "    return &kryon_project_host;\n");
    fprintf(out, "}\n\n");
    fprintf(out, "void\nDestroyAppHost(AppHost *host)\n{\n");
    fprintf(out, "    if(host == 0 || host != &kryon_project_host)\n");
    fprintf(out, "        return;\n");
    fprintf(out, "    if(DestroyApp != 0 && kryon_project_runtime.app != 0)\n");
    fprintf(out, "        DestroyApp(kryon_project_runtime.app);\n");
    fprintf(out, "    kryon_project_runtime = (App){0};\n");
    fprintf(out, "    kryon_project_host = (AppHost){0};\n");
    fprintf(out, "}\n");
    fclose(out);
}
