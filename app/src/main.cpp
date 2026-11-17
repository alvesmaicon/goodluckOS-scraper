// The scraper's screen: the ScreenScraper account, what to get and from which systems, and the run, with gl-scrape
// doing the work. ImGui on SDL2's renderer like goodluckOS's System Settings, rows like the launcher's settings.
#include <SDL2/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_sdlrenderer2.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include <limits.h>
#include <unistd.h>

#include "device.h"
#include "i18n.h"
#include "input.h"
#include "keyboard.h"
#include "scraper.h"

static const char* kFonts[] = {"/usr/share/fonts/NotoSans.ttf", "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf"};

// ScreenScraper's media types and regions as gl-scrape takes them, with their names here
static const std::vector<std::pair<const char*, const char*>> kPictures = {
    {"mixrbv2", "Mix"}, {"box-2D", "Box"}, {"box-3D", "3D box"}, {"ss", "Screenshot"},
    {"sstitle", "Title screen"}, {"wheel", "Logo"}, {"none", "None"}};
static const std::vector<std::pair<const char*, const char*>> kRegions = {
    {"auto", "Auto"}, {"br", "Brazil"}, {"us", "USA"}, {"eu", "Europe"}, {"jp", "Japan"}, {"wor", "World"}};

enum class Page { Main, Systems, Run, Typing };

struct State {
    Page page = Page::Main;
    ScraperSettings settings = ScraperSettings::load();
    std::vector<ScraperFolder> folders;
    bool foldersRead = false;
    ScraperRun run = ScraperRun::read(), check;
    bool watchRun = false, watchCheck = false;      // started, its status not written yet
    Uint32 watchSince = 0, readAt = 0, switchedAt = 0;
    NetStatus net = NetStatus::read();
    Keyboard keyboard;
    bool typingPassword = false, wasBusy = false;
    std::string focus = "internet";                 // the row to select on the next frame
    std::string hint;
    bool quit = false;

    bool busy() const { return run.running() || (watchRun && run.state.empty()); }
    bool account() const { return !settings.user.empty() && !settings.password.empty(); }
    int chosen() const {
        int n = 0;
        for (const ScraperFolder& f : folders)
            n += std::find(settings.skip.begin(), settings.skip.end(), f.folder) == settings.skip.end();
        return n;
    }
    void open(Page p, const char* first) {
        page = p;
        focus = first;
    }
};

// How far a run is through the games of all its systems, e.g. 10%
static std::string share(const ScraperRun& run) {
    return run.games ? std::to_string(run.done * 100 / run.games) + "%" : "";
}

// The text cut with "..." to fit the width, on a UTF-8 character boundary
static std::string fit(const std::string& text, float width) {
    if (ImGui::CalcTextSize(text.c_str()).x <= width) return text;
    std::string cut = text;
    while (!cut.empty() && ImGui::CalcTextSize((cut + "...").c_str()).x > width) {
        cut.pop_back();
        while (!cut.empty() && (cut.back() & 0xC0) == 0x80) cut.pop_back();
        if (!cut.empty() && (unsigned char)cut.back() >= 0xC0) cut.pop_back();
    }
    return cut + "...";
}

// One line of a page: its label on the left, its value on the right, and its hint under the page while it's
// selected. True when A picks it. A dim row can still be selected, to show in its hint why it does nothing.
static bool row(State& s, const char* id, const std::string& label, const std::string& value, const char* hint = nullptr,
                bool dim = false) {
    if (s.focus == id) {
        ImGui::SetKeyboardFocusHere();
        s.focus.clear();
    }
    const ImVec2 at = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x, h = ImGui::GetFrameHeight(), pad = 10;
    ImGui::PushID(id);
    const bool picked = ImGui::Selectable("##row", false, ImGuiSelectableFlags_None, ImVec2(w, h));
    ImGui::PopID();
    const bool focused = ImGui::IsItemFocused();
    if (focused && hint) s.hint = tr(hint);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    if (focused) dl->AddRectFilled(at, ImVec2(at.x + w, at.y + h), ImGui::GetColorU32(ImGuiCol_HeaderHovered));
    const float y = at.y + (h - ImGui::GetFontSize()) / 2;
    const float labelW = ImGui::CalcTextSize(label.c_str()).x;
    dl->AddText(ImVec2(at.x + pad, y), ImGui::GetColorU32(dim ? ImGuiCol_TextDisabled : ImGuiCol_Text), label.c_str());
    if (!value.empty()) {
        const std::string v = fit(value, w - labelW - 4 * pad);
        const float vw = ImGui::CalcTextSize(v.c_str()).x;
        dl->AddText(ImVec2(at.x + w - vw - pad, y), ImGui::GetColorU32(focused ? ImGuiCol_Text : ImGuiCol_PlotLines), v.c_str());
    }
    return picked && !dim && input::enabled;
}

