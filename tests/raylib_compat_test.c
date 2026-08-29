#include "kryon.h"

#include <stdio.h>

static int failures = 0;

static void
check_true(const char *name, int ok)
{
    if(!ok) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static void
use_types(void)
{
    Color color = RED;
    Rectangle rect = {10.0f, 20.0f, 30.0f, 40.0f};
    Vector2 point = {1.0f, 2.0f};
    Image image = {0};
    Texture2D texture = {0};
    Font font = {0};

    check_true("color fields", color.r == 230 && color.a == 255);
    check_true("rectangle fields", rect.x == 10.0f && rect.height == 40.0f);
    check_true("vector fields", point.x == 1.0f && point.y == 2.0f);
    check_true("zero image", image.data == 0 && image.width == 0);
    check_true("zero texture", texture.id == 0 && texture.width == 0);
    check_true("zero font", font.baseSize == 0 && font.texture.id == 0);
}

static void
use_math3d(void)
{
    Vector3 a = {1.0f, 2.0f, 3.0f};
    Vector3 b = {4.0f, 5.0f, 6.0f};
    Vector3 sum = Vector3Add(a, b);
    Vector3 diff = Vector3Subtract(b, a);
    Vector3 scaled = Vector3Scale(a, 2.0f);
    Vector3 clamped = Vector3Clamp(a, (Vector3){1.5f, 1.5f, 1.5f}, (Vector3){2.5f, 2.5f, 2.5f});
    Vector3 length_clamped = Vector3ClampValue(a, 1.0f, 2.0f);
    float clamped_length = Vector3Length(length_clamped);
    Vector3 unit = Vector3Normalize((Vector3){0.0f, 0.0f, 7.0f});
    Matrix identity = MatrixIdentity();
    Matrix product = MatrixMultiply(identity, identity);
    Matrix rotation = MatrixRotateXYZ((Vector3){0.0f, 0.0f, 0.0f});

    check_true("vector3 add", sum.x == 5.0f && sum.y == 7.0f && sum.z == 9.0f);
    check_true("vector3 subtract", diff.x == 3.0f && diff.y == 3.0f && diff.z == 3.0f);
    check_true("vector3 scale", scaled.z == 6.0f);
    check_true("vector3 clamp", clamped.x == 1.5f && clamped.y == 2.0f && clamped.z == 2.5f);
    check_true("vector3 clamp value length", clamped_length <= 2.0f && clamped_length > 1.9f);
    check_true("vector3 normalize", unit.z == 1.0f);
    check_true("matrix identity", identity.m0 == 1.0f && identity.m5 == 1.0f && identity.m15 == 1.0f && identity.m1 == 0.0f);
    check_true("matrix multiply identity", product.m0 == 1.0f && product.m10 == 1.0f && product.m4 == 0.0f);
    check_true("matrix zero rotation", rotation.m0 == 1.0f && rotation.m1 == 0.0f);
}

static void
use_3d(int argc)
{
    /* Link-level coverage for the 3D tier (camera, mesh, shader, rlgl).
     * Everything with a side effect sits inside the magic-argc guard like
     * the 2D path above, so the test stays headless while every symbol
     * must still link. */
    if(argc == 12345) {
        float verts[9] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};
        float texcoords[6] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f};
        unsigned char colors[12] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};
        unsigned short indices[3] = {0, 1, 2};
        float one = 1.0f;
        Camera3D camera = {0};
        Mesh mesh = {0};
        Material material;
        Shader shader;
        Matrix modelview, projection, mvp;
        unsigned int vao_id, vbo_id, ebo_id;
        int loc;

        camera.position = (Vector3){0.0f, 2.0f, -4.0f};
        camera.target = Vector3Zero();
        camera.up = (Vector3){0.0f, 1.0f, 0.0f};
        camera.fovy = 65.0f;
        camera.projection = CAMERA_PERSPECTIVE;
        UpdateCamera(&camera, CAMERA_CUSTOM);

        material = LoadMaterialDefault();
        shader = LoadShaderFromMemory(
            "attribute vec3 vertexPosition;\n"
            "uniform mat4 mvp;\n"
            "void main() { gl_Position = mvp * vec4(vertexPosition, 1.0); }\n",
            "precision mediump float;\n"
            "void main() { gl_FragColor = vec4(1.0); }\n");
        modelview = rlGetMatrixModelview();
        projection = rlGetMatrixProjection();
        mvp = MatrixMultiply(modelview, projection);

        mesh.vertexCount = 3;
        mesh.triangleCount = 1;
        mesh.vertices = verts;
        mesh.texcoords = texcoords;
        mesh.colors = colors;
        mesh.indices = indices;
        UploadMesh(&mesh, false);

        loc = GetShaderLocation(shader, "mvp");
        SetShaderValue(shader, loc, &one, RL_SHADER_UNIFORM_FLOAT);
        SetShaderValueMatrix(shader, loc, mvp);
        shader.locs[SHADER_LOC_MATRIX_MVP] = loc;
        material.shader = shader;

        BeginMode3D(camera);
        DrawMesh(mesh, material, MatrixIdentity());
        DrawCubeV(Vector3Zero(), (Vector3){1.0f, 1.0f, 1.0f}, WHITE);
        vao_id = rlLoadVertexArray();
        vbo_id = rlLoadVertexBuffer(verts, (int)sizeof(verts), false);
        ebo_id = rlLoadVertexBufferElement(indices, (int)sizeof(indices), false);
        rlEnableShader(shader.id);
        rlEnableTexture(material.maps[MATERIAL_MAP_DIFFUSE].texture.id);
        rlEnableVertexArray(vao_id);
        rlEnableVertexBuffer(vbo_id);
        rlEnableVertexBufferElement(ebo_id);
        rlSetVertexAttribute(0, 3, RL_FLOAT, false, 3 * (int)sizeof(float), 0);
        rlEnableVertexAttribute(0);
        rlSetUniform(rlGetLocationUniform(shader.id, "mvp"), &one, RL_SHADER_UNIFORM_FLOAT, 1);
        rlSetUniformMatrix(loc, mvp);
        rlDrawVertexArrayElements(0, 3, 0);
        rlDisableVertexBufferElement();
        rlDisableVertexBuffer();
        rlDisableVertexArray();
        rlDisableTexture();
        rlDisableShader();
        rlDisableBackfaceCulling();
        rlEnableBackfaceCulling();
        rlSetMatrixModelview(rlGetMatrixTransform());
        rlSetMatrixProjection(projection);
        rlEnableDepthTest();
        rlDisableDepthTest();
        EndMode3D();

        rlUnloadVertexBuffer(vbo_id);
        rlUnloadVertexArray(vao_id);
        UnloadMaterial(material);
        UnloadShader(shader);
        UnloadMesh(mesh);
    }
}

static void
use_functions(int argc)
{
    if(argc == 12345) {
        InitWindow(800, 600, "compat");
        SetTargetFPS(60);
        BeginDrawing();
        ClearBackground(BLACK);
        DrawRectangle(10, 20, 30, 40, RAYWHITE);
        DrawRectangleRec((Rectangle){50.0f, 60.0f, 70.0f, 80.0f}, BLUE);
        DrawText(TextFormat("mouse %.1f", GetMousePosition().x), 10, 10, 20, WHITE);
        DrawTexture((Texture2D){0}, 0, 0, WHITE);
        EndDrawing();
        if(WindowShouldClose() || IsKeyPressed(KEY_ESCAPE) || GetMouseWheelMove() != 0.0f)
            TraceLog(LOG_INFO, "input path linked");
        CloseWindow();
    }
}

int
main(int argc, char **argv)
{
    (void)argv;

    use_types();
    use_math3d();
    use_functions(argc);
    use_3d(argc);

    if(failures != 0)
        return 1;
    printf("raylib compatibility tests passed\n");
    return 0;
}
