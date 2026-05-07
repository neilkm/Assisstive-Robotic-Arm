#pragma once

#include <QColor>
#include <QString>

namespace jetsonqt::qtui {

struct ButtonTheme {
    QColor mainColor;
    QColor borderColor;
    QColor textColor;
    int padding = 0;
    int radius = 0;
    int fontSize = 0;
};

class Theme {
public:
    [[nodiscard]] static Theme loadDefault();

    [[nodiscard]] const ButtonTheme& button1() const;
    [[nodiscard]] const ButtonTheme& button2() const;

    [[nodiscard]] const QString& mainFont() const;
    [[nodiscard]] const QString& alternateFont() const;
    [[nodiscard]] int headingFontSize() const;
    [[nodiscard]] int descriptionFontSize() const;
    [[nodiscard]] int bodyTextFontSize() const;

    [[nodiscard]] const QColor& mainBgColor() const;
    [[nodiscard]] const QColor& mainFgColor() const;
    [[nodiscard]] const QColor& menuBgColor() const;
    [[nodiscard]] const QColor& menuFgColor() const;
    [[nodiscard]] const QColor& mainBorderColor() const;
    [[nodiscard]] const QColor& highlightedColor() const;
    [[nodiscard]] const QColor& highlightedBorderColor() const;

private:
    ButtonTheme button1_;
    ButtonTheme button2_;
    QString mainFont_;
    QString alternateFont_;
    int headingFontSize_ = 0;
    int descriptionFontSize_ = 0;
    int bodyTextFontSize_ = 0;
    QColor mainBgColor_;
    QColor mainFgColor_;
    QColor menuBgColor_;
    QColor menuFgColor_;
    QColor mainBorderColor_;
    QColor highlightedColor_;
    QColor highlightedBorderColor_;
};

}  // namespace jetsonqt::qtui
