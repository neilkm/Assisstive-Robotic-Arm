#pragma once

#include "AprilTagObject.h"

#include <QObject>
#include <QImage>
#include <QVariantList>
#include <memory>
#include <vector>

class AprilTagProvider final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString familyId READ familyId NOTIFY tagsChanged)
    Q_PROPERTY(double tagSizeInches READ tagSizeInches NOTIFY tagsChanged)
    Q_PROPERTY(bool checkerboardEnabled READ checkerboardEnabled NOTIFY tagsChanged)
    Q_PROPERTY(int pageCount READ pageCount NOTIFY tagsChanged)
    Q_PROPERTY(QVariantList tags READ tags NOTIFY tagsChanged)

public:
    explicit AprilTagProvider(QObject *parent = nullptr);

    bool configure(const QString &familyId,
                   double tagSizeInches,
                   int pageCount,
                   bool checkerboardEnabled,
                   int startId,
                   QString *errorMessage);

    QString familyId() const;
    double tagSizeInches() const;
    bool checkerboardEnabled() const;
    int pageCount() const;
    QVariantList tags() const;

    Q_INVOKABLE QObject *tagAt(int index) const;
    Q_INVOKABLE QString pageInfoText(int pageIndex) const;
    QImage tagImageAt(int index) const;

signals:
    void tagsChanged();

private:
    QImage markerImage(const QString &familyId, int tagId, QString *errorMessage) const;
    QUrl imageSourceForImage(const QImage &image, QString *errorMessage) const;

    QString m_familyId = "36h11";
    double m_tagSizeInches = 1.0;
    bool m_checkerboardEnabled = true;
    int m_pageCount = 2;
    int m_startId = 0;
    std::vector<std::unique_ptr<AprilTagObject>> m_tagObjects;
};
