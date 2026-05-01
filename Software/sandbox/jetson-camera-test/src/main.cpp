#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef __linux__
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace {

constexpr int kWindowWidth = 1280;
constexpr int kWindowHeight = 720;
constexpr char kWindowName[] = "Jetson Camera Test";
constexpr int kKeyUp = 1001;
constexpr int kKeyDown = 1002;
constexpr int kKeyLeft = 1003;
constexpr int kKeyRight = 1004;
constexpr int kSelectionVisibleRows = 7;

enum class CameraKind {
    V4L2Device,
    JetsonCsi,
    GenericIndex,
};

struct CameraOption {
    CameraKind kind = CameraKind::V4L2Device;
    std::string id;
    std::string label;
    std::string details;
    std::string openValue;
    int numericId = -1;
};

void drawText(cv::Mat& frame,
              const std::string& text,
              const cv::Point& origin,
              int fontHeight,
              const cv::Scalar& color,
              int thickness) {
    const double scale = static_cast<double>(fontHeight) / 30.0;
    cv::putText(frame, text, origin, cv::FONT_HERSHEY_DUPLEX, scale, color, thickness, cv::LINE_AA);
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

std::string trim(std::string value) {
    auto isSpace = [](unsigned char ch) { return std::isspace(ch) != 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](unsigned char ch) { return !isSpace(ch); }));
    value.erase(std::find_if(value.rbegin(), value.rend(), [&](unsigned char ch) { return !isSpace(ch); }).base(), value.end());
    return value;
}

std::optional<int> parseVideoNodeIndex(const std::filesystem::path& path) {
    const std::string name = path.filename().string();
    constexpr char prefix[] = "video";
    if (name.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }

    const std::string suffix = name.substr(sizeof(prefix) - 1);
    if (suffix.empty() ||
        !std::all_of(suffix.begin(), suffix.end(), [](unsigned char ch) { return std::isdigit(ch) != 0; })) {
        return std::nullopt;
    }

    return std::stoi(suffix);
}

void appendCameraOption(std::vector<CameraOption>& cameras, CameraOption camera) {
    const auto duplicate = std::find_if(cameras.begin(), cameras.end(), [&](const CameraOption& existing) {
        return existing.id == camera.id;
    });
    if (duplicate == cameras.end()) {
        cameras.push_back(std::move(camera));
    }
}

#ifdef __linux__
std::optional<CameraOption> describeVideoNode(const std::filesystem::path& devicePath) {
    const int fd = ::open(devicePath.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) {
        return std::nullopt;
    }

    v4l2_capability capability {};
    if (::ioctl(fd, VIDIOC_QUERYCAP, &capability) != 0) {
        ::close(fd);
        return std::nullopt;
    }
    ::close(fd);

    uint32_t capabilities = capability.capabilities;
    if ((capabilities & V4L2_CAP_DEVICE_CAPS) != 0U) {
        capabilities = capability.device_caps;
    }

    const uint32_t captureFlags = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_VIDEO_CAPTURE_MPLANE;
    if ((capabilities & captureFlags) == 0U) {
        return std::nullopt;
    }

    std::string label = trim(reinterpret_cast<const char*>(capability.card));
    std::string busInfo = trim(reinterpret_cast<const char*>(capability.bus_info));
    std::string driver = trim(reinterpret_cast<const char*>(capability.driver));
    std::string details = devicePath.string();
    if (!busInfo.empty()) {
        details += "  " + busInfo;
    } else if (!driver.empty()) {
        details += "  " + driver;
    }

    CameraOption option;
    option.kind = CameraKind::V4L2Device;
    option.id = devicePath.filename().string();
    option.label = label.empty() ? option.id : label;
    option.details = details;
    option.openValue = devicePath.string();
    option.numericId = parseVideoNodeIndex(devicePath).value_or(-1);
    return option;
}

std::vector<CameraOption> discoverV4L2Cameras() {
    std::vector<std::filesystem::path> videoNodes;
    const std::filesystem::path devPath("/dev");
    if (!std::filesystem::exists(devPath)) {
        return {};
    }

    for (const auto& entry : std::filesystem::directory_iterator(devPath)) {
        if (!entry.is_character_file()) {
            continue;
        }

        const std::string name = entry.path().filename().string();
        if (name.rfind("video", 0) == 0) {
            videoNodes.push_back(entry.path());
        }
    }

    std::sort(videoNodes.begin(), videoNodes.end(), [](const auto& left, const auto& right) {
        const std::optional<int> leftIndex = parseVideoNodeIndex(left);
        const std::optional<int> rightIndex = parseVideoNodeIndex(right);
        if (leftIndex.has_value() && rightIndex.has_value() && leftIndex.value() != rightIndex.value()) {
            return leftIndex.value() < rightIndex.value();
        }
        return left.string() < right.string();
    });

    std::vector<CameraOption> cameras;
    for (const auto& videoNode : videoNodes) {
        const std::optional<CameraOption> camera = describeVideoNode(videoNode);
        if (camera.has_value()) {
            appendCameraOption(cameras, camera.value());
        }
    }
    return cameras;
}

