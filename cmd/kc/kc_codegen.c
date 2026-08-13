/* C emission. write_generated turns one parsed KryFile into a .c/.h pair,
 * driving write_body_line for each statement and write_app_main for the
 * program entry. write_project_header/generate the umbrella kryon_project.h
 * that includes every file's header. Expression rewriting (rewrite_kry_expr)
 * and defer splicing (apply_defers) happen here, at output time. */
#include "kc_ast.h"
#include "kc_internal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
write_body_line(FILE *out, const KryFile *file, const KryFunction *fn,
                const char *line, int *indent)
{
    const char *text = skip_indent(line);
    char rewritten[KC_BODY_LINE_MAX];
    int delta;

    if(text[0] == '}')
        (*indent)--;
    if(*indent < 0)
        *indent = 0;
    if(text[0] != '#') {
        for(int i = 0; i < *indent; i++)
            fputs("    ", out);
    }
    if(text[0] == '#') {
        fprintf(out, "%s\n", text);
    } else {
        rewrite_kry_expr(rewritten, sizeof(rewritten), file, fn, text);
        fprintf(out, "%s\n", rewritten);
        text = rewritten;
    }
    delta = brace_delta(text);
    if(text[0] == '}')
        delta++;
    *indent += delta;
    if(*indent < 0)
        *indent = 0;
}

static void
function_name(char *dst, size_t dst_size, const KryFile *file,
              const KryFunction *fn)
{
    kc_function_name(dst, dst_size, file, fn);
}

static int
function_takes_rectangle(const KryFunction *fn)
{
    return fn != NULL && strstr(fn->args, "Rectangle") != NULL;
}

/* Resolve a lifecycle hook name (declared in the app{} block) to the C symbol
 * for the function it names in this file. Matches colon functions (exact_name)
 * and scene builders (is_scene); anything not found is emitted verbatim so the
 * C compiler reports the unresolved symbol with a real name. */
static void
app_hook_c_name(char *dst, size_t dst_size, const KryFile *file,
                const char *hook_name)
{
    for(int i = 0; i < file->function_count; i++) {
        const KryFunction *fn = &file->functions[i];

        if((fn->exact_name || fn->is_scene) &&
           strcmp(fn->screen, hook_name) == 0) {
            kc_function_name(dst, dst_size, file, fn);
            return;
        }
    }
    snprintf(dst, dst_size, "%s", hook_name);
}

