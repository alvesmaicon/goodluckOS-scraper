#include "keyboard.h"

#include <cmath>

#include "i18n.h"
#include "imgui.h"
#include "input.h"

static const size_t kMaxLength = 64;

const std::vector<std::vector<Keyboard::Key>>& Keyboard::rows() {
    static const std::vector<std::vector<Key>> layout = [] {
        auto evenRow = [](const char* chars, float x0) {
            std::vector<Key> keys;
            for (const char* c = chars; *c; ++c) keys.push_back({std::string(1, *c), x0++, 1.0f});
            return keys;
        };
        std::vector<std::vector<Key>> rows;
        rows.push_back(evenRow("1234567890", 0.0f));
        rows.push_back(evenRow("qwertyuiop", 0.0f));
        rows.push_back(evenRow("asdfghjkl", 0.5f));
        std::vector<Key> r3 = {{"-", 0.0f, 1.5f}};
        for (const Key& k : evenRow("zxcvbnm", 1.5f)) r3.push_back(k);
        r3.push_back({"del", 8.5f, 1.5f});
        rows.push_back(r3);
        rows.push_back({{"shift", 0.0f, 1.5f}, {"'", 1.5f, 1.0f}, {"(", 2.5f, 1.0f}, {")", 3.5f, 1.0f},
                        {"space", 4.5f, 3.0f}, {".", 7.5f, 1.0f}, {"ok", 8.5f, 1.5f}});
        return rows;
    }();
    return layout;
}

void Keyboard::open(const std::string& p, const std::string& text) {
    prompt = p;
    typed = text;
    caps = false;
    row = 1;
    col = 0;
}

// What a one-character key types with Aa on
std::string Keyboard::shifted(const std::string& key) const {
    static const char* kFrom = "1234567890-'().";
    static const char* kTo = "!@#$%^&*+=_\"[],";
    if (key.size() != 1) return key;
    const char c = key[0];
    if (c >= 'a' && c <= 'z') return std::string(1, (char)(c - 'a' + 'A'));
    for (int i = 0; kFrom[i]; ++i)
        if (kFrom[i] == c) return std::string(1, kTo[i]);
    return key;
}

void Keyboard::move(int dx, int dy) {
    const int n = (int)rows().size();
    if (dx) {
        const int keys = (int)rows()[row].size();
        col = (col + dx + keys) % keys;
    }
    if (dy) {
        // the rows are staggered: land on the key closest to the centre of the current one
        const Key& from = rows()[row][col];
        const float centre = from.x + from.w / 2;
        row = (row + dy + n) % n;
        const auto& to = rows()[row];
        col = 0;
        for (int i = 1; i < (int)to.size(); ++i)
            if (std::fabs(to[i].x + to[i].w / 2 - centre) < std::fabs(to[col].x + to[col].w / 2 - centre)) col = i;
    }
}

Keyboard::Result Keyboard::type(const std::string& key) {
    if (key == "ok") return Result::Done;
    if (key == "shift") {
        caps = !caps;
    } else if (key == "del") {
        // back over one UTF-8 character
        size_t at = typed.size();
        while (at > 0 && (typed[at - 1] & 0xC0) == 0x80) --at;
        if (at > 0) typed.erase(at - 1);
    } else if (typed.size() < kMaxLength) {
        const std::string c = key == "space" ? " " : key;
        typed += caps ? shifted(c) : c;
    }
    return Result::Open;
}

Keyboard::Result Keyboard::update() {
    if (input::pressed(Btn::Up)) move(0, -1);
    if (input::pressed(Btn::Down)) move(0, 1);
    if (input::pressed(Btn::Left)) move(-1, 0);
    if (input::pressed(Btn::Right)) move(1, 0);
    Result result = Result::Open;
    if (input::pressed(Btn::A)) result = type(rows()[row][col].label);
    if (input::pressed(Btn::B)) {
        if (typed.empty()) return Result::Cancelled;
        type("del");
    }
    if (input::pressed(Btn::Start)) return Result::Done;
    if (input::pressed(Btn::Select)) return Result::Cancelled;
    if (result != Result::Open) return result;

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImGuiStyle& style = ImGui::GetStyle();
    const float width = ImGui::GetContentRegionAvail().x;

    ImGui::TextUnformatted(tr(prompt.c_str()));
    // the text as typed, with a caret
    const ImVec2 box = ImGui::GetCursorScreenPos();
    const float boxH = ImGui::GetFrameHeight() + 8;
    dl->AddRectFilled(box, ImVec2(box.x + width, box.y + boxH), ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding);
    const float ty = box.y + (boxH - ImGui::GetFontSize()) / 2;
    dl->AddText(ImVec2(box.x + 10, ty), ImGui::GetColorU32(ImGuiCol_Text), typed.c_str());
    const float caret = box.x + 10 + ImGui::CalcTextSize(typed.c_str()).x + 2;
    dl->AddLine(ImVec2(caret, ty), ImVec2(caret, ty + ImGui::GetFontSize()), ImGui::GetColorU32(ImGuiCol_Text), 2.0f);
    ImGui::Dummy(ImVec2(width, boxH + 10));

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const float unit = width / 10, keyH = 52, gap = 4;
    for (int r = 0; r < (int)rows().size(); ++r) {
        for (int c = 0; c < (int)rows()[r].size(); ++c) {
            const Key& k = rows()[r][c];
            const ImVec2 a(origin.x + k.x * unit + gap / 2, origin.y + r * keyH + gap / 2);
            const ImVec2 b(a.x + k.w * unit - gap, a.y + keyH - gap);
            const bool on = r == row && c == col;
            const bool lit = k.label == "shift" && caps;
            dl->AddRectFilled(a, b, ImGui::GetColorU32(on ? ImGuiCol_ButtonActive : lit ? ImGuiCol_ButtonHovered : ImGuiCol_Button),
                              style.FrameRounding);
            std::string label = k.label == "shift" ? "Aa" : k.label == "del" ? tr("Del") : k.label == "space" ? tr("Space")
                              : k.label == "ok" ? tr("OK") : caps ? shifted(k.label) : k.label;
            const ImVec2 size = ImGui::CalcTextSize(label.c_str());
            dl->AddText(ImVec2((a.x + b.x - size.x) / 2, (a.y + b.y - size.y) / 2), ImGui::GetColorU32(ImGuiCol_Text),
                        label.c_str());
        }
    }
    ImGui::Dummy(ImVec2(width, rows().size() * keyH));
    return Result::Open;
}