// A row whose value LEFT/RIGHT (or A) steps through the names
static bool choice(State& s, const char* id, const char* label, const std::vector<std::pair<const char*, const char*>>& list,
                   std::string& key, const char* hint) {
    int i = 0;
    while (i < (int)list.size() && key != list[i].first) ++i;
    if (i == (int)list.size()) i = 0;
    int step = row(s, id, tr(label), std::string("<  ") + tr(list[i].second) + "  >", hint) ? 1 : 0;
    if (ImGui::IsItemFocused()) {
        if (input::pressed(Btn::Left)) step = -1;
        if (input::pressed(Btn::Right)) step = 1;
    }
    if (!step) return false;
    key = list[(i + step + list.size()) % list.size()].first;
    return true;
}

static bool toggle(State& s, const char* id, const std::string& label, bool& on, const char* hint) {
    bool flip = row(s, id, label, on ? tr("On") : tr("Off"), hint);
    if (ImGui::IsItemFocused() && (input::pressed(Btn::Left) || input::pressed(Btn::Right))) flip = true;
    if (flip) on = !on;
    return flip;
}

// A line that can't be selected
static void info(const std::string& label, const std::string& value) {
    const float w = ImGui::GetContentRegionAvail().x, pad = 10;
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(w, ImGui::GetFrameHeight()));
    const float y = at.y + (ImGui::GetFrameHeight() - ImGui::GetFontSize()) / 2;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(ImVec2(at.x + pad, y), ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
    const std::string v = fit(value, w - ImGui::CalcTextSize(label.c_str()).x - 4 * pad);
    dl->AddText(ImVec2(at.x + w - ImGui::CalcTextSize(v.c_str()).x - pad, y), ImGui::GetColorU32(ImGuiCol_PlotLines),
                v.c_str());
}

static void start(State& s) {
    startScraper();
    s.run = {};
    s.watchRun = true;
    s.watchSince = SDL_GetTicks();
    s.open(Page::Run, "stop");
}