static void
write_app_main(FILE *out, const KryFile *file)
{
    char title[512];
    int width = file->app_width > 0 ? file->app_width : 800;
    int height = file->app_height > 0 ? file->app_height : 600;
    int fps = file->app_fps > 0 ? file->app_fps : 60;

    if(file->app_title[0] == '\0')
        return;
    c_string_literal(title, sizeof(title), file->app_title);

    /* Hook-driven main: the program owns its loop. init() runs once before the
     * window loop, frame() runs each iteration, shutdown() runs once after.
     * The framework does not bracket drawing or UI frames — the program calls
     * BeginDrawing/BeginUIFrame/EndDrawing itself, so it can host nested
     * render-texture passes, input overrides, and inspection overlays exactly
     * like a hand-written C app. */
    if(file->app_frame[0] != '\0') {
        char init_name[KC_NAME_MAX + 16];
        char frame_name[KC_NAME_MAX + 16];
        char shutdown_name[KC_NAME_MAX + 16];

        app_hook_c_name(frame_name, sizeof(frame_name), file,
                        file->app_frame);
        fprintf(out, "\nint\nmain(void)\n{\n");
        fprintf(out, "    InitWindow(%d, %d, %s);\n", width, height, title);
        fprintf(out, "    SetTargetFPS(%d);\n", fps);
        if(file->app_font_examples)
            fprintf(out, "    LoadExampleUIFont();\n");
        fprintf(out, "    InitUI(%d, %d, GetUIScale());\n", width, height);
        if(file->app_theme[0] != '\0')
            fprintf(out, "    SetCurrentTheme(%s, %d);\n",
                    file->app_theme, file->app_dark_mode);
        if(file->app_init[0] != '\0') {
            app_hook_c_name(init_name, sizeof(init_name), file,
                            file->app_init);
            fprintf(out, "    %s();\n", init_name);
        }
        fprintf(out, "    while(!WindowShouldClose())\n");
        fprintf(out, "        %s();\n", frame_name);
        if(file->app_shutdown[0] != '\0') {
            app_hook_c_name(shutdown_name, sizeof(shutdown_name), file,
                            file->app_shutdown);
            fprintf(out, "    %s();\n", shutdown_name);
        }
        if(file->app_font_examples)
            fprintf(out, "    UnloadExampleUIFont();\n");
        fprintf(out, "    CloseWindow();\n");
        fprintf(out, "    return 0;\n");
        fprintf(out, "}\n");
        return;
    }

    /* Scene-tree main: the app{} block names a scene builder. kc emits a main()
     * that registers built-in node kinds, creates a KryScene, runs the named
     * builder to populate it, then ticks (process + physics) and draws it each
     * frame. This is the host path for retained-mode games; the single-screen
     * immediate-mode path below is for pure UI apps. */
    if(file->app_scene[0] != '\0') {
        char scene_name[KC_NAME_MAX + 16];
        app_hook_c_name(scene_name, sizeof(scene_name), file, file->app_scene);
        fprintf(out, "\nint\nmain(void)\n{\n");
        fprintf(out, "    KryScene scene;\n");
        fprintf(out, "    InitWindow(%d, %d, %s);\n", width, height, title);
        fprintf(out, "    SetTargetFPS(%d);\n", fps);
        if(file->app_font_examples)
            fprintf(out, "    LoadExampleUIFont();\n");
        fprintf(out, "    InitUI(%d, %d, GetUIScale());\n", width, height);
        if(file->app_theme[0] != '\0')
            fprintf(out, "    SetCurrentTheme(%s, %d);\n",
                    file->app_theme, file->app_dark_mode);
        fprintf(out, "    KrySceneRegisterBuiltins();\n");
        fprintf(out, "    KrySceneInit(&scene);\n");
        fprintf(out, "    %s(&scene);\n", scene_name);
        fprintf(out, "    while(!WindowShouldClose()) {\n");
        fprintf(out, "        float dt = GetFrameTime();\n");
        fprintf(out, "        KrySceneTick(&scene, dt);\n");
        fprintf(out, "        KryScenePhysicsTick(&scene, dt);\n");
        fprintf(out, "        BeginDrawing();\n");
        fprintf(out, "        ClearBackground(BLACK);\n");
        fprintf(out, "        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());\n");
        fprintf(out, "        KrySceneDraw(&scene);\n");
        fprintf(out, "        DrawUIOverlays();\n");
        fprintf(out, "        EndUIFocus();\n");
        fprintf(out, "        EndDrawing();\n");
        fprintf(out, "    }\n");
        fprintf(out, "    KrySceneDestroy(&scene);\n");
        if(file->app_font_examples)
            fprintf(out, "    UnloadExampleUIFont();\n");
        fprintf(out, "    CloseWindow();\n");
        fprintf(out, "    return 0;\n");
        fprintf(out, "}\n");
        return;
    }

    /* Single-screen main (the examples' app{}/screen dialect): one public
     * non-exact screen function is called each frame inside a fixed
     * BeginDrawing/BeginUIFrame/EndDrawing block. */
    {
        const KryFunction *screen = NULL;
        char screen_name[KC_NAME_MAX + 16];

        for(int i = 0; i < file->function_count; i++) {
            if(file->functions[i].is_public &&
               !file->functions[i].exact_name &&
               !file->functions[i].is_scene) {
                screen = &file->functions[i];
                break;
            }
        }
        if(screen == NULL)
            return;
        function_name(screen_name, sizeof(screen_name), file, screen);
        fprintf(out, "\nint\nmain(void)\n{\n");
        fprintf(out, "    InitWindow(%d, %d, %s);\n", width, height, title);
        fprintf(out, "    SetTargetFPS(%d);\n", fps);
        if(file->app_font_examples)
            fprintf(out, "    LoadExampleUIFont();\n");
        fprintf(out, "    InitUI(%d, %d, GetUIScale());\n", width, height);
        if(file->app_theme[0] != '\0')
            fprintf(out, "    SetCurrentTheme(%s, %d);\n",
                    file->app_theme, file->app_dark_mode);
        fprintf(out, "    while(!WindowShouldClose()) {\n");
        fprintf(out, "        BeginDrawing();\n");
        fprintf(out, "        BeginUIFrame(GetScreenWidth(), GetScreenHeight(), GetUIScale());\n");
        fprintf(out, "        UIBeginTree(1);\n");
        if(function_takes_rectangle(screen))
            fprintf(out, "        %s((Rectangle){0, 0, GetScreenWidth(), GetScreenHeight()});\n",
                    screen_name);
        else
            fprintf(out, "        %s();\n", screen_name);
        fprintf(out, "        UIEndTree();\n");
        fprintf(out, "        UIReconcileTree();\n");
        fprintf(out, "        UILayoutTree();\n");
        fprintf(out, "        UIRouteInput();\n");
        fprintf(out, "        UIUpdateTree();\n");
        fprintf(out, "        DrawUITree();\n");
        fprintf(out, "        DrawUIOverlays();\n");
        fprintf(out, "        EndUIFocus();\n");
        fprintf(out, "        EndDrawing();\n");
        fprintf(out, "    }\n");
        if(file->app_font_examples)
            fprintf(out, "    UnloadExampleUIFont();\n");
        fprintf(out, "    CloseWindow();\n");
        fprintf(out, "    return 0;\n");
        fprintf(out, "}\n");
    }
}