bool hasJetsonArgusPlugin() {
    return std::filesystem::exists("/usr/lib/aarch64-linux-gnu/gstreamer-1.0/libgstnvarguscamerasrc.so") ||
           std::filesystem::exists("/usr/lib/aarch64-linux-gnu/gstreamer-1.0/deepstream/libgstnvarguscamerasrc.so") ||
           std::filesystem::exists("/usr/lib/aarch64-linux-gnu/tegra/libgstnvarguscamerasrc.so");
}

std::string jetsonCsiPipeline(int sensorId) {
    std::ostringstream pipeline;
    pipeline
        << "nvarguscamerasrc sensor-id=" << sensorId
        << " ! video/x-raw(memory:NVMM), width=(int)1280, height=(int)720, framerate=(fraction)30/1"
        << " ! nvvidconv flip-method=0"
        << " ! video/x-raw, width=(int)1280, height=(int)720, format=(string)BGRx"
        << " ! videoconvert"
        << " ! video/x-raw, format=(string)BGR"
        << " ! appsink drop=true sync=false";
    return pipeline.str();
}

bool probeJetsonCsiSensor(int sensorId) {
    cv::VideoCapture capture;
    if (!capture.open(jetsonCsiPipeline(sensorId), cv::CAP_GSTREAMER)) {
        return false;
    }

    cv::Mat frame;
    const bool ok = capture.read(frame) && !frame.empty();
    capture.release();
    return ok;
}

std::vector<CameraOption> discoverJetsonCsiCameras() {
    if (!hasJetsonArgusPlugin()) {
        return {};
    }

    std::vector<CameraOption> cameras;
    for (int sensorId = 0; sensorId <= 1; ++sensorId) {
        if (!probeJetsonCsiSensor(sensorId)) {
            continue;
        }

        CameraOption camera;
        camera.kind = CameraKind::JetsonCsi;
        camera.id = "csi-" + std::to_string(sensorId);
        camera.label = "Jetson CSI Camera " + std::to_string(sensorId);
        camera.details = "nvarguscamerasrc sensor-id=" + std::to_string(sensorId);
        camera.openValue = jetsonCsiPipeline(sensorId);
        camera.numericId = sensorId;
        appendCameraOption(cameras, std::move(camera));
    }

    return cameras;
}
#endif

std::vector<CameraOption> discoverGenericIndexCameras() {
    std::vector<CameraOption> cameras;

#ifndef __linux__
    for (int index = 0; index < 8; ++index) {
        cv::VideoCapture capture(index);
        if (!capture.isOpened()) {
            continue;
        }

        cv::Mat frame;
        if (!capture.read(frame) || frame.empty()) {
            continue;
        }

        CameraOption camera;
        camera.kind = CameraKind::GenericIndex;
        camera.id = "index-" + std::to_string(index);
        camera.label = "Camera " + std::to_string(index);
        camera.details = "OpenCV device index " + std::to_string(index);
        camera.numericId = index;
        appendCameraOption(cameras, std::move(camera));
    }
#endif

    return cameras;
}

std::vector<CameraOption> discoverCameras() {
    std::vector<CameraOption> cameras;

#ifdef __linux__
    for (CameraOption& camera : discoverV4L2Cameras()) {
        appendCameraOption(cameras, std::move(camera));
    }
    for (CameraOption& camera : discoverJetsonCsiCameras()) {
        appendCameraOption(cameras, std::move(camera));
    }
#else
    for (CameraOption& camera : discoverGenericIndexCameras()) {
        appendCameraOption(cameras, std::move(camera));
    }
#endif

    return cameras;
}

