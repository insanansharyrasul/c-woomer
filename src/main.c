#define _POSIX_C_SOURCE 200809L
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define SPOTLIGHT_TINT CLITERAL(Color){0, 0, 0, 190}

// Embedded spotlight fragment shader
const char *spotlight_fragment_shader =
    "#version 330\n"
    "\n"
    "// Input vertex attributes (from vertex shader)\n"
    "in vec2 fragTexCoord;\n"
    "in vec4 fragColor;\n"
    "\n"
    "// Input uniform values\n"
    "uniform sampler2D texture0;\n"
    "uniform vec4 colDiffuse;\n"
    "\n"
    "uniform vec4 spotlightTint;\n"
    "uniform vec2 cursorPosition;\n"
    "uniform float spotlightRadiusMultiplier;\n"
    "\n"
    "// This is multiplied by spotlightRadiusMultiplier to obtain the radius "
    "in pixels\n"
    "const int UNIT_RADIUS = 150;\n"
    "\n"
    "// Output fragment color\n"
    "out vec4 finalColor;\n"
    "\n"
    "void main()\n"
    "{\n"
    "    // Texel color fetching from texture sampler\n"
    "    vec4 texelColor = texture(texture0, fragTexCoord);\n"
    "\n"
    "    // Calculate distance from the current fragment to the cursor "
    "position\n"
    "    float distanceToCursor = distance(gl_FragCoord.xy, "
    "vec2(cursorPosition.x, cursorPosition.y));\n"
    "\n"
    "    // Calculate spotlight radius in pixels\n"
    "    float spotlightRadius = float(UNIT_RADIUS) * "
    "spotlightRadiusMultiplier;\n"
    "    if (distanceToCursor > spotlightRadius)  {\n"
    "        finalColor = (mix(texelColor, vec4(spotlightTint.rgb, 1.0), "
    "spotlightTint.a) * colDiffuse);\n"
    "    } else {\n"
    "        finalColor = (texelColor * colDiffuse);\n"
    "    }\n"
    "}";

// [opt] Start grim in the background, streaming an uncompressed PPM to a
// pipe. No PNG encode, no file on disk, and we don't block while it works.
// Safe to fork here: the process is still single-threaded.
static pid_t start_capture(const char *output, int *out_fd) {
    int fds[2];
    if (pipe(fds) != 0)
        return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    if (pid == 0) {
        dup2(fds[1], STDOUT_FILENO);
        close(fds[0]);
        close(fds[1]);
        execlp("grim", "grim", "-o", output, "-t", "ppm", "-", (char *)NULL);
        _exit(127);
    }

    close(fds[1]);
    *out_fd = fds[0];
    return pid;
}

// [opt] Read the PPM (P6, 8-bit RGB) from the pipe straight into a raylib
// Image, skipping the PNG decoder entirely.
static bool read_capture(int fd, pid_t pid, Image *out) {
    FILE *f = fdopen(fd, "rb");
    if (!f) {
        close(fd);
        waitpid(pid, NULL, 0);
        return false;
    }

    int w = 0, h = 0, maxval = 0;
    if (fscanf(f, "P6 %d %d %d", &w, &h, &maxval) != 3 || maxval != 255 ||
        w <= 0 || h <= 0) {
        fclose(f);
        waitpid(pid, NULL, 0);
        return false;
    }
    fgetc(f); // exactly one whitespace byte separates header and pixel data

    size_t size = (size_t)w * (size_t)h * 3;
    unsigned char *pixels = (unsigned char *)malloc(size);
    if (!pixels || fread(pixels, 1, size, f) != size) {
        free(pixels);
        fclose(f);
        waitpid(pid, NULL, 0);
        return false;
    }
    fclose(f);
    waitpid(pid, NULL, 0);

    out->data = pixels;
    out->width = w;
    out->height = h;
    out->mipmaps = 1;
    out->format = PIXELFORMAT_UNCOMPRESSED_R8G8B8;
    return true;
}

