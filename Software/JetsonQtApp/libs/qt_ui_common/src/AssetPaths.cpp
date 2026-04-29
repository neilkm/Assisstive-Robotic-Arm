#include "JetsonQtApp/QtUiCommon/AssetPaths.hpp"

#include <QDir>
#include <QFileInfo>

namespace jetsonqt::qtui {

QString findExistingAssetDirectory(const QString& relativeAssetPath) {
    const QString applicationDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(applicationDir).filePath(relativeAssetPath),
        QDir(applicationDir).filePath(QStringLiteral("../") + relativeAssetPath),
        QDir(applicationDir).filePath(QStringLiteral("../../") + relativeAssetPath),
        QDir(QDir::currentPath()).filePath(relativeAssetPath),
    };

    for (const QString& candidate : candidates) {
        const QFileInfo info(candidate);
        if (info.exists() && info.isDir()) {
            return info.absoluteFilePath();
        }
    }

    return QDir(applicationDir).filePath(relativeAssetPath);
}

}  // namespace jetsonqt::qtui
