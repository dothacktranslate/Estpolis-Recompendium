#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

namespace fs = std::filesystem;

enum class BuildState {
    NotLoaded,
    Building,
    Ready,
    Failed
};

struct GameEntry {
    const char *id;
    const char *name;
    const char *sha256;
    std::uintmax_t size;
    BuildState state;
    std::string message;
};

static std::array<GameEntry, 4> g_games{{
    {
        "lufia1_us",
        "Lufia & the Fortress of Doom",
        "73731a5a7932965de02a9e98055dcf88b4d17b8f710a6ecfde3e36a1f248773b",
        1048576,
        BuildState::NotLoaded,
        "Not loaded"
    },
    {
        "estopolis1_jp",
        "Estpolis Denki",
        "",
        0,
        BuildState::NotLoaded,
        "Not yet supported"
    },
    {
        "lufia2_us",
        "Lufia II: Rise of the Sinistrals",
        "",
        0,
        BuildState::NotLoaded,
        "Not yet supported"
    },
    {
        "estopolis2_jp",
        "Estpolis Denki II",
        "",
        0,
        BuildState::NotLoaded,
        "Not yet supported"
    }
}};

static std::mutex g_stateMutex;
static std::atomic<bool> g_buildBusy{false};
static std::atomic<bool> g_gameRunning{false};
static int g_selected = 0;

static constexpr SDL_Color TEXT{31, 35, 43, 255};
static constexpr SDL_Color MUTED{105, 112, 124, 255};
static constexpr SDL_Color GREEN{35, 145, 80, 255};
static constexpr SDL_Color RED{188, 60, 55, 255};
static constexpr SDL_Color PANEL{249, 250, 252, 255};
static constexpr SDL_Color BORDER{208, 213, 220, 255};

static std::string shell_quote(const std::string &input)
{
    std::string out = "'";
    for (char c : input) {
        if (c == '\'')
            out += "'\\''";
        else
            out += c;
    }
    out += "'";
    return out;
}

static std::string capture(const std::string &command)
{
    std::array<char, 4096> buffer{};
    std::string result;

    FILE *pipe = popen(command.c_str(), "r");
    if (!pipe)
        return {};

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe))
        result += buffer.data();

    pclose(pipe);

    while (!result.empty() &&
           (result.back() == '\n' || result.back() == '\r'))
        result.pop_back();

    return result;
}

static std::string sha256(const fs::path &path)
{
    return capture(
        "sha256sum " + shell_quote(path.string()) +
        " | awk '{print $1}'");
}

static std::string choose_rom()
{
    return capture(
        "zenity --file-selection "
        "--title='Select SNES ROM' "
        "--file-filter='SNES ROMs | *.sfc *.smc' "
        "2>/dev/null");
}

static TTF_Font *open_font(int pointSize)
{
    static constexpr const char *candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation2/LiberationSans-Regular.ttf"
    };

    for (const char *candidate : candidates) {
        if (fs::exists(candidate)) {
            if (TTF_Font *font = TTF_OpenFont(candidate, pointSize))
                return font;
        }
    }

    return nullptr;
}

static void draw_text(
    SDL_Renderer *renderer,
    TTF_Font *font,
    const std::string &text,
    int x,
    int y,
    SDL_Color color)
{
    if (!font || text.empty())
        return;

    SDL_Surface *surface =
        TTF_RenderUTF8_Blended(font, text.c_str(), color);

    if (!surface)
        return;

    SDL_Texture *texture =
        SDL_CreateTextureFromSurface(renderer, surface);

    if (texture) {
        SDL_Rect dst{x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &dst);
        SDL_DestroyTexture(texture);
    }

    SDL_FreeSurface(surface);
}

static void fill_rect(
    SDL_Renderer *renderer,
    const SDL_Rect &rect,
    SDL_Color color)
{
    SDL_SetRenderDrawColor(
        renderer, color.r, color.g, color.b, color.a);
    SDL_RenderFillRect(renderer, &rect);
}

static void stroke_rect(
    SDL_Renderer *renderer,
    const SDL_Rect &rect,
    SDL_Color color)
{
    SDL_SetRenderDrawColor(
        renderer, color.r, color.g, color.b, color.a);
    SDL_RenderDrawRect(renderer, &rect);
}

