#pragma once
// On-screen keyboard for the account, driven by the D-pad like the launcher's: A types, B deletes (and closes when the
// text is empty), START is done, SELECT cancels. Aa switches the case and the symbols, which passwords need. QWERTY
// like Android's: rows are staggered and some keys are wider, so positions and widths are in key units (10 per row).
#include <string>
#include <vector>

class Keyboard {
public:
    enum class Result { Open, Done, Cancelled };

    void open(const std::string& prompt, const std::string& text);
    Result update();        // draws it in the current window and takes the buttons
    const std::string& text() const { return typed; }

private:
    struct Key {
        std::string label;
        float x, w;     // in key units
    };
    static const std::vector<std::vector<Key>>& rows();
    std::string shifted(const std::string& key) const;
    void move(int dx, int dy);
    Result type(const std::string& key);

    std::string prompt, typed;
    bool caps = false;
    int row = 1, col = 0;   // starts on Q
};
