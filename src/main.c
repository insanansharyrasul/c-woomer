#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <stdio.h>

#define SPOTLIGHT_TINT CLITERAL(Color){0, 0, 0, 190}

int main(void)
{
    // Take screenshot using grim for Wayland.
    const char *screenshot_path = "/tmp/woomer_screenshot.png";
    char command[256];
    sprintf(command, "grim %s", screenshot_path);
    if (system(command) != 0)
    {
        printf("Error: Failed to take screenshot. Make sure 'grim' is installed and you are running a Wayland compositor.\n");
        return -1;
    }

    // Initialize Raylib Window
    const int screenWidth = GetMonitorWidth(0);
    const int screenHeight = GetMonitorHeight(0);

    SetConfigFlags(FLAG_WINDOW_UNDECORATED | FLAG_WINDOW_TRANSPARENT | FLAG_VSYNC_HINT);
    InitWindow(screenWidth, screenHeight, "woomer");
    SetWindowPosition(0, 0);

    // Load screenshot texture
    Image screenshot_image = LoadImage(screenshot_path);
    if (screenshot_image.data == NULL)
    {
        printf("Error: Failed to load screenshot image from %s.\n", screenshot_path);
        CloseWindow();
        return -1;
    }
    Texture2D screenshot_texture = LoadTextureFromImage(screenshot_image);
    UnloadImage(screenshot_image);

    // Load shader
    Shader spotlight_shader = LoadShader(0, "shaders/spotlight.fs");
    int spotlight_tint_uniform_location = GetShaderLocation(spotlight_shader, "spotlightTint");
    int cursor_position_uniform_location = GetShaderLocation(spotlight_shader, "cursorPosition");
    int spotlight_radius_multiplier_uniform_location = GetShaderLocation(spotlight_shader, "spotlightRadiusMultiplier");

    // Setup Camera and state
    Camera2D camera = {0};
    camera.zoom = 1.0f;

    float delta_scale = 0.0f;
    Vector2 scale_pivot = {0};
    Vector2 velocity = {0};
    float spotlight_radius_multiplier = 1.0f;
    float spotlight_radius_multiplier_delta = 0.0f;

    SetTargetFPS(144);

    // Main game loop
    while (!WindowShouldClose())
    {
        // --- Update ---
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsKeyPressed(KEY_ESCAPE))
            break;

        bool enable_spotlight = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        float scrolled_amount = GetMouseWheelMove();

        if (IsKeyPressed(KEY_LEFT_CONTROL) || IsKeyPressed(KEY_RIGHT_CONTROL))
        {
            spotlight_radius_multiplier = 5.0f;
            spotlight_radius_multiplier_delta = -15.0f;
        }

        if (scrolled_amount != 0.0f)
        {
            bool shift_down = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            if (enable_spotlight && shift_down)
            {
                spotlight_radius_multiplier_delta -= scrolled_amount;
            }
            else
            {
                delta_scale += scrolled_amount;
            }
            scale_pivot = GetMousePosition();
        }

        if (fabsf(delta_scale) > 0.01f)
        {
            Vector2 p0 = GetScreenToWorld2D(scale_pivot, camera);
            camera.zoom += delta_scale * GetFrameTime() * 2.0f;
            camera.zoom = Clamp(camera.zoom, 0.1f, 10.0f);
            Vector2 p1 = GetScreenToWorld2D(scale_pivot, camera);
            camera.target = Vector2Add(camera.target, Vector2Subtract(p0, p1));
            delta_scale -= delta_scale * GetFrameTime() * 4.0f;
        }

        spotlight_radius_multiplier += spotlight_radius_multiplier_delta * GetFrameTime();
        spotlight_radius_multiplier = Clamp(spotlight_radius_multiplier, 0.3f, 10.0f);
        spotlight_radius_multiplier_delta -= spotlight_radius_multiplier_delta * GetFrameTime() * 4.0f;

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
        {
            Vector2 delta = GetMouseDelta();
            delta = Vector2Scale(delta, -1.0f / camera.zoom);
            camera.target = Vector2Add(camera.target, delta);
            velocity = Vector2Scale(delta, 1.0f / GetFrameTime());
        }
        else
        {
            if (Vector2LengthSqr(velocity) > 1.0f)
            {
                camera.target = Vector2Add(camera.target, Vector2Scale(velocity, GetFrameTime()));
                velocity = Vector2Subtract(velocity, Vector2Scale(velocity, GetFrameTime() * 6.0f));
            }
        }

        // --- Draw ---
        BeginDrawing();
        ClearBackground(BLANK);

        BeginMode2D(camera);

        if (enable_spotlight)
        {
            ClearBackground(SPOTLIGHT_TINT);

            Vector2 mouse_position = GetMousePosition();
            float screen_height = (float)GetScreenHeight();
            Vector2 cursor_for_shader = {mouse_position.x, screen_height - mouse_position.y};
            Vector4 spotlight_tint_normalized = ColorNormalize(SPOTLIGHT_TINT);

            SetShaderValue(spotlight_shader, spotlight_tint_uniform_location, &spotlight_tint_normalized, SHADER_UNIFORM_VEC4);
            SetShaderValue(spotlight_shader, cursor_position_uniform_location, &cursor_for_shader, SHADER_UNIFORM_VEC2);
            SetShaderValue(spotlight_shader, spotlight_radius_multiplier_uniform_location, &spotlight_radius_multiplier, SHADER_UNIFORM_FLOAT);

            BeginShaderMode(spotlight_shader);
            DrawTexture(screenshot_texture, 0, 0, WHITE);
            EndShaderMode();
        }
        else
        {
            DrawTexture(screenshot_texture, 0, 0, WHITE);
        }

        EndMode2D();
        EndDrawing();
    }

    // De-Initialization
    UnloadTexture(screenshot_texture);
    UnloadShader(spotlight_shader);
    CloseWindow();
    remove(screenshot_path);

    return 0;
}