static bool contains(const SDL_Rect &rect, int x, int y)
{
    return x >= rect.x &&
           y >= rect.y &&
           x < rect.x + rect.w &&
           y < rect.y + rect.h;
}

static void set_state(
    int index,
    BuildState state,
    const std::string &message)
{
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_games[index].state = state;
    g_games[index].message = message;
}

static std::array<GameEntry, 4> snapshot_games()
{
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_games;
}

static int identify_rom(
    const fs::path &path,
    std::string &hashOut,
    std::uintmax_t &sizeOut)
{
    std::error_code ec;
    sizeOut = fs::file_size(path, ec);

    if (ec)
        return -1;

    hashOut = sha256(path);

    if (hashOut.empty())
        return -1;

    for (int i = 0; i < static_cast<int>(g_games.size()); ++i) {
        const auto &game = g_games[i];

        if (game.sha256[0] != '\0' &&
            hashOut == game.sha256 &&
            sizeOut == game.size)
            return i;
    }

    return -1;
}

static void detect_existing_lufia1()
{
    const fs::path rom =
        "local/roms/Lufia & the Fortress of Doom (USA).sfc";

    if (!fs::exists(rom))
        return;

    std::string hash;
    std::uintmax_t size = 0;

    if (identify_rom(rom, hash, size) != 0)
        return;

    if (fs::exists("local/build/lufia1/lufia1")) {
        set_state(0, BuildState::Ready, "Ready to play");
    } else {
        set_state(
            0,
            BuildState::NotLoaded,
            "ROM imported; select or drop it to build");
    }
}

static void import_rom(const std::string &fileName)
{
    if (g_buildBusy || fileName.empty())
        return;

    const fs::path source(fileName);

    if (!fs::is_regular_file(source)) {
        set_state(g_selected, BuildState::Failed, "Selected path is not a file");
        return;
    }

    std::string hash;
    std::uintmax_t size = 0;
    const int game = identify_rom(source, hash, size);

    if (game < 0) {
        std::string shortHash =
            hash.empty() ? "unavailable" : hash.substr(0, 16) + "...";

        set_state(
            g_selected,
            BuildState::Failed,
            "Unsupported ROM (SHA-256 " + shortHash + ")");
        return;
    }

    g_selected = game;

    if (game != 0) {
        set_state(
            game,
            BuildState::Failed,
            "Recognized, but this target is not implemented yet");
        return;
    }

    g_buildBusy = true;
    set_state(0, BuildState::Building, "Building...");

    std::thread([source]() {
        const std::string command =
            "launcher/scripts/build_lufia1.sh " +
            shell_quote(source.string());

        const int result = std::system(command.c_str());

        if (result == 0)
            set_state(0, BuildState::Ready, "Ready to play");
        else
            set_state(
                0,
                BuildState::Failed,
                "Build failed — see local/logs/launcher-lufia1-build.log");

        g_buildBusy = false;
    }).detach();
}

static void launch_selected_game()
{
    if (g_gameRunning)
        return;

    const auto games = snapshot_games();

    if (g_selected != 0 ||
        games[g_selected].state != BuildState::Ready)
        return;

    g_gameRunning = true;

    std::thread([]() {
        std::system("launcher/scripts/run_lufia1.sh");
        g_gameRunning = false;
    }).detach();
}

static const char *state_label(BuildState state)
{
    switch (state) {
    case BuildState::NotLoaded:
        return "Not loaded";
    case BuildState::Building:
        return "Building...";
    case BuildState::Ready:
        return "Loaded / Built";
    case BuildState::Failed:
        return "Problem";
    }

    return "";
}

