#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <SDL.h>

#include "common_cpu_infra.h"
#include "common_rtl.h"
#include "lufia1_runtime.h"
#include "snes/ppu.h"

extern const RtlGameInfo kLufia1GameInfo;
extern Ppu *g_ppu;

/*
 * Headless/native SNES framebuffer.
 *
 * The legacy renderer writes B,G,R,0 bytes. On little-endian hosts this
 * corresponds to SDL_PIXELFORMAT_ARGB8888 with blending disabled.
 */
enum {
    FRAME_WIDTH  = 256,
    FRAME_HEIGHT = 240,
    FRAME_BPP    = 4
};

static uint8_t g_framebuffer[
    FRAME_WIDTH * FRAME_HEIGHT * FRAME_BPP
];

/*
 * RtlRunFrame input layout:
 *
 * bit 0  B
 * bit 1  Y
 * bit 2  Select
 * bit 3  Start
 * bit 4  Up
 * bit 5  Down
 * bit 6  Left
 * bit 7  Right
 * bit 8  A
 * bit 9  X
 * bit 10 L
 * bit 11 R
 *
 * bit 30 marks controller 1 as connected.
 */
enum {
    SNES_B      = 1u << 0,
    SNES_Y      = 1u << 1,
    SNES_SELECT = 1u << 2,
    SNES_START  = 1u << 3,
    SNES_UP     = 1u << 4,
    SNES_DOWN   = 1u << 5,
    SNES_LEFT   = 1u << 6,
    SNES_RIGHT  = 1u << 7,
    SNES_A      = 1u << 8,
    SNES_X      = 1u << 9,
    SNES_L      = 1u << 10,
    SNES_R      = 1u << 11,

    SNES_CONTROLLER_1_ACTIVE = 1u << 30
};

static uint8_t *ReadFile(
    const char *path,
    uint32_t *size_out)
{
    FILE *f = fopen(path, "rb");

    if (!f) {
        fprintf(stderr, "Could not open ROM: %s\n", path);
        return NULL;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }

    long size = ftell(f);

    if (size <= 0 || size > 0x7fffffffL) {
        fclose(f);
        return NULL;
    }

    rewind(f);

    uint8_t *data = malloc((size_t)size);

    if (!data) {
        fclose(f);
        return NULL;
    }

    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return NULL;
    }

    fclose(f);

    *size_out = (uint32_t)size;
    return data;
}

static SDL_GameController *OpenFirstController(void)
{
    const int count = SDL_NumJoysticks();

    for (int i = 0; i < count; i++) {
        if (!SDL_IsGameController(i))
            continue;

        SDL_GameController *pad =
            SDL_GameControllerOpen(i);

        if (pad) {
            fprintf(
                stderr,
                "[host] controller: %s\n",
                SDL_GameControllerName(pad));

            return pad;
        }
    }

    fprintf(
        stderr,
        "[host] no game controller detected; "
        "keyboard input available\n");

    return NULL;
}

