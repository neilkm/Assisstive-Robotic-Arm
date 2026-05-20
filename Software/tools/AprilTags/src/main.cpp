#include "AprilTagProvider.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QPageLayout>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QStandardPaths>
#include <QTextStream>
#include <algorithm>
#include <memory>

namespace {
constexpr int kPdfDpi = 72;
constexpr int kTagsPerPage = 4;

bool parseYesNo(const QString &value, bool defaultValue) {
    if (value.isEmpty()) {
        return defaultValue;
    }
    const QString lowered = value.toLower();
    return lowered == "y" || lowered == "yes" || lowered == "true" || lowered == "1";
}

QList<QQuickItem *> collectItems(QQuickItem *root, const QString &objectName) {
    QList<QQuickItem *> matches;
    if (!root) {
        return matches;
    }
    if (root->objectName() == objectName) {
        matches.push_back(root);
    }
    for (QQuickItem *child : root->childItems()) {
        matches.append(collectItems(child, objectName));
    }
    return matches;
}

QColor colorProperty(QObject *object, const char *name, const QColor &fallback) {
    const QVariant value = object->property(name);
    return value.canConvert<QColor>() ? value.value<QColor>() : fallback;
}

QString downloadsOutputPath(const QString &requestedPath) {
    const QString downloadsDir =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation).isEmpty()
            ? QDir::homePath() + "/Downloads"
            : QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);

    if (requestedPath.isEmpty()) {
        return QDir(downloadsDir).absoluteFilePath("apriltags.pdf");
    }

    const QFileInfo requestedInfo(requestedPath);
    if (requestedInfo.isAbsolute()) {
        return requestedInfo.absoluteFilePath();
    }

    return QDir(downloadsDir).absoluteFilePath(requestedPath);
}

void renderItem(QPainter &painter,
                QQuickItem *page,
                QQuickItem *item,
                const AprilTagProvider &tagProvider) {
    if (!item || !item->isVisible()) {
        return;
    }

    const QString objectName = item->objectName();
    const QPointF pagePos = item->mapToItem(page, QPointF(0, 0));
    const QRectF bounds(pagePos, QSizeF(item->width(), item->height()));

    if (objectName == "tag") {
        const int pageIndex = page->property("pageIndex").toInt();
        const int indexOnPage = item->property("indexOnPage").toInt();
        const QImage image = tagProvider.tagImageAt(pageIndex * kTagsPerPage + indexOnPage);
        if (!image.isNull()) {
            painter.drawImage(bounds, image);
        }
        return;
    }

    if (objectName == "printRect") {
        painter.fillRect(bounds, colorProperty(item, "color", Qt::black));
    } else if (objectName == "printText") {
        painter.save();
        painter.setPen(colorProperty(item, "color", Qt::black));
        QFont font = painter.font();
        const QVariant fontValue = item->property("font");
        if (fontValue.canConvert<QFont>()) {
            font = fontValue.value<QFont>();
        }
        painter.setFont(font);
        painter.drawText(bounds, Qt::AlignCenter | Qt::TextWordWrap, item->property("text").toString());
        painter.restore();
    }

    for (QQuickItem *child : item->childItems()) {
        renderItem(painter, page, child, tagProvider);
    }
}

bool renderPdf(QQuickItem *document,
               const AprilTagProvider &tagProvider,
               const QString &outputPath,
               QString *errorMessage) {
    QList<QQuickItem *> pages = collectItems(document, "page");
    QList<QQuickItem *> checkerboardPages = collectItems(document, "checkerboardPage");
    pages.append(checkerboardPages);
    std::sort(pages.begin(), pages.end(), [](QQuickItem *left, QQuickItem *right) {
        return left->property("pageIndex").toInt() < right->property("pageIndex").toInt();
    });

    pages.erase(std::remove_if(pages.begin(), pages.end(), [](QQuickItem *page) {
                    return !page->isVisible();
                }),
                pages.end());

    if (pages.isEmpty()) {
        *errorMessage = "QML layout did not create any visible pages.";
        return false;
    }

    QDir().mkpath(QFileInfo(outputPath).absolutePath());

    QPdfWriter writer(outputPath);
    writer.setResolution(kPdfDpi);
    writer.setPageSize(QPageSize(QPageSize::Letter));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0), QPageLayout::Point);

    QPainter painter(&writer);
    if (!painter.isActive()) {
        *errorMessage = QString("Failed to open PDF for writing: %1").arg(outputPath);
        return false;
    }

    for (int index = 0; index < pages.size(); ++index) {
        if (index > 0) {
            writer.newPage();
        }

        QQuickItem *page = pages[index];
        painter.fillRect(QRectF(0, 0, page->width(), page->height()), Qt::white);
        for (QQuickItem *child : page->childItems()) {
            renderItem(painter, page, child, tagProvider);
        }
    }

    painter.end();
    return true;
}
} // namespace

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("AprilTags");
    QGuiApplication::setOrganizationName("AssistiveRoboticArm");

    QCommandLineParser parser;
    parser.setApplicationDescription("Headless Qt/QML AprilTag PDF generator.");
    parser.addHelpOption();

    const QCommandLineOption familyOption("family", "AprilTag family.", "family", "36h11");
    const QCommandLineOption sizeOption("tag-size-inches", "Printed tag size.", "inches", "1.0");
    const QCommandLineOption pagesOption("pages", "Number of AprilTag pages.", "count", "2");
    const QCommandLineOption checkerboardOption("checkerboard", "Enable checkerboard page.", "yes-no", "yes");
    const QCommandLineOption startIdOption("start-id", "First AprilTag ID.", "id", "0");
    const QCommandLineOption outputOption("output", "Output PDF filename or absolute path.", "path");
    parser.addOptions({familyOption, sizeOption, pagesOption, checkerboardOption, startIdOption, outputOption});
    parser.process(app);

    AprilTagProvider tagProvider;
    QString errorMessage;
    if (!tagProvider.configure(parser.value(familyOption),
                               parser.value(sizeOption).toDouble(),
                               parser.value(pagesOption).toInt(),
                               parseYesNo(parser.value(checkerboardOption), true),
                               parser.value(startIdOption).toInt(),
                               &errorMessage)) {
        QTextStream(stderr) << "ERROR: " << errorMessage << Qt::endl;
        return 1;
    }

    QQmlEngine engine;
    engine.rootContext()->setContextProperty("tagProvider", &tagProvider);

    QQmlComponent component(&engine, QUrl(QStringLiteral("qrc:/AprilTags/TagSheetLayout.qml")));
    std::unique_ptr<QObject> rootObject(component.create());
    if (!rootObject) {
        QTextStream(stderr) << "ERROR: Could not load TagSheetLayout.qml" << Qt::endl;
        for (const QQmlError &error : component.errors()) {
            QTextStream(stderr) << error.toString() << Qt::endl;
        }
        return 1;
    }

    auto *document = qobject_cast<QQuickItem *>(rootObject.get());
    if (!document) {
        QTextStream(stderr) << "ERROR: QML root must be a QtQuick Item." << Qt::endl;
        return 1;
    }

    app.processEvents();

    const QString outputPath = downloadsOutputPath(parser.value(outputOption));
    if (!renderPdf(document, tagProvider, outputPath, &errorMessage)) {
        QTextStream(stderr) << "ERROR: " << errorMessage << Qt::endl;
        return 1;
    }

    QTextStream(stdout) << "Wrote " << outputPath << Qt::endl;
    return 0;
}
