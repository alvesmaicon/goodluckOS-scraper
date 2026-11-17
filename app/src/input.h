#pragma once
// The console's buttons through ImGui's gamepad keys, and a PC keyboard's for testing.
#include "imgui.h"

enum class Btn { Up, Down, Left, Right, A, B, X, Y, Start, Select };

namespace input {
inline bool enabled = true;     // off while a press that woke the screen is still held

inline bool pressed(Btn b) {
    if (!enabled) return false;
    auto any = [](ImGuiKey pad, ImGuiKey key) { return ImGui::IsKeyPressed(pad, true) || ImGui::IsKeyPressed(key, true); };
    switch (b) {
        case Btn::Up:     return any(ImGuiKey_GamepadDpadUp, ImGuiKey_UpArrow);
        case Btn::Down:   return any(ImGuiKey_GamepadDpadDown, ImGuiKey_DownArrow);
        case Btn::Left:   return any(ImGuiKey_GamepadDpadLeft, ImGuiKey_LeftArrow);
        case Btn::Right:  return any(ImGuiKey_GamepadDpadRight, ImGuiKey_RightArrow);
        case Btn::A:      return any(ImGuiKey_GamepadFaceDown, ImGuiKey_Enter);
        case Btn::B:      return any(ImGuiKey_GamepadFaceRight, ImGuiKey_Escape);
        case Btn::X:      return any(ImGuiKey_GamepadFaceLeft, ImGuiKey_F1);
        case Btn::Y:      return any(ImGuiKey_GamepadFaceUp, ImGuiKey_F2);
        case Btn::Start:  return any(ImGuiKey_GamepadStart, ImGuiKey_F3);
        case Btn::Select: return any(ImGuiKey_GamepadBack, ImGuiKey_F4);
    }
    return false;
}
}  // namespace input