static void mainPage(State& s) {
    const bool online = s.net.online(), switching = s.switchedAt && !online && SDL_GetTicks() - s.switchedAt < 15000;
    const char* needs = !online ? "Needs the internet: see Internet, above."
                      : !s.account() ? "Type your ScreenScraper account first: it's free at screenscraper.fr." : nullptr;

    const std::string net = online ? (s.net.address.empty() ? tr("Online") : trf("Online, %s", s.net.address))
                          : switching ? tr("Switching...") : tr("Offline");
    const char* netHint = online ? "ScreenScraper is reached through this connection."
                        : s.net.usbNetwork ? "The PC or phone on the USB cable has to share its connection: Internet Sharing on a Mac, "
                                             "the adapter's Sharing tab on Windows, Ethernet tethering on a phone."
                        : s.net.canSwitch ? "A sets the USB cable to Network, so the PC or phone on it can share its connection. "
                                            "HOME stops showing up on it as a drive until the cable is set back to Files."
                        : "The console needs the internet to reach ScreenScraper.";
    if (row(s, "internet", tr("Internet"), net, netHint) && !online && !s.net.usbNetwork && s.net.canSwitch && !switching) {
        switchUsbToNetwork();
        s.switchedAt = SDL_GetTicks();
    }

    ImGui::SeparatorText(tr("ScreenScraper account"));
    if (row(s, "user", tr("User"), s.settings.user.empty() ? tr("not set") : s.settings.user,
            "Free at screenscraper.fr. Without one, ScreenScraper often turns the scraper away.")) {
        s.keyboard.open("User:", s.settings.user);
        s.typingPassword = false;
        s.page = Page::Typing;
    }
    const std::string stars(std::min<size_t>(s.settings.password.size(), 12), '*');
    if (row(s, "password", tr("Password"), stars.empty() ? tr("not set") : stars)) {
        s.keyboard.open("Password:", s.settings.password);
        s.typingPassword = true;
        s.page = Page::Typing;
    }
    std::string checked;
    if (s.watchCheck) checked = tr("checking...");
    else if (s.check.state == "done") checked = trf("OK, %s requests today", s.check.requests);
    else if (!s.check.state.empty()) checked = tr(s.check.message);
    if (row(s, "check", tr("Check the account"), checked, needs ? needs : "Logs in and shows how many of today's requests are used.",
            !s.account() || !online || s.watchCheck)) {
        checkScraperAccount();
        s.check = {};
        s.watchCheck = true;
        s.watchSince = SDL_GetTicks();
    }

    ImGui::SeparatorText(tr("What to get"));
    std::string games = s.settings.all ? "all" : "missing";
    if (choice(s, "games", "Games", {{"missing", "Missing only"}, {"all", "All"}}, games,
               "Missing only fills in what the games lack; All replaces it. Answers already received are kept, so asking "
               "again costs no requests.")) {
        s.settings.all = games == "all";
        s.settings.save();
    }
    if (choice(s, "picture", "Picture", kPictures, s.settings.image, "Mix puts the screenshot, the box and the logo in one picture."))
        s.settings.save();
    if (toggle(s, "videos", tr("Videos"), s.settings.video, "Short gameplay clips, a few MB each.")) s.settings.save();
    if (choice(s, "region", "Region", kRegions, s.settings.region,
               "Whose names, covers and dates come first. Auto follows the file name, e.g. (USA)."))
        s.settings.save();
    const std::string systems = !s.foldersRead ? tr("looking for games...")
                              : trf("%s of %s", std::to_string(s.chosen()), std::to_string(s.folders.size()));
    if (row(s, "systems", tr("Systems"), systems, "The ROM folders of the launcher's tabs, and its Games tab.", !s.foldersRead))
        s.open(Page::Systems, "system0");

    ImGui::SeparatorText(tr("Scrape"));
    if (s.busy()) {
        if (row(s, "scraping", tr("Scraping"), share(s.run), "The app has to stay open while it scrapes.")) s.open(Page::Run, "stop");
    } else {
        const std::string last = s.run.state == "done" ? tr("Done") : s.run.state == "stopped" ? tr("Stopped")
                               : s.run.state == "error" ? tr("Error") : "";
        const bool ready = s.account() && online && s.foldersRead && s.chosen() > 0;
        if (row(s, "start", tr("Start"), last,
                needs ? needs : "The app has to stay open while it scrapes; Screen off, on the next page, keeps it going in the dark.",
                !ready))
            start(s);
        if (!s.run.state.empty() && row(s, "last", tr("Last run"), "")) s.open(Page::Run, "start");
    }
    if (row(s, "close", tr("Close"), "", s.busy() ? "Closing the app stops the scraper. Start goes on from where it stopped." : nullptr))
        s.quit = true;
}

// The systems with games, each one on or off
static void systemsPage(State& s) {
    if (s.folders.empty()) info(tr("No games in the ROM folders"), "");
    for (size_t i = 0; i < s.folders.size(); ++i) {
        const ScraperFolder& f = s.folders[i];
        auto& skip = s.settings.skip;
        bool on = std::find(skip.begin(), skip.end(), f.folder) == skip.end();
        // gl-scrape's "games" is the Games tab, whose gamelist is in the ports folder
        const std::string label = f.folder == "games" ? tr(f.name) : f.name + "  (" + f.folder + ")";
        const std::string id = "system" + std::to_string(i);
        if (toggle(s, id.c_str(), label, on, nullptr)) {
            skip.erase(std::remove(skip.begin(), skip.end(), f.folder), skip.end());
            if (!on) skip.push_back(f.folder);
            s.settings.save();
        }
    }
}

