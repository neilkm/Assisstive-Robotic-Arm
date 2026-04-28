#include <opencv2/freetype.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

struct StateDefinition {
    std::string name;
    std::vector<std::string> actions;
};

struct FontRenderer {
    cv::Ptr<cv::freetype::FreeType2> freetype;
    bool useArial = false;
};

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr char kWindowName[] = "Jetson Nano State UI";
constexpr int kKeyUp = 1001;
constexpr int kKeyDown = 1002;
constexpr int kKeyLeft = 1003;
constexpr int kKeyRight = 1004;

std::vector<StateDefinition> buildStates() {
    return {
        {"Init", {"select spice", "select utensil"}},
        {"Selecting spice", {"spice 1", "spice 2", "spice 3", "spice 4", "spice 5", "spice 6", "spice 7", "spice 8", "spice 9"}},
        {"Spice selected", {"shake into pot", "put down"}},
        {"Shaking spice into pot", {"more spice", "less spice", "put down"}},
        {"Selecting utensil", {"utensil 1", "utensil 2", "utensil 3", "utensil 4", "utensil 5"}},
        {"Utensil selected", {"use utensil", "put down"}},
        {"Using utensil", {"stir faster", "stir slower", "put down"}},
    };
}

FontRenderer createFontRenderer() {
    const std::vector<std::string> fontCandidates = {
        "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
    };

    FontRenderer renderer;

    try {
        renderer.freetype = cv::freetype::createFreeType2();
        for (const std::string& fontPath : fontCandidates) {
            try {
                renderer.freetype->loadFontData(fontPath, 0);
                renderer.useArial = true;
                return renderer;
            } catch (const cv::Exception&) {
            }
        }
    } catch (const cv::Exception&) {
    }

    renderer.freetype.release();
    renderer.useArial = false;
    return renderer;
}

void drawText(cv::Mat& frame,
              const FontRenderer& fontRenderer,
              const std::string& text,
              cv::Point origin,
              int fontHeight,
              const cv::Scalar& color,
              int thickness) {
    if (fontRenderer.useArial && fontRenderer.freetype) {
        fontRenderer.freetype->putText(frame, text, origin, fontHeight, color, thickness, cv::LINE_AA, true);
        return;
    }

    const double scale = static_cast<double>(fontHeight) / 30.0;
    cv::putText(frame, text, origin, cv::FONT_HERSHEY_DUPLEX, scale, color, thickness, cv::LINE_AA);
}

void drawHeader(cv::Mat& frame, const FontRenderer& fontRenderer, const StateDefinition& state) {
    drawText(frame, fontRenderer, "Current state", {60, 90}, 30, {230, 240, 255}, 2);
    drawText(frame, fontRenderer, state.name, {60, 150}, 48, {80, 220, 255}, 3);

    drawText(frame, fontRenderer, "Allowed actions", {60, 245}, 30, {230, 240, 255}, 2);
    drawText(frame,
             fontRenderer,
             "Arrow keys: change selected action    Enter: trigger action    i: reset to Init    q or ESC: quit",
             {60, 680},
             24,
             {175, 185, 195},
             2);
}

void drawActionList(cv::Mat& frame,
                    const FontRenderer& fontRenderer,
                    const StateDefinition& state,
                    std::size_t selectedAction) {
    const int x = 60;
    const int top = 285;
    const int rowHeight = 46;
    const int boxWidth = 520;

    for (std::size_t i = 0; i < state.actions.size(); ++i) {
        const bool isSelected = i == selectedAction;
        const int y = top + static_cast<int>(i) * rowHeight;
        const cv::Rect actionRect(x, y, boxWidth, 34);

        cv::Scalar fillColor = isSelected ? cv::Scalar(90, 180, 90) : cv::Scalar(38, 44, 54);
        cv::Scalar textColor = isSelected ? cv::Scalar(18, 24, 18) : cv::Scalar(230, 240, 255);
        cv::Scalar borderColor = isSelected ? cv::Scalar(180, 255, 180) : cv::Scalar(70, 78, 92);

        cv::rectangle(frame, actionRect, fillColor, cv::FILLED);
        cv::rectangle(frame, actionRect, borderColor, 2);
        drawText(frame, fontRenderer, state.actions[i], {x + 16, y + 25}, 26, textColor, 2);
    }
}