bool openCamera(const CameraOption& camera, cv::VideoCapture& capture, std::string& errorMessage) {
    capture.release();

    bool opened = false;
    switch (camera.kind) {
        case CameraKind::V4L2Device:
            opened = capture.open(camera.openValue, cv::CAP_V4L2);
            if (!opened) {
                opened = capture.open(camera.openValue);
            }
            break;
        case CameraKind::JetsonCsi:
            opened = capture.open(camera.openValue, cv::CAP_GSTREAMER);
            break;
        case CameraKind::GenericIndex:
            opened = capture.open(camera.numericId);
            break;
    }

    if (!opened) {
        errorMessage = "Could not open " + camera.label + ".";
        return false;
    }

    capture.set(cv::CAP_PROP_BUFFERSIZE, 1);
    if (camera.kind != CameraKind::JetsonCsi) {
        capture.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
        capture.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    }

    cv::Mat frame;
    for (int attempt = 0; attempt < 30; ++attempt) {
        if (capture.read(frame) && !frame.empty()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    capture.release();
    errorMessage = "Opened " + camera.label + " but did not receive frames.";
    return false;
}

std::string defaultSelectionMessage(std::size_t cameraCount) {
    if (cameraCount == 0) {
        return "No cameras detected. Connect a camera and press R to refresh.";
    }

    std::ostringstream message;
    message << "Detected " << cameraCount << " camera";
    if (cameraCount != 1) {
        message << "s";
    }
    message << ". Use arrow keys and Enter to open one.";
    return message.str();
}

void drawSelectionScreen(cv::Mat& canvas,
                         const std::vector<CameraOption>& cameras,
                         std::size_t selectedIndex,
                         const std::string& statusMessage) {
    drawText(canvas, "Jetson Camera Test", {60, 80}, 44, {80, 220, 255}, 3);
    drawText(canvas, "Detected camera devices", {60, 140}, 28, {230, 240, 255}, 2);
    drawText(canvas,
             "Arrow keys: select    Enter: open stream    R: refresh devices    Q / Esc: quit",
             {60, 685},
             24,
             {175, 185, 195},
             2);

    const cv::Rect statusRect(60, 170, 1160, 52);
    cv::rectangle(canvas, statusRect, cv::Scalar(34, 44, 56), cv::FILLED);
    cv::rectangle(canvas, statusRect, cv::Scalar(74, 86, 104), 2);
    drawText(canvas, statusMessage, {80, 205}, 22, {220, 232, 245}, 2);

    const cv::Rect listRect(60, 250, 1160, 380);
    cv::rectangle(canvas, listRect, cv::Scalar(26, 30, 38), cv::FILLED);
    cv::rectangle(canvas, listRect, cv::Scalar(62, 72, 88), 2);

    if (cameras.empty()) {
        drawText(canvas, "No camera devices are available yet.", {90, 365}, 32, {228, 236, 244}, 2);
        drawText(canvas, "For Jetson Nano, connect a USB camera or a CSI camera, then press R.", {90, 420}, 24, {180, 190, 202}, 2);
        return;
    }

    const std::size_t rowCount = std::min<std::size_t>(kSelectionVisibleRows, cameras.size());
    std::size_t startIndex = 0;
    if (selectedIndex >= rowCount / 2) {
        startIndex = selectedIndex - rowCount / 2;
    }
    if (startIndex + rowCount > cameras.size()) {
        startIndex = cameras.size() - rowCount;
    }

    const int rowHeight = 52;
    const int rowGap = 14;
    const int baseY = 275;
    for (std::size_t visibleRow = 0; visibleRow < rowCount; ++visibleRow) {
        const std::size_t cameraIndex = startIndex + visibleRow;
        const CameraOption& camera = cameras[cameraIndex];
        const bool selected = cameraIndex == selectedIndex;
        const int y = baseY + static_cast<int>(visibleRow) * (rowHeight + rowGap);
        const cv::Rect rowRect(90, y, 1100, rowHeight);

        const cv::Scalar fill = selected ? cv::Scalar(90, 180, 90) : cv::Scalar(40, 46, 56);
        const cv::Scalar border = selected ? cv::Scalar(185, 255, 185) : cv::Scalar(76, 86, 104);
        const cv::Scalar title = selected ? cv::Scalar(18, 28, 18) : cv::Scalar(232, 240, 255);
        const cv::Scalar details = selected ? cv::Scalar(30, 46, 30) : cv::Scalar(180, 190, 202);

        cv::rectangle(canvas, rowRect, fill, cv::FILLED);
        cv::rectangle(canvas, rowRect, border, 2);
        drawText(canvas, camera.label, {115, y + 22}, 24, title, 2);
        drawText(canvas, camera.details, {115, y + 45}, 18, details, 1);
    }
}

void drawFrameIntoPanel(const cv::Mat& frame, cv::Mat& canvas, const cv::Rect& panel) {
    cv::rectangle(canvas, panel, cv::Scalar(18, 22, 28), cv::FILLED);
    cv::rectangle(canvas, panel, cv::Scalar(62, 72, 88), 2);

    if (frame.empty()) {
        drawText(canvas, "Waiting for video frames...", {panel.x + 40, panel.y + panel.height / 2}, 28, {220, 228, 238}, 2);
        return;
    }

    const double scaleX = static_cast<double>(panel.width) / static_cast<double>(frame.cols);
    const double scaleY = static_cast<double>(panel.height) / static_cast<double>(frame.rows);
    const double scale = std::min(scaleX, scaleY);

    cv::Mat resizedFrame;
    cv::resize(frame,
               resizedFrame,
               cv::Size(),
               scale,
               scale,
               scale < 1.0 ? cv::INTER_AREA : cv::INTER_LINEAR);

    const int x = panel.x + (panel.width - resizedFrame.cols) / 2;
    const int y = panel.y + (panel.height - resizedFrame.rows) / 2;
    resizedFrame.copyTo(canvas(cv::Rect(x, y, resizedFrame.cols, resizedFrame.rows)));
}

void drawStreamScreen(cv::Mat& canvas,
                      const CameraOption& camera,
                      const cv::Mat& frame,
                      double fps) {
    drawText(canvas, "Live Camera View", {60, 80}, 42, {80, 220, 255}, 3);
    drawText(canvas, camera.label, {60, 135}, 28, {230, 240, 255}, 2);
    drawText(canvas, camera.details, {60, 170}, 20, {175, 185, 195}, 2);

    std::ostringstream metrics;
    metrics << "Resolution: " << frame.cols << " x " << frame.rows;
    if (fps > 0.0) {
        metrics << "    FPS: " << std::fixed << std::setprecision(1) << fps;
    }
    drawText(canvas, metrics.str(), {60, 210}, 20, {200, 212, 224}, 2);

    drawFrameIntoPanel(frame, canvas, cv::Rect(60, 240, 1160, 390));
    drawText(canvas,
             "B: back to selection    Q / Esc: quit",
             {60, 685},
             24,
             {175, 185, 195},
             2);
}

}  // namespace

int main() {
    cv::namedWindow(kWindowName, cv::WINDOW_AUTOSIZE);

    std::vector<CameraOption> cameras = discoverCameras();
    std::size_t selectedIndex = 0;
    std::string statusMessage = defaultSelectionMessage(cameras.size());

    cv::VideoCapture activeCapture;
    CameraOption activeCamera;
    bool streamOpen = false;
    double fpsEstimate = 0.0;
    int framesSinceTick = 0;
    const double tickFrequency = cv::getTickFrequency();
    int64 frameTickStart = cv::getTickCount();

    while (true) {
        if (!streamOpen) {
            if (!cameras.empty() && selectedIndex >= cameras.size()) {
                selectedIndex = cameras.size() - 1;
            }

            cv::Mat canvas(kWindowHeight, kWindowWidth, CV_8UC3, cv::Scalar(24, 28, 36));
            drawSelectionScreen(canvas, cameras, selectedIndex, statusMessage);
            cv::imshow(kWindowName, canvas);

            const int key = normalizeKey(cv::waitKeyEx(0));
            if (key == 27 || key == 'q' || key == 'Q') {
                break;
            }

            if ((key == kKeyUp || key == kKeyLeft) && !cameras.empty()) {
                selectedIndex = (selectedIndex + cameras.size() - 1) % cameras.size();
                continue;
            }

            if ((key == kKeyDown || key == kKeyRight) && !cameras.empty()) {
                selectedIndex = (selectedIndex + 1) % cameras.size();
                continue;
            }

            if (key == 'r' || key == 'R') {
                cameras = discoverCameras();
                if (selectedIndex >= cameras.size()) {
                    selectedIndex = 0;
                }
                statusMessage = defaultSelectionMessage(cameras.size());
                continue;
            }

            if ((key == 10 || key == 13) && !cameras.empty()) {
                std::string errorMessage;
                if (!openCamera(cameras[selectedIndex], activeCapture, errorMessage)) {
                    statusMessage = errorMessage;
                    continue;
                }

                activeCamera = cameras[selectedIndex];
                streamOpen = true;
                fpsEstimate = 0.0;
                framesSinceTick = 0;
                frameTickStart = cv::getTickCount();
            }

            continue;
        }

        cv::Mat frame;
        if (!activeCapture.read(frame) || frame.empty()) {
            activeCapture.release();
            streamOpen = false;
            statusMessage = "Lost the video stream from " + activeCamera.label + ".";
            continue;
        }

        ++framesSinceTick;
        const int64 now = cv::getTickCount();
        const double elapsed = static_cast<double>(now - frameTickStart) / tickFrequency;
        if (elapsed >= 1.0) {
            fpsEstimate = static_cast<double>(framesSinceTick) / elapsed;
            framesSinceTick = 0;
            frameTickStart = now;
        }

        cv::Mat canvas(kWindowHeight, kWindowWidth, CV_8UC3, cv::Scalar(24, 28, 36));
        drawStreamScreen(canvas, activeCamera, frame, fpsEstimate);
        cv::imshow(kWindowName, canvas);

        const int key = normalizeKey(cv::waitKeyEx(1));
        if (key == 27 || key == 'q' || key == 'Q') {
            break;
        }
        if (key == 'b' || key == 'B') {
            activeCapture.release();
            streamOpen = false;
            statusMessage = "Closed " + activeCamera.label + ".";
        }
    }

    activeCapture.release();
    cv::destroyAllWindows();
    return EXIT_SUCCESS;
}