// A run under way, or the last one: Stop first, its progress under it
static void runPage(State& s) {
    const bool busy = s.busy();
    if (busy) {
        if (row(s, "stop", tr("Stop"), "")) stopScraper(s.run);
        if (row(s, "dark", tr("Screen off"), "",
                "Turns the screen off and goes on scraping; any button turns it back on. POWER would pause the scraper.") &&
            screen::off())
            input::enabled = false;
    } else if (row(s, "start", tr("Start"), "", nullptr, !s.account() || !s.net.online())) {
        start(s);
    }
    if (s.run.state.empty()) {
        info(tr("Status"), tr("Starting..."));
        return;
    }
    const ScraperRun& r = s.run;
    const std::string state = r.state == "done" ? tr("Done") : r.state == "stopped" ? tr("Stopped")
                            : r.state == "error" ? tr("Error")
                            : r.steps > 1 ? tr(r.system) + " (" + trf("%s of %s", std::to_string(r.step), std::to_string(r.steps)) + ")"
                            : tr(r.system);
    info(tr("Status"), state);
    if (r.state == "error") {
        // ScreenScraper's own words too: they are long, so on lines of their own
        ImGui::Indent(10);
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
        ImGui::TextUnformatted(tr(r.message.c_str()));
        if (!r.detail.empty()) ImGui::TextDisabled("%s", r.detail.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Unindent(10);
    }
    if (r.games > 0) {
        info(tr("Total"), trf("%s of %s", std::to_string(r.done), std::to_string(r.games)) + " (" + share(r) + ")");
        ImGui::ProgressBar((float)r.done / r.games, ImVec2(-FLT_MIN, 8), "");
    }
    if (busy && r.total > 0) {
        info(tr("Game"), trf("%s of %s", std::to_string(r.index), std::to_string(r.total)));
        info(fit(r.game, ImGui::GetContentRegionAvail().x - 20), "");
    }
    info(tr("Scraped"), std::to_string(r.found));
    if (r.notFound) info(tr("Not found"), std::to_string(r.notFound));
    if (r.failed) info(tr("Failed"), std::to_string(r.failed));
    if (r.skipped) info(tr("Already complete"), std::to_string(r.skipped));
    if (!r.requests.empty()) info(tr("Requests today"), r.requests);
}

static void typingPage(State& s) {
    const Keyboard::Result r = s.keyboard.update();
    if (r == Keyboard::Result::Open) return;
    if (r == Keyboard::Result::Done) {
        (s.typingPassword ? s.settings.password : s.settings.user) = s.keyboard.text();
        s.settings.save();
        s.check = {};
    }
    s.open(Page::Main, s.typingPassword ? "password" : "user");
}

static const char* title(Page p) {
    switch (p) {
        case Page::Systems: return "Systems to scrape";
        case Page::Run: return "Scraping";
        case Page::Typing: return "ScreenScraper account";
        default: return "Scraper";
    }
}

static void frame(State& s) {
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Scraper", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PushFont(nullptr, ImGui::GetStyle().FontSizeBase * 1.2f);
    ImGui::TextUnformatted(tr(title(s.page)));
    ImGui::PopFont();
    ImGui::Separator();

    if (s.page == Page::Typing) {
        typingPage(s);
        ImGui::TextDisabled("%s", tr("A type   B delete   START done   SELECT cancel"));
        ImGui::End();
        return;
    }

    // B goes back to the first page, to the row that left it
    if (s.page != Page::Main && input::pressed(Btn::B)) s.open(Page::Main, s.page == Page::Systems ? "systems" : s.busy() ? "scraping" : "start");

    // when a run ends, its Stop or Scraping row goes: Start takes its place
    if (s.wasBusy && !s.busy() && s.page != Page::Systems) s.focus = "start";
    s.wasBusy = s.busy();

    const float hintH = ImGui::GetTextLineHeightWithSpacing() * 2 + ImGui::GetStyle().ItemSpacing.y * 2;
    s.hint.clear();
    ImGui::BeginChild("rows", ImVec2(0, -hintH), ImGuiChildFlags_NavFlattened);
    if (s.page == Page::Main) mainPage(s);
    else if (s.page == Page::Systems) systemsPage(s);
    else runPage(s);
    ImGui::EndChild();
    ImGui::Separator();
    ImGui::PushTextWrapPos(0);
    ImGui::TextDisabled("%s", s.hint.c_str());
    ImGui::PopTextWrapPos();
    ImGui::End();
}

static std::string appDir() {
    char path[PATH_MAX];
    const ssize_t n = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (n <= 0) return ".";
    std::string dir(path, n);
    return dir.substr(0, dir.rfind('/'));
}

// Whether any button or key is still held, after the press that woke the screen
static bool held() {
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        SDL_GameController* pad = SDL_GameControllerFromInstanceID(SDL_JoystickGetDeviceInstanceID(i));
        for (int b = 0; pad && b < SDL_CONTROLLER_BUTTON_MAX; ++b)
            if (SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)b)) return true;
    }
    int n = 0;
    const Uint8* keys = SDL_GetKeyboardState(&n);
    for (int k = 0; k < n; ++k)
        if (keys[k]) return true;
    return false;
}