static uint32_t ReadPlayerInput(
    SDL_GameController *controller)
{
    uint32_t input = SNES_CONTROLLER_1_ACTIVE;

    const Uint8 *keys = SDL_GetKeyboardState(NULL);

    /*
     * Keyboard layout:
     *
     * Arrow keys  D-pad
     *
     * Z = B       X = A
     * A = Y       S = X
     *
     * Q = L       W = R
     *
     * Enter     = Start
     * Backspace = Select
     */
    if (keys[SDL_SCANCODE_Z])
        input |= SNES_B;

    if (keys[SDL_SCANCODE_A])
        input |= SNES_Y;

    if (keys[SDL_SCANCODE_BACKSPACE])
        input |= SNES_SELECT;

    if (keys[SDL_SCANCODE_RETURN])
        input |= SNES_START;

    if (keys[SDL_SCANCODE_UP])
        input |= SNES_UP;

    if (keys[SDL_SCANCODE_DOWN])
        input |= SNES_DOWN;

    if (keys[SDL_SCANCODE_LEFT])
        input |= SNES_LEFT;

    if (keys[SDL_SCANCODE_RIGHT])
        input |= SNES_RIGHT;

    if (keys[SDL_SCANCODE_X])
        input |= SNES_A;

    if (keys[SDL_SCANCODE_S])
        input |= SNES_X;

    if (keys[SDL_SCANCODE_Q])
        input |= SNES_L;

    if (keys[SDL_SCANCODE_W])
        input |= SNES_R;

    /*
     * Standard modern-controller spatial mapping:
     *
     * bottom face button -> SNES B
     * right              -> SNES A
     * left               -> SNES Y
     * top                -> SNES X
     */
    if (controller) {
        if (SDL_GameControllerGetButton(
                controller,
                SDL_CONTROLLER_BUTTON_A))
            input |= SNES_B;

        if (SDL_GameControllerGetButton(
                controller,
                SDL_CONTROLLER_BUTTON_B))
            input |= SNES_A;

        if (SDL_GameControllerGetButton(
                controller,
                SDL_CONTROLLER_BUTTON_X))
            input |= SNES_Y;

        if (SDL_GameControllerGetButton(
                controller,
                SDL_CONTROLLER_BUTTON_Y))
            input |= SNES_X;

        if (SDL_GameControllerGetButton(
                controller,
                SDL_CONTROLLER_BUTTON_BACK))
            input |= SNES_SELECT;

        if (SDL_GameControllerGetButton(
                controller,
                SDL_CONTROLLER_BUTTON_START))
            input |= SNES_START;

        if (SDL_GameControllerGetButton(
                controller,
                SDL_CONTROLLER_BUTTON_DPAD_UP))
            input |= SNES_UP;

        if (SDL_GameControllerGetButton(
                controller,
                SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            input |= SNES_DOWN;

        if (SDL_GameControllerGetButton(
                controller,
                SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            input |= SNES_LEFT;

        if (SDL_GameControllerGetButton(
                controller,
                SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            input |= SNES_RIGHT;

        if (SDL_GameControllerGetButton(
                controller,
                SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
            input |= SNES_L;

        if (SDL_GameControllerGetButton(
                controller,
                SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
            input |= SNES_R;
    }

    return input;
}

static SDL_Rect MakeDisplayRect(
    int output_width,
    int output_height)
{
    /*
     * Present SNES output at a conventional 4:3 display aspect.
     */
    SDL_Rect rect = {0, 0, output_width, output_height};

    const double target_aspect = 4.0 / 3.0;
    const double output_aspect =
        (double)output_width / (double)output_height;

    if (output_aspect > target_aspect) {
        rect.h = output_height;
        rect.w = (int)((double)rect.h * target_aspect);
        rect.x = (output_width - rect.w) / 2;
        rect.y = 0;
    } else {
        rect.w = output_width;
        rect.h = (int)((double)rect.w / target_aspect);
        rect.x = 0;
        rect.y = (output_height - rect.h) / 2;
    }

    return rect;
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(
            stderr,
            "Usage: %s <lufia1.sfc>\n",
            argv[0]);

        return 1;
    }

    uint32_t rom_size = 0;
    uint8_t *rom = ReadFile(argv[1], &rom_size);

    if (!rom)
        return 1;

    fprintf(
        stderr,
        "[host] loaded ROM: %u bytes\n",
        rom_size);

    if (rom_size != 1048576u) {
        fprintf(
            stderr,
            "[host] unexpected ROM size; "
            "expected 1048576 bytes\n");

        free(rom);
        return 1;
    }

    if (SDL_Init(
            SDL_INIT_VIDEO |
            SDL_INIT_EVENTS |
            SDL_INIT_GAMECONTROLLER) != 0) {

        fprintf(
            stderr,
            "[host] SDL_Init failed: %s\n",
            SDL_GetError());

        free(rom);
        return 1;
    }

    SDL_SetHint(
        SDL_HINT_RENDER_SCALE_QUALITY,
        "0");

    SDL_Window *window = SDL_CreateWindow(
        "Lufia I - Estpolis Recompendium - Milestone 1D",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        960,
        720,
        SDL_WINDOW_RESIZABLE |
        SDL_WINDOW_ALLOW_HIGHDPI);

    if (!window) {
        fprintf(
            stderr,
            "[host] SDL_CreateWindow failed: %s\n",
            SDL_GetError());

        SDL_Quit();
        free(rom);
        return 1;
    }

    bool manual_pacing = false;

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED |
        SDL_RENDERER_PRESENTVSYNC);

    if (!renderer) {
        fprintf(
            stderr,
            "[host] accelerated/vsync renderer unavailable: %s\n"
            "[host] falling back to software renderer\n",
            SDL_GetError());

        renderer = SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_SOFTWARE);

        manual_pacing = true;
    }

    if (!renderer) {
        fprintf(
            stderr,
            "[host] SDL_CreateRenderer failed: %s\n",
            SDL_GetError());

        SDL_DestroyWindow(window);
        SDL_Quit();
        free(rom);
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        FRAME_WIDTH,
        FRAME_HEIGHT);

    if (!texture) {
        fprintf(
            stderr,
            "[host] SDL_CreateTexture failed: %s\n",
            SDL_GetError());

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        free(rom);
        return 1;
    }

    SDL_SetTextureBlendMode(
        texture,
        SDL_BLENDMODE_NONE);

    SDL_GameController *controller =
        OpenFirstController();

    /*
     * Register Lufia before SnesInit exactly as required by the
     * SNESRecomp runner.
     */
    RtlRegisterGame(&kLufia1GameInfo);

    Snes *snes = SnesInit(
        rom,
        (int)rom_size);

    if (!snes) {
        fprintf(
            stderr,
            "[host] SnesInit failed\n");

        if (controller)
            SDL_GameControllerClose(controller);

        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        free(rom);

        return 1;
    }

    fprintf(
        stderr,
        "[host] SnesInit succeeded\n"
        "[host] interactive runtime started\n"
        "[host] Esc quits\n"
        "[host] keyboard: arrows, Z/X, A/S, Q/W, "
        "Enter, Backspace\n");

    bool running = true;
    uint64_t presented_frames = 0;

    while (running && !g_fail) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            if (event.type == SDL_KEYDOWN &&
                event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                running = false;
            }
        }

        if (!running)
            break;

        const uint32_t input =
            ReadPlayerInput(controller);

        /*
         * Advance one guest frame.
         *
         * RtlRunFrame() returns false normally in this pinned runner
         * revision, so its return value is intentionally ignored.
         */
        (void)RtlRunFrame(input);

        /*
         * Give the SNES PPU a valid framebuffer, then execute the
         * title-owned PPU/HDMA frame callback.
         */
        PpuBeginDrawing(
            g_ppu,
            g_framebuffer,
            (size_t)FRAME_WIDTH * FRAME_BPP,
            0);

        if (g_rtl_game_info->draw_ppu_frame)
            g_rtl_game_info->draw_ppu_frame();

        if (SDL_UpdateTexture(
                texture,
                NULL,
                g_framebuffer,
                FRAME_WIDTH * FRAME_BPP) != 0) {

            fprintf(
                stderr,
                "[host] SDL_UpdateTexture failed: %s\n",
                SDL_GetError());

            break;
        }

        int output_width = 0;
        int output_height = 0;

        SDL_GetRendererOutputSize(
            renderer,
            &output_width,
            &output_height);

        const SDL_Rect dst =
            MakeDisplayRect(
                output_width,
                output_height);

        SDL_SetRenderDrawColor(
            renderer,
            0,
            0,
            0,
            255);

        SDL_RenderClear(renderer);

        SDL_RenderCopy(
            renderer,
            texture,
            NULL,
            &dst);

        SDL_RenderPresent(renderer);

        /*
         * Normally PRESENTVSYNC provides pacing.
         *
         * The software fallback does not, so keep it approximately
         * real-time rather than allowing the game to run unbounded.
         */
        if (manual_pacing)
            SDL_Delay(16);

        presented_frames++;

        if ((presented_frames % 600) == 0) {
            fprintf(
                stderr,
                "[host] presented %llu frames\n",
                (unsigned long long)presented_frames);

            Lufia1PrintDiagnostics();
        }
    }

    fprintf(
        stderr,
        "[host] runtime stopped after %llu presented frames\n",
        (unsigned long long)presented_frames);

    Lufia1PrintDiagnostics();

    if (controller)
        SDL_GameControllerClose(controller);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    SDL_Quit();

    free(rom);

    return g_fail ? 2 : 0;
}
