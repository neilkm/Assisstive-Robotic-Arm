#include "JetsonQtApp/QtUiCommon/Theme.h"

#include <QQmlComponent>
#include <QQmlEngine>
#include <QScopedPointer>
#include <QUrl>
#include <QVariant>

namespace {

constexpr char kThemeUrl[] = "qrc:/JetsonQtApp/theme.qml";

QColor readColor(const QObject& object, const char* propertyName, const QColor& fallback) {
    const QVariant value = object.property(propertyName);
    if (value.canConvert<QColor>()) {
        return value.value<QColor>();
    }

    const QColor color(value.toString());
    return color.isValid() ? color : fallback;
}

QString readString(const QObject& object, const char* propertyName, const QString& fallback) {
    const QVariant value = object.property(propertyName);
    return value.isValid() ? value.toString() : fallback;
}

int readInt(const QObject& object, const char* propertyName, int fallback) {
    const QVariant value = object.property(propertyName);
    return value.isValid() ? value.toInt() : fallback;
}

}  // namespace

namespace jetsonqt::qtui {

Theme Theme::loadDefault() {
    QQmlEngine engine;
    QQmlComponent component(&engine, QUrl(QString::fromLatin1(kThemeUrl)));
    QScopedPointer<QObject> themeObject(component.create());

    Theme theme;
    theme.button1_ = {
        QColor(QStringLiteral("#5ab45a")),
        QColor(QStringLiteral("#b4ffb4")),
        QColor(QStringLiteral("#121812")),
        16,
        6,
        26,
    };
    theme.button2_ = {
        QColor(QStringLiteral("#262c36")),
        QColor(QStringLiteral("#464e5c")),
        QColor(QStringLiteral("#e6f0ff")),
        16,
        6,
        26,
    };
    theme.mainFont_ = QStringLiteral("Avenir Next");
    theme.alternateFont_ = QStringLiteral("Helvetica Neue");
    theme.headingFontSize_ = 48;
    theme.descriptionFontSize_ = 30;
    theme.bodyTextFontSize_ = 22;
    theme.mainBgColor_ = QColor(QStringLiteral("#181c24"));
    theme.mainFgColor_ = QColor(QStringLiteral("#50dcff"));
    theme.menuBgColor_ = QColor(QStringLiteral("#20242c"));
    theme.menuFgColor_ = QColor(QStringLiteral("#e6f0ff"));
    theme.mainBorderColor_ = QColor(QStringLiteral("#464e5c"));
    theme.highlightedColor_ = QColor(QStringLiteral("#f2c14e"));
    theme.highlightedBorderColor_ = QColor(QStringLiteral("#ffe08a"));

    if (themeObject.isNull()) {
        return theme;
    }

    theme.button1_ = {
        readColor(*themeObject, "button1_mainColor", theme.button1_.mainColor),
        readColor(*themeObject, "button1_borderColor", theme.button1_.borderColor),
        readColor(*themeObject, "button1_textColor", theme.button1_.textColor),
        readInt(*themeObject, "button1_padding", theme.button1_.padding),
        readInt(*themeObject, "button1_radius", theme.button1_.radius),
        readInt(*themeObject, "button1_fontSize", theme.button1_.fontSize),
    };
    theme.button2_ = {
        readColor(*themeObject, "button2_mainColor", theme.button2_.mainColor),
        readColor(*themeObject, "button2_borderColor", theme.button2_.borderColor),
        readColor(*themeObject, "button2_textColor", theme.button2_.textColor),
        readInt(*themeObject, "button2_padding", theme.button2_.padding),
        readInt(*themeObject, "button2_radius", theme.button2_.radius),
        readInt(*themeObject, "button2_fontSize", theme.button2_.fontSize),
    };
    theme.mainFont_ = readString(*themeObject, "mainFont", theme.mainFont_);
    theme.alternateFont_ = readString(*themeObject, "alternateFont", theme.alternateFont_);
    theme.headingFontSize_ = readInt(*themeObject, "heading_FontSize", theme.headingFontSize_);
    theme.descriptionFontSize_ = readInt(*themeObject, "description_FontSize", theme.descriptionFontSize_);
    theme.bodyTextFontSize_ = readInt(*themeObject, "bodyText_FontSize", theme.bodyTextFontSize_);
    theme.mainBgColor_ = readColor(*themeObject, "mainBgColor", theme.mainBgColor_);
    theme.mainFgColor_ = readColor(*themeObject, "mainFgColor", theme.mainFgColor_);
    theme.menuBgColor_ = readColor(*themeObject, "menuBgColor", theme.menuBgColor_);
    theme.menuFgColor_ = readColor(*themeObject, "menuFGColor", theme.menuFgColor_);
    theme.mainBorderColor_ = readColor(*themeObject, "mainBorderColor", theme.mainBorderColor_);
    theme.highlightedColor_ = readColor(*themeObject, "highlightedColor", theme.highlightedColor_);
    theme.highlightedBorderColor_ = readColor(*themeObject, "highlightedBorderColor", theme.highlightedBorderColor_);

    return theme;
}

const ButtonTheme& Theme::button1() const {
    return button1_;
}

const ButtonTheme& Theme::button2() const {
    return button2_;
}

const QString& Theme::mainFont() const {
    return mainFont_;
}

const QString& Theme::alternateFont() const {
    return alternateFont_;
}

int Theme::headingFontSize() const {
    return headingFontSize_;
}

int Theme::descriptionFontSize() const {
    return descriptionFontSize_;
}

int Theme::bodyTextFontSize() const {
    return bodyTextFontSize_;
}

const QColor& Theme::mainBgColor() const {
    return mainBgColor_;
}

const QColor& Theme::mainFgColor() const {
    return mainFgColor_;
}

const QColor& Theme::menuBgColor() const {
    return menuBgColor_;
}

const QColor& Theme::menuFgColor() const {
    return menuFgColor_;
}

const QColor& Theme::mainBorderColor() const {
    return mainBorderColor_;
}

const QColor& Theme::highlightedColor() const {
    return highlightedColor_;
}

const QColor& Theme::highlightedBorderColor() const {
    return highlightedBorderColor_;
}

}  // namespace jetsonqt::qtui
