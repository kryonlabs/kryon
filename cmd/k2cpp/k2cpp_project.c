/*
 * k2cpp_project.c — emit kryon_project.h/.c after all files lower (the
 * project files stay plain C; the generated module headers are .hpp but
 * self-guard their extern "C" so C translation units include them too). The
 * app-host ABI (CreateAppHost/DestroyAppHost + route table) consumed by
 * the preview tool and IDE host builds. Routes auto-collect from #ui
 * functions. Explicit route{} blocks supply route id/title/group metadata
 * when present.
 */
#include "k2cpp_lower.h"
#include "k2cpp_plan9.h"
#include "kir.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

typedef struct K2cppRoute {
    char id[KIR_NAME_MAX];
    char title[KIR_NAME_MAX];
    char group[KIR_NAME_MAX];
    char source_path[KIR_PATH_MAX];
    const KirModule *m;
    const KirFunction *fn;
} K2cppRoute;

#define K2CPP_ROUTE_MAX 512

static const KirFunction *
find_ui_function(const KirModule *mod, const char *name)
{
    if(mod == NULL || name == NULL || name[0] == '\0')
        return NULL;
    for(int i = 0; i < mod->function_count; i++) {
        const KirFunction *fn = &mod->functions[i];

        if(fn->is_public && fn->is_ui && strcmp(fn->name, name) == 0)
            return fn;
    }
    return NULL;
}

static int
function_has_explicit_route(const KirModule *mod, const KirFunction *fn)
{
    if(mod == NULL || fn == NULL)
        return 0;
    for(int i = 0; i < mod->route_count; i++) {
        if(strcmp(mod->routes[i].page, fn->name) == 0)
            return 1;
    }
    return 0;
}

static void
mkdir_parent_local(const char *path)
{
    char tmp[KIR_PATH_MAX];
    size_t i;

    snprintf(tmp, sizeof(tmp), "%s", path);
    for(i = 1; i < strlen(tmp); i++) {
        if(tmp[i] == '/') {
            tmp[i] = '\0';
            mkdir(tmp, 0755);
            tmp[i] = '/';
        }
    }
}

static void
c_str(char *dst, size_t dst_size, const char *src)
{
    size_t n = 0;

    dst[n++] = '"';
    for(const char *p = src != NULL ? src : "";
        *p != '\0' && n + 3 < dst_size; p++) {
        if(*p == '"' || *p == '\\') {
            dst[n++] = '\\';
            dst[n++] = *p;
        } else if(*p == '\n') {
            dst[n++] = '\\';
            dst[n++] = 'n';
        } else {
            dst[n++] = *p;
        }
    }
    if(n < dst_size)
        dst[n++] = '"';
    dst[n] = '\0';
}

static int
collect_routes(KirProgram *const *progs, int prog_count,
               K2cppRoute *routes, int max_routes)
{
    int count = 0;

    for(int i = 0; i < prog_count; i++) {
        const KirProgram *prog = progs[i];

        if(prog == NULL)
            continue;
        for(int m = 0; m < prog->module_count; m++) {
            const KirModule *mod = &prog->modules[m];

            for(int j = 0; j < mod->route_count; j++) {
                const KirRoute *route = &mod->routes[j];
                const KirFunction *fn = find_ui_function(mod, route->page);

                if(fn == NULL) {
                    fprintf(stderr, "%s:%d: route `%s` references missing #ui page `%s`\n",
                            route->span.path, route->span.line,
                            route->id, route->page);
                    continue;
                }
                if(count >= max_routes)
                    return count;
                snprintf(routes[count].id, sizeof(routes[0].id), "%s",
                         route->id);
                snprintf(routes[count].title, sizeof(routes[0].title), "%s",
                         route->title[0] != '\0' ? route->title : route->id);
                snprintf(routes[count].group, sizeof(routes[0].group), "%s",
                         route->group[0] != '\0' ? route->group : "Project");
                snprintf(routes[count].source_path,
                         sizeof(routes[0].source_path), "%s",
                         mod->source_path);
                routes[count].m = mod;
                routes[count].fn = fn;
                count++;
            }
            for(int j = 0; j < mod->function_count; j++) {
                const KirFunction *fn = &mod->functions[j];

                if(!fn->is_public || !fn->is_ui || fn->name[0] == '\0')
                    continue;
                if(function_has_explicit_route(mod, fn))
                    continue;
                if(count >= max_routes)
                    return count;
                snprintf(routes[count].id, sizeof(routes[0].id), "%s",
                         fn->name);
                snprintf(routes[count].title, sizeof(routes[0].title), "%s",
                         fn->name);
                snprintf(routes[count].group, sizeof(routes[0].group),
                         "Project");
                snprintf(routes[count].source_path,
                         sizeof(routes[0].source_path), "%s",
                         mod->source_path);
                routes[count].m = mod;
                routes[count].fn = fn;
                count++;
            }
        }
    }
    return count;
}

