#include "AprilTagProvider.h"

#include <QByteArray>
#include <QBuffer>
#include <QImage>
#include <QUrl>
#include <opencv2/aruco.hpp>
#include <opencv2/core/version.hpp>
#include <opencv2/imgcodecs.hpp>

namespace {
constexpr int kTagsPerPage = 4;
constexpr int kMarkerPixels = 720;

int dictionaryForFamily(const QString &familyId) {
    if (familyId == "16h5") {
        return cv::aruco::DICT_APRILTAG_16h5;
    }
    if (familyId == "25h9") {
        return cv::aruco::DICT_APRILTAG_25h9;
    }
    if (familyId == "36h10") {
        return cv::aruco::DICT_APRILTAG_36h10;
    }
    return cv::aruco::DICT_APRILTAG_36h11;
}

int familySize(const QString &familyId) {
    if (familyId == "16h5") {
        return 30;
    }
    if (familyId == "25h9") {
        return 35;
    }
    if (familyId == "36h10") {
        return 2320;
    }
    return 587;
}
} // namespace

AprilTagProvider::AprilTagProvider(QObject *parent) : QObject(parent) {}

bool AprilTagProvider::configure(const QString &familyId,
                                 double tagSizeInches,
                                 int pageCount,
                                 bool checkerboardEnabled,
                                 int startId,
                                 QString *errorMessage) {
    if (tagSizeInches <= 0.0) {
        *errorMessage = "Tag size must be greater than zero.";
        return false;
    }
    if (pageCount <= 0) {
        *errorMessage = "Page count must be greater than zero.";
        return false;
    }
    if (startId < 0) {
        *errorMessage = "Start ID must be non-negative.";
        return false;
    }

    const int tagCount = pageCount * kTagsPerPage;
    const int maxFamilySize = familySize(familyId);
    if (startId + tagCount > maxFamilySize) {
        *errorMessage = QString("Family %1 supports IDs 0 through %2.")
                            .arg(familyId)
                            .arg(maxFamilySize - 1);
        return false;
    }

    std::vector<std::unique_ptr<AprilTagObject>> newTags;
    newTags.reserve(static_cast<size_t>(tagCount));
    for (int offset = 0; offset < tagCount; ++offset) {
        const int tagId = startId + offset;
        QImage image = markerImage(familyId, tagId, errorMessage);
        if (!errorMessage->isEmpty()) {
            return false;
        }
        QUrl imageSource = imageSourceForImage(image, errorMessage);
        if (!errorMessage->isEmpty()) {
            return false;
        }
        newTags.push_back(std::make_unique<AprilTagObject>(
            tagId, familyId, tagSizeInches, image, imageSource, this));
    }

    m_familyId = familyId;
    m_tagSizeInches = tagSizeInches;
    m_pageCount = pageCount;
    m_checkerboardEnabled = checkerboardEnabled;
    m_startId = startId;
    m_tagObjects = std::move(newTags);
    emit tagsChanged();
    return true;
}

QString AprilTagProvider::familyId() const {
    return m_familyId;
}

double AprilTagProvider::tagSizeInches() const {
    return m_tagSizeInches;
}

bool AprilTagProvider::checkerboardEnabled() const {
    return m_checkerboardEnabled;
}

int AprilTagProvider::pageCount() const {
    return m_pageCount;
}

QVariantList AprilTagProvider::tags() const {
    QVariantList values;
    values.reserve(static_cast<int>(m_tagObjects.size()));
    for (const auto &tag : m_tagObjects) {
        values.push_back(QVariant::fromValue<QObject *>(tag.get()));
    }
    return values;
}

QObject *AprilTagProvider::tagAt(int index) const {
    if (index < 0 || index >= static_cast<int>(m_tagObjects.size())) {
        return nullptr;
    }
    return m_tagObjects[static_cast<size_t>(index)].get();
}

QString AprilTagProvider::pageInfoText(int pageIndex) const {
    const int first = pageIndex * kTagsPerPage;
    if (first + 3 >= static_cast<int>(m_tagObjects.size())) {
        return {};
    }

    return QString("Cut along dotted lines.\n"
                   "TopLeft: %1 ID %2\n"
                   "TopRight: %3 ID %4\n"
                   "BottLeft: %5 ID %6\n"
                   "BottRight: %7 ID %8\n"
                   "Size: %9 in")
        .arg(m_tagObjects[first + 0]->familyId())
        .arg(m_tagObjects[first + 0]->idNumber())
        .arg(m_tagObjects[first + 1]->familyId())
        .arg(m_tagObjects[first + 1]->idNumber())
        .arg(m_tagObjects[first + 2]->familyId())
        .arg(m_tagObjects[first + 2]->idNumber())
        .arg(m_tagObjects[first + 3]->familyId())
        .arg(m_tagObjects[first + 3]->idNumber())
        .arg(m_tagSizeInches, 0, 'f', 3);
}

QImage AprilTagProvider::tagImageAt(int index) const {
    if (index < 0 || index >= static_cast<int>(m_tagObjects.size())) {
        return {};
    }
    return m_tagObjects[static_cast<size_t>(index)]->image();
}

QImage AprilTagProvider::markerImage(const QString &familyId,
                                     int tagId,
                                     QString *errorMessage) const {
    cv::Mat marker;
    const auto dictionary = cv::aruco::getPredefinedDictionary(dictionaryForFamily(familyId));

#if CV_VERSION_MAJOR > 4 || (CV_VERSION_MAJOR == 4 && CV_VERSION_MINOR >= 7)
    cv::aruco::generateImageMarker(dictionary, tagId, kMarkerPixels, marker, 1);
#else
    cv::aruco::drawMarker(dictionary, tagId, kMarkerPixels, marker, 1);
#endif

    QImage image(marker.data, marker.cols, marker.rows, static_cast<int>(marker.step), QImage::Format_Grayscale8);
    return image.copy();
}

QUrl AprilTagProvider::imageSourceForImage(const QImage &image, QString *errorMessage) const {
    QByteArray pngBytes;
    QBuffer buffer(&pngBytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        *errorMessage = "Failed to encode AprilTag PNG.";
        return {};
    }
    return QUrl(QString("data:image/png;base64,%1").arg(QString::fromLatin1(pngBytes.toBase64())));
}
