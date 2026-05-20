#include "AprilTagObject.h"

AprilTagObject::AprilTagObject(int idNumber,
                               QString familyId,
                               double tagSizeInches,
                               QImage image,
                               QUrl imageSource,
                               QObject *parent)
    : QObject(parent),
      m_idNumber(idNumber),
      m_familyId(std::move(familyId)),
      m_tagSizeInches(tagSizeInches),
      m_image(std::move(image)),
      m_imageSource(std::move(imageSource)) {}

int AprilTagObject::idNumber() const {
    return m_idNumber;
}

QString AprilTagObject::familyId() const {
    return m_familyId;
}

double AprilTagObject::tagSizeInches() const {
    return m_tagSizeInches;
}

QImage AprilTagObject::image() const {
    return m_image;
}

QUrl AprilTagObject::imageSource() const {
    return m_imageSource;
}