int main(int, char**) {
    // gl-scrape and the app's own curl, with its certificates: goodluckOS has no curl
    const std::string dir = appDir();
    const char* path = getenv("PATH");
    setenv("PATH", (dir + "/bin:" + (path ? path : "/usr/bin:/bin")).c_str(), 1);
    if (access((dir + "/cacert.pem").c_str(), R_OK) == 0) setenv("CURL_CA_BUNDLE", (dir + "/cacert.pem").c_str(), 1);
    i18n::load(dir);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) return 1;
    SDL_Window* window = SDL_CreateWindow("Scraper", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC | SDL_RENDERER_ACCELERATED);
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
        if (SDL_IsGameController(i)) SDL_GameControllerOpen(i);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigNavCursorVisibleAlways = true;
    ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer2_Init(renderer);
    for (const char* font : kFonts)
        if (access(font, R_OK) == 0 && io.Fonts->AddFontFromFileTTF(font, 22.0f)) break;
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontSizeBase = 22.0f;
    if (io.Fonts->Fonts.empty()) style.FontScaleMain = 1.65f;
    style.FramePadding = ImVec2(8, 6);
    style.ItemSpacing = ImVec2(8, 2);
    style.SeparatorTextPadding = ImVec2(10, 4);
    style.SeparatorTextBorderSize = 2;

    State s;
    int frames = 2;
    while (!s.quit) {
        SDL_Event e;
        const Uint32 wait = screen::isOff() ? 2000 : s.busy() || s.watchCheck ? 500 : 1000;
        if (SDL_WaitEventTimeout(&e, frames > 0 ? 1 : wait)) {
            do {
                const bool press = e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_KEYDOWN;
                if (press && screen::isOff()) {
                    screen::on();       // that press only wakes the screen
                    input::enabled = false;
                    continue;
                }
                ImGui_ImplSDL2_ProcessEvent(&e);
                if (e.type == SDL_QUIT) s.quit = true;
                frames = 2;
            } while (SDL_PollEvent(&e));
        }

        const Uint32 now = SDL_GetTicks();
        if (now - s.readAt >= 500) {
            s.readAt = now;
            s.net = NetStatus::read();
            s.run = ScraperRun::read();
            if (s.watchCheck) s.check = ScraperRun::readCheck();
            if (s.watchRun && (!s.run.state.empty() || now - s.watchSince > 5000)) s.watchRun = false;
            if (s.watchCheck && (!s.check.state.empty() && s.check.state != "running")) s.watchCheck = false;
            if (s.watchCheck && now - s.watchSince > 60000) s.watchCheck = false;
            frames = std::max(frames, 1);
        }
        if (screen::isOff()) continue;
        if (!input::enabled && !held()) input::enabled = true;
        io.ConfigFlags = input::enabled ? io.ConfigFlags | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard
                                        : io.ConfigFlags & ~(ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NavEnableKeyboard);
        if (frames <= 0) continue;
        --frames;

        ImGui_ImplSDLRenderer2_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        frame(s);
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);

        // the systems take a moment: read once the first page is up
        if (!s.foldersRead) {
            s.folders = scraperFolders();
            s.foldersRead = true;
            frames = 2;
        }
    }

    // a run can't go on without the app: it stops, and Start goes on from there next time
    if (s.run.running()) stopScraper(s.run);
    screen::on();
    ImGui_ImplSDLRenderer2_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
