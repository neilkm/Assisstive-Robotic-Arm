#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

struct StateDefinition {
    std::string name;
    std::vector<std::string> actions;
};

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr char kWindowName[] = "Jetson Nano State UI";
constexpr int kKeyUp = 1001;
constexpr int kKeyDown = 1002;

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

void drawHeader(cv::Mat& frame, const StateDefinition& state) {
    cv::putText(frame, "Current state", {60, 90}, cv::FONT_HERSHEY_DUPLEX, 1.0, {230, 240, 255}, 2, cv::LINE_AA);
    cv::putText(frame, state.name, {60, 150}, cv::FONT_HERSHEY_DUPLEX, 1.6, {80, 220, 255}, 3, cv::LINE_AA);

    cv::putText(frame, "Allowed actions", {60, 245}, cv::FONT_HERSHEY_DUPLEX, 1.0, {230, 240, 255}, 2, cv::LINE_AA);
    cv::putText(frame, "Up/Down: change state    j/k: change selected action    q or ESC: quit",
                {60, 680}, cv::FONT_HERSHEY_SIMPLEX, 0.8, {175, 185, 195}, 2, cv::LINE_AA);
}

void drawActionList(cv::Mat& frame, const StateDefinition& state, std::size_t selectedAction) {
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
        cv::putText(frame, state.actions[i], {x + 16, y + 24}, cv::FONT_HERSHEY_DUPLEX, 0.85, textColor, 2, cv::LINE_AA);
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
        default:
            return key & 0xFF;
    }
}

}  // namespace

int main() {
    std::vector<StateDefinition> states = buildStates();
    if (states.empty()) {
        std::cerr << "No states configured." << std::endl;
        return EXIT_FAILURE;
    }

    std::size_t stateIndex = 0;
    std::size_t actionIndex = 0;

    cv::namedWindow(kWindowName, cv::WINDOW_AUTOSIZE);

    while (true) {
        cv::Mat frame(kWindowHeight, kWindowWidth, CV_8UC3, cv::Scalar(24, 28, 36));
        const StateDefinition& currentState = states[stateIndex];
        if (actionIndex >= currentState.actions.size()) {
            actionIndex = 0;
        }

        drawHeader(frame, currentState);
        drawActionList(frame, currentState, actionIndex);
        cv::imshow(kWindowName, frame);

        const int rawKey = cv::waitKeyEx(0);
        const int key = normalizeKey(rawKey);

        if (key == 27 || key == 'q' || key == 'Q') {
            break;
        }

        if (key == kKeyUp) {
            stateIndex = (stateIndex + states.size() - 1) % states.size();
            actionIndex = 0;
            continue;
        }

        if (key == kKeyDown) {
            stateIndex = (stateIndex + 1) % states.size();
            actionIndex = 0;
            continue;
        }

        if ((key == 'j' || key == 'J') && !currentState.actions.empty()) {
            actionIndex = (actionIndex + currentState.actions.size() - 1) % currentState.actions.size();
            continue;
        }

        if ((key == 'k' || key == 'K') && !currentState.actions.empty()) {
            actionIndex = (actionIndex + 1) % currentState.actions.size();
            continue;
        }
    }

    cv::destroyAllWindows();
    return EXIT_SUCCESS;
}