int main()
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        std::cerr << "SDL_Init failed: "
                  << SDL_GetError() << '\n';
        return 1;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        std::cerr << "SDL_image could not initialize PNG support\n";
        SDL_Quit();
        return 1;
    }

    if (TTF_Init() != 0) {
        std::cerr << "SDL_ttf initialization failed: "
                  << TTF_GetError() << '\n';
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow(
        "Estopolis Recompendium",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1120,
        760,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    if (!window) {
        std::cerr << "SDL_CreateWindow failed: "
                  << SDL_GetError() << '\n';
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED |
            SDL_RENDERER_PRESENTVSYNC);

    if (!renderer) {
        std::cerr << "SDL_CreateRenderer failed: "
                  << SDL_GetError() << '\n';
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 1;
    }

    TTF_Font *font = open_font(18);
    TTF_Font *small = open_font(14);
    TTF_Font *heading = open_font(22);

    SDL_Texture *logo = nullptr;
    int logoW = 0;
    int logoH = 0;

    if (SDL_Surface *surface =
            IMG_Load("launcher/assets/estopolis-recompendium.png")) {

        logo = SDL_CreateTextureFromSurface(renderer, surface);
        logoW = surface->w;
        logoH = surface->h;
        SDL_FreeSurface(surface);
    }

    SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
    detect_existing_lufia1();

    bool running = true;

    while (running) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_DROPFILE) {
                const std::string file =
                    event.drop.file ? event.drop.file : "";

                SDL_free(event.drop.file);
                import_rom(file);
            } else if (
                event.type == SDL_MOUSEBUTTONUP &&
                event.button.button == SDL_BUTTON_LEFT) {

                int windowW = 0;
                int windowH = 0;
                SDL_GetWindowSize(window, &windowW, &windowH);

                const int mx = event.button.x;
                const int my = event.button.y;

                const SDL_Rect selectButton{
                    windowW / 2 - 90,
                    276,
                    180,
                    44
                };

                if (contains(selectButton, mx, my) &&
                    !g_buildBusy) {

                    const std::string file = choose_rom();

                    if (!file.empty())
                        import_rom(file);
                }

                const int listX = 35;
                const int listY = 390;
                const int listW = windowW / 2 + 70;
                const int rowH = 56;

                for (int i = 0; i < 4; ++i) {
                    const SDL_Rect row{
                        listX,
                        listY + i * rowH,
                        listW,
                        rowH - 4
                    };

                    if (contains(row, mx, my))
                        g_selected = i;
                }

                const SDL_Rect playButton{
                    windowW / 2 + 130,
                    windowH - 105,
                    windowW / 2 - 170,
                    54
                };

                if (contains(playButton, mx, my))
                    launch_selected_game();
            }
        }

        int windowW = 0;
        int windowH = 0;
        SDL_GetWindowSize(window, &windowW, &windowH);

        SDL_SetRenderDrawColor(
            renderer, 242, 244, 247, 255);
        SDL_RenderClear(renderer);

        if (logo && logoW > 0 && logoH > 0) {
            const int maxW = 500;

            const double scale =
                std::min(
                    1.0,
                    static_cast<double>(maxW) /
                        static_cast<double>(logoW));

            const int drawW =
                static_cast<int>(logoW * scale);
            const int drawH =
                static_cast<int>(logoH * scale);

            SDL_Rect dst{
                windowW / 2 - drawW / 2,
                18,
                drawW,
                drawH
            };

            SDL_RenderCopy(
                renderer, logo, nullptr, &dst);
        } else {
            draw_text(
                renderer,
                heading,
                "Estopolis Recompendium",
                windowW / 2 - 120,
                42,
                TEXT);
        }

        SDL_Rect dropPanel{
            35,
            200,
            windowW - 70,
            145
        };

        fill_rect(
            renderer,
            dropPanel,
            {250, 251, 252, 255});
        stroke_rect(renderer, dropPanel, BORDER);

        draw_text(
            renderer,
            heading,
            "Drop ROM here",
            windowW / 2 - 75,
            220,
            TEXT);

        draw_text(
            renderer,
            small,
            "or select a ROM file",
            windowW / 2 - 67,
            250,
            MUTED);

        SDL_Rect selectButton{
            windowW / 2 - 90,
            276,
            180,
            44
        };

        fill_rect(
            renderer,
            selectButton,
            g_buildBusy
                ? SDL_Color{225, 228, 232, 255}
                : SDL_Color{247, 248, 250, 255});

        stroke_rect(renderer, selectButton, BORDER);

        draw_text(
            renderer,
            font,
            "Select ROM",
            selectButton.x + 42,
            selectButton.y + 10,
            g_buildBusy ? MUTED : TEXT);

        const int listX = 35;
        const int listY = 390;
        const int listW = windowW / 2 + 70;
        const int rowH = 56;

        SDL_Rect gamesPanel{
            listX - 10,
            listY - 38,
            listW + 20,
            rowH * 4 + 50
        };

        fill_rect(renderer, gamesPanel, PANEL);
        stroke_rect(renderer, gamesPanel, BORDER);

        draw_text(
            renderer,
            heading,
            "Games",
            gamesPanel.x + 12,
            gamesPanel.y + 7,
            TEXT);

        const auto games = snapshot_games();

        for (int i = 0; i < 4; ++i) {
            SDL_Rect row{
                listX,
                listY + i * rowH,
                listW,
                rowH - 4
            };

            if (i == g_selected) {
                fill_rect(
                    renderer,
                    row,
                    {232, 242, 235, 255});
            }

            draw_text(
                renderer,
                font,
                games[i].name,
                row.x + 14,
                row.y + 13,
                TEXT);

            SDL_Color statusColor = MUTED;

            if (games[i].state == BuildState::Ready)
                statusColor = GREEN;
            else if (games[i].state == BuildState::Failed)
                statusColor = RED;

            draw_text(
                renderer,
                font,
                games[i].state == BuildState::Ready ? "✓" : "—",
                row.x + row.w - 155,
                row.y + 12,
                statusColor);

            draw_text(
                renderer,
                small,
                state_label(games[i].state),
                row.x + row.w - 125,
                row.y + 16,
                statusColor);
        }

        SDL_Rect infoPanel{
            windowW / 2 + 130,
            listY - 38,
            windowW / 2 - 165,
            rowH * 4 + 50
        };

        fill_rect(renderer, infoPanel, PANEL);
        stroke_rect(renderer, infoPanel, BORDER);

        draw_text(
            renderer,
            heading,
            "ROM Information",
            infoPanel.x + 14,
            infoPanel.y + 7,
            TEXT);

        const GameEntry &selected = games[g_selected];

        draw_text(
            renderer,
            font,
            selected.name,
            infoPanel.x + 14,
            infoPanel.y + 55,
            TEXT);

        SDL_Color messageColor = MUTED;

        if (selected.state == BuildState::Ready)
            messageColor = GREEN;
        else if (selected.state == BuildState::Failed)
            messageColor = RED;

        draw_text(
            renderer,
            small,
            selected.message,
            infoPanel.x + 14,
            infoPanel.y + 90,
            messageColor);

        if (selected.sha256[0] != '\0') {
            draw_text(
                renderer,
                small,
                std::string("SHA-256: ") +
                    std::string(selected.sha256).substr(0, 20) +
                    "...",
                infoPanel.x + 14,
                infoPanel.y + 125,
                MUTED);

            draw_text(
                renderer,
                small,
                "Expected size: " +
                    std::to_string(selected.size) +
                    " bytes",
                infoPanel.x + 14,
                infoPanel.y + 153,
                MUTED);
        }

        SDL_Rect playButton{
            windowW / 2 + 130,
            windowH - 105,
            windowW / 2 - 170,
            54
        };

        const bool canPlay =
            selected.state == BuildState::Ready &&
            !g_gameRunning;

        fill_rect(
            renderer,
            playButton,
            canPlay
                ? GREEN
                : SDL_Color{184, 190, 198, 255});

        draw_text(
            renderer,
            heading,
            g_gameRunning ? "Game Running..." : "Play",
            playButton.x +
                (g_gameRunning
                    ? playButton.w / 2 - 80
                    : playButton.w / 2 - 22),
            playButton.y + 13,
            {255, 255, 255, 255});

        draw_text(
            renderer,
            small,
            "Closing the game returns you to the launcher.",
            45,
            windowH - 34,
            MUTED);

        SDL_RenderPresent(renderer);
    }

    if (logo)
        SDL_DestroyTexture(logo);

    if (heading)
        TTF_CloseFont(heading);
    if (small)
        TTF_CloseFont(small);
    if (font)
        TTF_CloseFont(font);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    IMG_Quit();
    SDL_Quit();

    return 0;
}
