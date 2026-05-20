#pragma once

#include <QObject>
#include <QImage>
#include <QUrl>

class AprilTagObject final : public QObject {
    Q_OBJECT
    Q_PROPERTY(int idNumber READ idNumber CONSTANT)
    Q_PROPERTY(QString familyId READ familyId CONSTANT)
    Q_PROPERTY(double tagSizeInches READ tagSizeInches CONSTANT)
    Q_PROPERTY(QUrl imageSource READ imageSource CONSTANT)

public:
    AprilTagObject(int idNumber,
                   QString familyId,
                   double tagSizeInches,
                   QImage image,
                   QUrl imageSource,
                   QObject *parent = nullptr);

    int idNumber() const;
    QString familyId() const;
    double tagSizeInches() const;
    QImage image() const;
    QUrl imageSource() const;

private:
    int m_idNumber;
    QString m_familyId;
    double m_tagSizeInches;
    QImage m_image;
    QUrl m_imageSource;
};