int normalizeKey(int key) {
    if (key < 0) {
        return key;
    }

    switch (key) {
        case 2490368:
        case 65362:
            return kKeyUp;
        case 2621440:
        case 65364:
            return kKeyDown;
        case 2424832:
        case 65361:
            return kKeyLeft;
        case 2555904:
        case 65363:
            return kKeyRight;
        default:
            return key & 0xFF;
    }
}

std::size_t findStateIndex(const std::vector<StateDefinition>& states, const std::string& name) {
    for (std::size_t i = 0; i < states.size(); ++i) {
        if (states[i].name == name) {
            return i;
        }
    }
    return 0;
}

std::optional<std::string> nextStateForAction(const std::string& stateName, const std::string& actionName) {
    if (stateName == "Init") {
        if (actionName == "select spice") {
            return "Selecting spice";
        }
        if (actionName == "select utensil") {
            return "Selecting utensil";
        }
    }

    if (stateName == "Selecting spice") {
        return "Spice selected";
    }

    if (stateName == "Spice selected") {
        if (actionName == "shake into pot") {
            return "Shaking spice into pot";
        }
        if (actionName == "put down") {
            return "Init";
        }
    }

    if (stateName == "Shaking spice into pot") {
        if (actionName == "put down") {
            return "Init";
        }
        return "Shaking spice into pot";
    }

    if (stateName == "Selecting utensil") {
        return "Utensil selected";
    }

    if (stateName == "Utensil selected") {
        if (actionName == "use utensil") {
            return "Using utensil";
        }
        if (actionName == "put down") {
            return "Init";
        }
    }

    if (stateName == "Using utensil") {
        if (actionName == "put down") {
            return "Init";
        }
        return "Using utensil";
    }

    return std::nullopt;
}

}  // namespace

int main() {
    std::vector<StateDefinition> states = buildStates();
    if (states.empty()) {
        std::cerr << "No states configured." << std::endl;
        return EXIT_FAILURE;
    }

    const FontRenderer fontRenderer = createFontRenderer();
    std::size_t stateIndex = 0;
    std::size_t actionIndex = 0;

    cv::namedWindow(kWindowName, cv::WINDOW_AUTOSIZE);

    while (true) {
        cv::Mat frame(kWindowHeight, kWindowWidth, CV_8UC3, cv::Scalar(24, 28, 36));
        const StateDefinition& currentState = states[stateIndex];
        if (actionIndex >= currentState.actions.size()) {
            actionIndex = 0;
        }

        drawHeader(frame, fontRenderer, currentState);
        drawActionList(frame, fontRenderer, currentState, actionIndex);
        cv::imshow(kWindowName, frame);

        const int rawKey = cv::waitKeyEx(0);
        const int key = normalizeKey(rawKey);

        if (key == 27 || key == 'q' || key == 'Q') {
            break;
        }

        if (key == 'i' || key == 'I') {
            stateIndex = findStateIndex(states, "Init");
            actionIndex = 0;
            continue;
        }

        if ((key == kKeyUp || key == kKeyLeft || key == 'j' || key == 'J') && !currentState.actions.empty()) {
            actionIndex = (actionIndex + currentState.actions.size() - 1) % currentState.actions.size();
            continue;
        }

        if ((key == kKeyDown || key == kKeyRight || key == 'k' || key == 'K') && !currentState.actions.empty()) {
            actionIndex = (actionIndex + 1) % currentState.actions.size();
            continue;
        }

        if ((key == 10 || key == 13) && !currentState.actions.empty()) {
            const std::optional<std::string> nextState =
                nextStateForAction(currentState.name, currentState.actions[actionIndex]);
            if (nextState.has_value()) {
                stateIndex = findStateIndex(states, nextState.value());
                actionIndex = 0;
            }
            continue;
        }
    }

    cv::destroyAllWindows();
    return EXIT_SUCCESS;
}