void
write_generated(const KryFile *file, const char *root, const char *out_dir)
{
    const char *rel = relative_path(root, file->path);
    char gen_rel[KC_PATH_MAX];
    char crel[KC_PATH_MAX];
    char hrel[KC_PATH_MAX];
    char cpath[KC_PATH_MAX];
    char hpath[KC_PATH_MAX];
    char guard[KC_NAME_MAX * 2];
    char hbase[KC_PATH_MAX];
    FILE *out;

    strip_kry_ext(gen_rel, sizeof(gen_rel), rel);
    if(file->module_file[0] != '\0') {
        char module_rel[KC_PATH_MAX];

        replace_path_basename(module_rel, sizeof(module_rel), gen_rel,
                              file->module_file);
        snprintf(gen_rel, sizeof(gen_rel), "%s", module_rel);
    }
    snprintf(crel, sizeof(crel), "%s.c", gen_rel);
    snprintf(hrel, sizeof(hrel), "%s.h", gen_rel);
    path_join(cpath, sizeof(cpath), out_dir, crel);
    path_join(hpath, sizeof(hpath), out_dir, hrel);
    mkdir_parent(cpath);
    mkdir_parent(hpath);
    header_guard(guard, sizeof(guard), gen_rel);
    snprintf(hbase, sizeof(hbase), "%s.h", gen_rel);

    out = fopen(hpath, "wb");
    if(out == NULL)
        die("%s: open failed: %s", hpath, strerror(errno));
    fprintf(out, "/* Generated by kc from %s. */\n", rel);
    fprintf(out, "#ifndef %s\n#define %s\n\n", guard, guard);
    for(int i = 0; i < file->include_count; i++) {
        if(file->include_guards[i][0] != '\0')
            fprintf(out, "#if %s\n", file->include_guards[i]);
        fprintf(out, "#include \"%s\"\n", file->includes[i]);
        if(file->include_guards[i][0] != '\0')
            fprintf(out, "#endif\n");
    }
    if(file->include_count > 0)
        fputc('\n', out);
    for(int i = 0; i < file->public_type_count; i++)
        fprintf(out, "%s\n", file->public_types[i]);
    if(file->public_type_count > 0)
        fputc('\n', out);
    for(int i = 0; i < file->public_global_count; i++)
        fprintf(out, "%s\n", file->public_globals[i]);
    if(file->public_global_count > 0)
        fputc('\n', out);
    for(int i = 0; i < file->function_count; i++) {
        const KryFunction *fn = &file->functions[i];
        const char *args = fn->args[0] != '\0' ? fn->args : "void";
        const char *return_type = fn->return_type[0] != '\0'
                                      ? fn->return_type
                                      : "void";
        char name[KC_NAME_MAX + 16];

        if(!fn->is_public)
            continue;
        function_name(name, sizeof(name), file, fn);
        if(fn->guard[0] != '\0')
            fprintf(out, "#if %s\n", fn->guard);
        fprintf(out, "%s %s(%s);\n", return_type, name, args);
        if(fn->guard[0] != '\0')
            fprintf(out, "#endif\n");
    }
    fputc('\n', out);
    fprintf(out, "#endif\n");
    fclose(out);

    out = fopen(cpath, "wb");
    if(out == NULL)
        die("%s: open failed: %s", cpath, strerror(errno));
    fprintf(out, "/* Generated by kc from %s. */\n", rel);
    fprintf(out, "#include \"%s\"\n", hbase);
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#include \"ui_inspect.h\"\n");
    fprintf(out, "#if defined(__GNUC__) || defined(__clang__)\n");
    fprintf(out, "#define KRYON_PRIVATE_UNUSED __attribute__((unused))\n");
    fprintf(out, "#else\n");
    fprintf(out, "#define KRYON_PRIVATE_UNUSED\n");
    fprintf(out, "#endif\n");
    if(file->app_font_examples)
        fprintf(out, "#include \"example_ui_font.h\"\n");
    if(file->define_count > 0) {
        fputc('\n', out);
        for(int i = 0; i < file->define_count; i++) {
            if(file->define_guards[i][0] != '\0')
                fprintf(out, "#if %s\n", file->define_guards[i]);
            fprintf(out, "#define %s %s\n", file->define_names[i],
                    file->define_values[i]);
            if(file->define_guards[i][0] != '\0')
                fprintf(out, "#endif\n");
        }
    }
    if(file->raw_count > 0) {
        fputc('\n', out);
        for(int i = 0; i < file->raw_count; i++)
            fprintf(out, "%s\n", file->raw[i]);
    }
    if(file->private_type_count > 0) {
        fputc('\n', out);
        for(int i = 0; i < file->private_type_count; i++)
            fprintf(out, "%s\n", file->private_types[i]);
    }
    if(file->global_count > 0) {
        fputc('\n', out);
        for(int i = 0; i < file->global_count; i++)
            fprintf(out, "%s\n", file->globals[i]);
    }
    for(int i = 0; i < file->function_count; i++) {
        const KryFunction *fn = &file->functions[i];
        const char *args = fn->args[0] != '\0' ? fn->args : "void";
        const char *return_type = fn->return_type[0] != '\0'
                                      ? fn->return_type
                                      : "void";
        char name[KC_NAME_MAX + 16];

        if(fn->is_public)
            continue;
        function_name(name, sizeof(name), file, fn);
        if(fn->guard[0] != '\0')
            fprintf(out, "\n#if %s", fn->guard);
        fprintf(out, "\nstatic KRYON_PRIVATE_UNUSED %s %s(%s);\n",
                return_type, name, args);
        if(fn->guard[0] != '\0')
            fprintf(out, "#endif\n");
    }
    if(file->state_count > 0) {
        fputc('\n', out);
        for(int i = 0; i < file->state_count; i++)
            fprintf(out, "%s\n", file->state[i]);
    }
    /* Splice deferred statements into each function body before emission. */
    for(int i = 0; i < file->function_count; i++)
        apply_defers(&file->functions[i]);
    for(int i = 0; i < file->function_count; i++) {
        const KryFunction *fn = &file->functions[i];
        const char *args = fn->args[0] != '\0' ? fn->args : "void";
        const char *return_type = fn->return_type[0] != '\0'
                                      ? fn->return_type
                                      : "void";
        char name[KC_NAME_MAX + 16];
        int indent = 1;

        function_name(name, sizeof(name), file, fn);
        if(fn->guard[0] != '\0')
            fprintf(out, "\n#if %s\n", fn->guard);
        if(fn->is_public)
            fprintf(out, "\n%s\n%s(%s)\n{\n", return_type, name, args);
        else
            fprintf(out, "\nstatic KRYON_PRIVATE_UNUSED %s\n%s(%s)\n{\n",
                    return_type, name, args);
        {
            AstFunction *af = ast_function_from_body(fn);

            if(af != NULL) {
                for(int j = 0; j < af->stmt_count; j++)
                    write_body_line(out, file, fn, af->stmts[j].text, &indent);
                ast_function_free(af);
            } else {
                for(int j = 0; j < fn->body_count; j++)
                    write_body_line(out, file, fn, fn->body[j], &indent);
            }
        }
        for(int j = 0; j < fn->call_count; j++) {
            const char *call = fn->calls[j];
            size_t len = strlen(call);

            for(int k = 0; k < indent; k++)
                fputs("    ", out);
            fprintf(out, "%s", call);
            if(len == 0 || call[len - 1] != ';')
                fputc(';', out);
            fputc('\n', out);
        }
        fprintf(out, "}\n");
        if(fn->guard[0] != '\0')
            fprintf(out, "#endif\n");
    }
    if(!file->no_main)
        write_app_main(out, file);
    fclose(out);
}

/* Relative path (from the project root) of a file's generated header, applying
 * the module_file rename when set. Shared by write_generated and the project
 * header emitter. */
void
generated_header_rel(char *dst, size_t dst_size, const KryFile *file,
                     const char *root)
{
    const char *rel = relative_path(root, file->path);
    char gen_rel[KC_PATH_MAX];

    strip_kry_ext(gen_rel, sizeof(gen_rel), rel);
    if(file->module_file[0] != '\0') {
        char dir[KC_PATH_MAX];
        char *slash;

        snprintf(dir, sizeof(dir), "%s", gen_rel);
        slash = strrchr(dir, '/');
        if(slash != NULL) {
            *slash = '\0';
            snprintf(gen_rel, sizeof(gen_rel), "%s/%s", dir,
                     file->module_file);
        } else {
            snprintf(gen_rel, sizeof(gen_rel), "%s", file->module_file);
        }
    }
    snprintf(dst, dst_size, "%s.h", gen_rel);
}
