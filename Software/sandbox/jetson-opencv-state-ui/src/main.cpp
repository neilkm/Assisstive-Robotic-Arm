#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <unordered_map>
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
constexpr int kKeyLeft = 1003;
constexpr int kKeyRight = 1004;

std::optional<std::string> imageFilenameForState(const std::string& stateName) {
    if (stateName == "Init") {
        return "init.png";
    }
    if (stateName == "Selecting spice") {
        return "selectspice.png";
    }
    if (stateName == "Spice selected") {
        return "spiceconfirm.png";
    }
    if (stateName == "Shaking spice into pot") {
        return "usingspice.png";
    }
    return std::nullopt;
}

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

void drawText(cv::Mat& frame,
              const std::string& text,
              cv::Point origin,
              int fontHeight,
              const cv::Scalar& color,
              int thickness) {
    const double scale = static_cast<double>(fontHeight) / 30.0;
    cv::putText(frame, text, origin, cv::FONT_HERSHEY_DUPLEX, scale, color, thickness, cv::LINE_AA);
}

void drawHeader(cv::Mat& frame, const StateDefinition& state) {
    drawText(frame, "Current state", {60, 90}, 30, {230, 240, 255}, 2);
    drawText(frame, state.name, {60, 150}, 48, {80, 220, 255}, 3);

    drawText(frame, "Allowed actions", {60, 245}, 30, {230, 240, 255}, 2);
    drawText(frame,
             "Arrow keys: change selected action    Enter: trigger action    i: reset to Init    q or ESC: quit",
             {60, 680},
             24,
             {175, 185, 195},
             2);
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
        drawText(frame, state.actions[i], {x + 16, y + 25}, 26, textColor, 2);
    }
}

void drawStateImage(cv::Mat& frame,
                    const std::string& stateName,
                    const std::unordered_map<std::string, cv::Mat>& stateImages) {
    const cv::Rect panelRect(650, 70, 580, 590);
    const cv::Scalar panelFill(32, 36, 44);
    const cv::Scalar panelBorder(70, 78, 92);
    cv::rectangle(frame, panelRect, panelFill, cv::FILLED);
    cv::rectangle(frame, panelRect, panelBorder, 2);

    drawText(frame, "State image", {670, 110}, 30, {230, 240, 255}, 2);

    const auto imageIt = stateImages.find(stateName);
    if (imageIt == stateImages.end() || imageIt->second.empty()) {
        drawText(frame, "No image yet for this state", {700, 340}, 28, {190, 198, 208}, 2);
        return;
    }

    const cv::Mat& sourceImage = imageIt->second;
    const cv::Rect imageArea(670, 130, 540, 500);
    const double scaleX = static_cast<double>(imageArea.width) / static_cast<double>(sourceImage.cols);
    const double scaleY = static_cast<double>(imageArea.height) / static_cast<double>(sourceImage.rows);
    const double scale = std::min(scaleX, scaleY);

    cv::Mat resizedImage;
    cv::resize(sourceImage,
               resizedImage,
               cv::Size(),
               scale,
               scale,
               scale < 1.0 ? cv::INTER_AREA : cv::INTER_LINEAR);

    const int x = imageArea.x + (imageArea.width - resizedImage.cols) / 2;
    const int y = imageArea.y + (imageArea.height - resizedImage.rows) / 2;
    resizedImage.copyTo(frame(cv::Rect(x, y, resizedImage.cols, resizedImage.rows)));
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

int main(int argc, char** argv) {
    std::vector<StateDefinition> states = buildStates();
    if (states.empty()) {
        std::cerr << "No states configured." << std::endl;
        return EXIT_FAILURE;
    }

    std::filesystem::path projectDir = std::filesystem::current_path();
    if (argc > 0) {
        try {
            const std::filesystem::path binaryPath = std::filesystem::canonical(argv[0]);
            projectDir = binaryPath.parent_path().parent_path();
        } catch (const std::filesystem::filesystem_error&) {
        }
    }

    const std::filesystem::path imageDir = projectDir / "robotarm state images";
    std::unordered_map<std::string, cv::Mat> stateImages;
    for (const StateDefinition& state : states) {
        const std::optional<std::string> imageFilename = imageFilenameForState(state.name);
        if (!imageFilename.has_value()) {
            continue;
        }

        const std::filesystem::path imagePath = imageDir / imageFilename.value();
        cv::Mat image = cv::imread(imagePath.string(), cv::IMREAD_COLOR);
        if (!image.empty()) {
            stateImages.emplace(state.name, image);
        }
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
        drawStateImage(frame, currentState.name, stateImages);
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