int main(void) {
    Monitor mon;

    if (!get_focused_monitor(&mon)) {
        printf("Error: could not query the focused monitor from Hyprland.\n");
        return -1;
    }
    // [opt] 1. Kick off the capture before anything else. grim runs in
    //          parallel while we bring up the window, GL context and shader.
    int capture_fd = -1;
    pid_t grim_pid = start_capture(mon.name, &capture_fd);
    if (grim_pid < 0) {
        printf("Error: Failed to start grim. Make sure 'grim' is installed "
               "and you are running a Wayland compositor.\n");
        return -1;
    }

    // const int screenWidth = 2880;
    // const int screenHeight = 1800;
    const int screenWidth = (int)(mon.width / mon.scale + 0.5f);
    const int screenHeight = (int)(mon.height / mon.scale + 0.5f);

    // [opt] 2. The window starts HIDDEN so grim can never capture woomer
    //          itself. It is only shown once the screenshot is in hand.
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TRANSPARENT |
                   FLAG_VSYNC_HINT | FLAG_WINDOW_HIDDEN);
    InitWindow(screenWidth, screenHeight, "woomer");
    SetWindowPosition(0, 0);

    // Load shader
    Shader spotlight_shader =
        LoadShaderFromMemory(0, spotlight_fragment_shader);
    int spotlight_tint_uniform_location =
        GetShaderLocation(spotlight_shader, "spotlightTint");
    int cursor_position_uniform_location =
        GetShaderLocation(spotlight_shader, "cursorPosition");
    int spotlight_radius_multiplier_uniform_location =
        GetShaderLocation(spotlight_shader, "spotlightRadiusMultiplier");

    // Pre-calculate constant values outside the loop
    Vector4 spotlight_tint_normalized = ColorNormalize(SPOTLIGHT_TINT);

    // Set constant shader value once
    SetShaderValue(spotlight_shader, spotlight_tint_uniform_location,
                   &spotlight_tint_normalized, SHADER_UNIFORM_VEC4);

    // [opt] 3. Only now wait for the screenshot (by this point grim has
    //          usually finished capturing and is just waiting on the pipe).
    Image screenshot_image;
    if (!read_capture(capture_fd, grim_pid, &screenshot_image)) {
        printf("Error: Failed to read screenshot from grim.\n");
        UnloadShader(spotlight_shader);
        CloseWindow();
        return -1;
    }
    Texture2D screenshot_texture = LoadTextureFromImage(screenshot_image);
    UnloadImage(screenshot_image);

    // [opt] 4. Screenshot is on the GPU: reveal the window.
    ClearWindowState(FLAG_WINDOW_HIDDEN);

    // Setup Camera and state
    Camera2D camera = {0};
    camera.zoom = 1.0f;

    // Center the screenshot in the middle of the screen
    // camera.target.x = (screenshot_texture.width - screenWidth) / 5.0f;
    // camera.target.y = (screenshot_texture.height - screenHeight) / 5.0f;

    float delta_scale = 0.0f;
    Vector2 scale_pivot = {0};
    Vector2 velocity = {0};
    float spotlight_radius_multiplier = 1.0f;
    float spotlight_radius_multiplier_delta = 0.0f;

    SetTargetFPS(144);

    // Main game loop
    while (!WindowShouldClose()) {
        // --- Update ---
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_ESCAPE))
            break;

        // Cache frame time to avoid multiple calls
        float frame_time = GetFrameTime();
        const float win_w = (float)GetScreenWidth();
        const float win_h = (float)GetScreenHeight();
        const float px = (float)screenshot_texture.width / win_w;

        bool enable_spotlight =
            IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        float scrolled_amount = GetMouseWheelMove();

        const float SPOTLIGHT_START = 10.0f;
        const float SPOTLIGHT_REST = 2.5f;

        if (IsKeyPressed(KEY_LEFT_CONTROL) || IsKeyPressed(KEY_RIGHT_CONTROL)) {
            spotlight_radius_multiplier = SPOTLIGHT_START;
            spotlight_radius_multiplier_delta =
                -4.0f * (SPOTLIGHT_START - SPOTLIGHT_REST);
        }

        if (scrolled_amount != 0.0f) {
            bool shift_down =
                IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            if (enable_spotlight && shift_down) {
                spotlight_radius_multiplier_delta -= scrolled_amount;
            } else {
                delta_scale += scrolled_amount;
            }
            scale_pivot = GetMousePosition();
        }

        if (fabsf(delta_scale) > 0.01f) {
            Vector2 p0 = GetScreenToWorld2D(scale_pivot, camera);
            camera.zoom += delta_scale * frame_time * 2.0f;
            camera.zoom = Clamp(camera.zoom, 1.0f, 10.0f);
            Vector2 p1 = GetScreenToWorld2D(scale_pivot, camera);
            camera.target = Vector2Add(camera.target, Vector2Subtract(p0, p1));
            delta_scale -= delta_scale * frame_time * 4.0f;
        }

        spotlight_radius_multiplier +=
            spotlight_radius_multiplier_delta * frame_time;
        spotlight_radius_multiplier =
            Clamp(spotlight_radius_multiplier, 0.3f, 10.0f);
        spotlight_radius_multiplier_delta -=
            spotlight_radius_multiplier_delta * frame_time * 4.0f;

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            Vector2 delta = GetMouseDelta();
            delta = Vector2Scale(delta, -1.0f / camera.zoom);
            camera.target = Vector2Add(camera.target, delta);
            velocity = Vector2Scale(delta, 1.0f / frame_time);
        } else {
            if (Vector2LengthSqr(velocity) > 1.0f) {
                camera.target = Vector2Add(camera.target,
                                           Vector2Scale(velocity, frame_time));
                velocity = Vector2Subtract(
                    velocity, Vector2Scale(velocity, frame_time * 6.0f));
            }
        }

        // --- Draw ---
        BeginDrawing();
        rlViewport(0, 0, screenshot_texture.width, screenshot_texture.height);
        ClearBackground(BLACK);

        BeginMode2D(camera);

        if (enable_spotlight) {
            ClearBackground(SPOTLIGHT_TINT);

            Vector2 mouse_position = GetMousePosition();
            Vector2 cursor_for_shader = {mouse_position.x * px,
                                         (win_h - mouse_position.y) - px};

            SetShaderValue(spotlight_shader, cursor_position_uniform_location,
                           &cursor_for_shader, SHADER_UNIFORM_VEC2);
            SetShaderValue(spotlight_shader,
                           spotlight_radius_multiplier_uniform_location,
                           &spotlight_radius_multiplier, SHADER_UNIFORM_FLOAT);

            BeginShaderMode(spotlight_shader);
            DrawTextureEx(screenshot_texture, (Vector2){0, 0}, 0.0f, 1.0f / px, WHITE);
            EndShaderMode();
        } else {
            DrawTextureEx(screenshot_texture, (Vector2){0, 0}, 0.0f, 1.0f / px, WHITE);
        }

        EndMode2D();
        EndDrawing();
    }

    // De-Initialization
    UnloadTexture(screenshot_texture);
    UnloadShader(spotlight_shader);
    CloseWindow();

    return 0;
}