void
k2cpp_write_project(KirProgram *const *progs, int prog_count,
                  const char *root, const char *out_dir, int no_main)
{
    K2cppRoute routes[K2CPP_ROUTE_MAX];
    char path[KIR_PATH_MAX];
    const KirModule *appmod = NULL;
    FILE *out;
    int count;

    (void)root;
    if(progs == NULL || prog_count <= 0)
        return;
    count = collect_routes(progs, prog_count, routes, K2CPP_ROUTE_MAX);

    /* the one app{} module (for main()) */
    for(int i = 0; i < prog_count && appmod == NULL; i++) {
        if(progs[i] == NULL)
            continue;
        for(int m = 0; m < progs[i]->module_count; m++)
            if(progs[i]->modules[m].app.has_app) {
                appmod = &progs[i]->modules[m];
                break;
            }
    }

    /* --- kryon_project.h --- */
    snprintf(path, sizeof(path), "%s/kryon_project.h", out_dir);
    mkdir_parent_local(path);
    out = fopen(path, "wb");
    if(out == NULL)
        return;
    fprintf(out, "/* Generated by k2cpp from project Kry files. */\n");
    fprintf(out, "#ifndef K_KRYON_PROJECT_H\n#define K_KRYON_PROJECT_H\n\n");
    for(int i = 0; i < prog_count; i++) {
        const KirProgram *prog = progs[i];

        if(prog == NULL)
            continue;
        for(int m = 0; m < prog->module_count; m++) {
            const KirModule *mod = &prog->modules[m];
            char stem[KIR_PATH_MAX];
            size_t n = strlen(mod->source_path);

            if(n > 4 && strcmp(mod->source_path + n - 4, ".kry") == 0)
                n -= 4;
            if(n >= sizeof(stem))
                n = sizeof(stem) - 1;
            memcpy(stem, mod->source_path, n);
            stem[n] = '\0';
            fprintf(out, "#include \"%s.hpp\"\n", stem);
        }
    }
    fprintf(out, "\n#endif\n");
    fclose(out);

    if(count <= 0)
        return;

    /* --- kryon_project.c --- */
    snprintf(path, sizeof(path), "%s/kryon_project.c", out_dir);
    out = fopen(path, "wb");
    if(out == NULL)
        return;
    fprintf(out, "/* Generated by k2cpp from project Kry files. */\n");
    fprintf(out, "#include \"kryon_project.h\"\n");
    fprintf(out, "#include \"app_runtime.h\"\n");
    fprintf(out, "#include \"web.h\"\n");
    if(appmod != NULL && appmod->app.font_examples)
        fprintf(out, "#include \"kryon_example_font.h\"\n");
    fprintf(out, "\n");
    fprintf(out, "#if defined(__GNUC__) || defined(__clang__)\n");
    fprintf(out, "void *CreateApp(const char *project_path) __attribute__((weak));\n");
    fprintf(out, "void DestroyApp(void *app) __attribute__((weak));\n");
    fprintf(out, "void ApplyRoute(void *app, const AppRouteInfo *route) __attribute__((weak));\n");
    fprintf(out, "void BeginScreenDraw(void *app, Rectangle viewport) __attribute__((weak));\n");
    fprintf(out, "#endif\n\n");

    for(int i = 0; i < count; i++) {
        char cname[KIR_NAME_MAX * 3];
        char wrapper[64];

        snprintf(wrapper, sizeof(wrapper), "kryon_project_draw_%d", i);
        k2cpp_function_c_name(routes[i].m, routes[i].fn, cname, sizeof(cname));
        fprintf(out, "static void\n%s(void *app, Rectangle viewport)\n{\n",
                wrapper);
        fprintf(out, "    if(BeginScreenDraw != 0)\n");
        fprintf(out, "        BeginScreenDraw(app, viewport);\n");
        if(strstr(routes[i].fn->args, "Rectangle") != NULL)
            fprintf(out, "    (void)app;\n    %s(viewport);\n", cname);
        else if(routes[i].fn->args[0] != '\0')
            fprintf(out, "    (void)viewport;\n    %s(app);\n", cname);
        else
            fprintf(out, "    (void)app;\n    (void)viewport;\n    %s();\n",
                    cname);
        fprintf(out, "}\n\n");
    }

    fprintf(out, "static const AppRouteInfo kryon_project_routes[%d];\n\n",
            count);
    fprintf(out, "static void\nkryon_project_enter(void *app, int index)\n{\n");
    fprintf(out, "    if(ApplyRoute == 0 || index < 0 || index >= %d)\n",
            count);
    fprintf(out, "        return;\n");
    fprintf(out, "    ApplyRoute(app, &kryon_project_routes[index]);\n");
    fprintf(out, "}\n\n");

    fprintf(out, "static const AppRouteInfo kryon_project_routes[] = {\n");
    for(int i = 0; i < count; i++) {
        char id[KIR_NAME_MAX * 2];
        char title[KIR_NAME_MAX * 2];
        char group[KIR_NAME_MAX * 2];
        char source[KIR_PATH_MAX * 2];
        char wrapper[64];

        c_str(id, sizeof(id), routes[i].id);
        c_str(title, sizeof(title), routes[i].title);
        c_str(group, sizeof(group), routes[i].group);
        c_str(source, sizeof(source), routes[i].source_path);
        snprintf(wrapper, sizeof(wrapper), "kryon_project_draw_%d", i);
        fprintf(out, "    {%s, %s, %s, %s, kryon_project_enter, %s},\n",
                id, group, title, source, wrapper);
    }
    fprintf(out, "};\n\n");
    fprintf(out, "static App kryon_project_runtime;\n");
    fprintf(out, "static AppHost kryon_project_host;\n\n");
    fprintf(out, "AppHost *\nCreateAppHost(int abi_version, const char *project_path)\n{\n");
    fprintf(out, "    void *app = 0;\n");
    fprintf(out, "    if(abi_version != APP_HOST_ABI_VERSION)\n");
    fprintf(out, "        return 0;\n");
    fprintf(out, "    if(CreateApp != 0) {\n");
    fprintf(out, "        app = CreateApp(project_path);\n");
    fprintf(out, "        if(app == 0)\n");
    fprintf(out, "            return 0;\n");
    fprintf(out, "    } else {\n");
    fprintf(out, "        (void)project_path;\n");
    fprintf(out, "    }\n");
    fprintf(out, "    kryon_project_runtime.app = app;\n");
    fprintf(out, "    kryon_project_runtime.routes = kryon_project_routes;\n");
    fprintf(out, "    kryon_project_runtime.route_count = (int)(sizeof(kryon_project_routes) / sizeof(kryon_project_routes[0]));\n");
    fprintf(out, "    kryon_project_runtime.selected_route = 0;\n");
    fprintf(out, "    BindAppHost(&kryon_project_runtime, &kryon_project_host);\n");
    fprintf(out, "    SetAppScreen(&kryon_project_host, 0);\n");
    fprintf(out, "    return &kryon_project_host;\n}\n\n");
    fprintf(out, "void\nDestroyAppHost(AppHost *host)\n{\n");
    fprintf(out, "    if(host == 0 || host != &kryon_project_host)\n");
    fprintf(out, "        return;\n");
    fprintf(out, "    if(DestroyApp != 0 && kryon_project_runtime.app != 0)\n");
    fprintf(out, "        DestroyApp(kryon_project_runtime.app);\n");
    fprintf(out, "    kryon_project_runtime = (App){0};\n");
    fprintf(out, "    kryon_project_host = (AppHost){0};\n}\n");
    fclose(out);
    k2cpp_plan9_rewrite_file(path, 1);

    /* --- main() (hook-driven, from KirAppMeta) --- */
    if(no_main || appmod == NULL)
        return;
    out = fopen(path, "ab");
    if(out == NULL)
        return;
    {
        int width = appmod->app.width > 0 ? appmod->app.width : 800;
        int height = appmod->app.height > 0 ? appmod->app.height : 600;
        int fps = appmod->app.fps > 0 ? appmod->app.fps : 60;
        int route_host_main = appmod->app.frame[0] == '\0' && count > 0;
        char title[KIR_NAME_MAX * 2];

        c_str(title, sizeof(title), appmod->app.title);
        fprintf(out, "\nint\nmain(void)\n{\n");
        if(route_host_main) {
            fprintf(out, "    AppHost *host;\n");
            fprintf(out, "    int route_version = -1;\n");
        }
        fprintf(out, "    int width = %d;\n", width);
        fprintf(out, "    int height = %d;\n", height);
        fprintf(out, "#if defined(PLATFORM_WEB)\n");
        fprintf(out, "    SetConfigFlags(GetWebWindowFlags());\n");
        fprintf(out, "    GetWebViewportSize(width, height, &width, &height);\n");
        fprintf(out, "#endif\n");
        fprintf(out, "    InitWindow(width, height, %s);\n", title);
        fprintf(out, "    SetTargetFPS(%d);\n", fps);
        if(appmod->app.font_examples)
            fprintf(out, "    LoadExampleUIFont();\n");
        fprintf(out, "    InitUI(width, height, GetUIScale());\n");
        if(appmod->app.theme[0] != '\0')
            fprintf(out, "    SetCurrentTheme(%s, %d);\n",
                    appmod->app.theme, appmod->app.dark_mode);
        if(appmod->app.init[0] != '\0') {
            char hook[KIR_NAME_MAX * 3];
            int found = 0;

            for(int j = 0; j < appmod->function_count && !found; j++) {
                if(strcmp(appmod->functions[j].name, appmod->app.init) == 0) {
                    k2cpp_function_c_name(appmod, &appmod->functions[j],
                                        hook, sizeof(hook));
                    found = 1;
                }
            }
            fprintf(out, "    %s();\n",
                    found ? hook : appmod->app.init);
        }
        if(route_host_main) {
            fprintf(out, "    host = CreateAppHost(APP_HOST_ABI_VERSION, \".\");\n");
            fprintf(out, "    if(host == 0) {\n");
            fprintf(out, "        CloseWindow();\n");
            fprintf(out, "        return 1;\n");
            fprintf(out, "    }\n");
        }
        fprintf(out, "    while(!WindowShouldClose()) {\n");
        if(appmod->app.frame[0] != '\0') {
            char hook[KIR_NAME_MAX * 3];
            const KirFunction *entry = NULL;
            int found = 0;

            for(int j = 0; j < appmod->function_count && !found; j++) {
                if(strcmp(appmod->functions[j].name, appmod->app.frame) == 0) {
                    k2cpp_function_c_name(appmod, &appmod->functions[j],
                                        hook, sizeof(hook));
                    entry = &appmod->functions[j];
                    found = 1;
                }
            }
            if(entry != NULL && entry->is_ui) {
                fprintf(out, "        BeginFrame();\n");
                fprintf(out, "        BeginUIFrame(GetFrameWidth(), "
                             "GetFrameHeight(), GetFrameScale());\n");
                fprintf(out, "        BeginUI(Key(\"%s\"));\n", hook);
                if(strstr(entry->args, "Rectangle") != NULL) {
                    fprintf(out, "        %s((Rectangle){0, 0, "
                                 "(float)GetFrameWidth(), (float)GetFrameHeight()});\n",
                            hook);
                } else {
                    fprintf(out, "        %s();\n", hook);
                }
                fprintf(out, "        EndUI();\n");
                fprintf(out, "        EndUIFrame();\n");
                fprintf(out, "        EndFrame();\n");
            } else {
                fprintf(out, "        %s();\n",
                        found ? hook : appmod->app.frame);
            }
        } else if(route_host_main) {
            fprintf(out, "        int next_route_version = GetRouteVersion();\n");
            fprintf(out, "        if(next_route_version != route_version) {\n");
            fprintf(out, "            if(!SetAppScreenFromRoute(host))\n");
            fprintf(out, "                ReplaceAppScreenRoute(host, 0);\n");
            fprintf(out, "            route_version = GetRouteVersion();\n");
            fprintf(out, "        }\n");
            fprintf(out, "        BeginFrame();\n");
            fprintf(out, "        BeginUIFrame(GetFrameWidth(), "
                         "GetFrameHeight(), GetFrameScale());\n");
            fprintf(out, "        BeginUI(Key(\"kryon_project\"));\n");
            fprintf(out, "        DrawAppScreen(host, (Rectangle){0, 0, "
                         "(float)GetFrameWidth(), (float)GetFrameHeight()});\n");
            fprintf(out, "        EndUI();\n");
            fprintf(out, "        EndUIFrame();\n");
            fprintf(out, "        EndFrame();\n");
        } else {
            fprintf(out, "        ;\n");
        }
        fprintf(out, "    }\n");
        if(appmod->app.shutdown[0] != '\0') {
            char hook[KIR_NAME_MAX * 3];
            int found = 0;

            for(int j = 0; j < appmod->function_count && !found; j++) {
                if(strcmp(appmod->functions[j].name, appmod->app.shutdown) == 0) {
                    k2cpp_function_c_name(appmod, &appmod->functions[j],
                                        hook, sizeof(hook));
                    found = 1;
                }
            }
            fprintf(out, "    %s();\n",
                    found ? hook : appmod->app.shutdown);
        }
        if(route_host_main)
            fprintf(out, "    DestroyAppHost(host);\n");
        if(appmod->app.font_examples)
            fprintf(out, "    UnloadExampleUIFont();\n");
        fprintf(out, "    CloseWindow();\n");
        fprintf(out, "    return 0;\n}\n");
    }
    fclose(out);
    k2cpp_plan9_rewrite_file(path, 1);
}
