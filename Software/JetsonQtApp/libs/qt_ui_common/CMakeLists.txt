jetsonqt_add_library(qt_ui_common
    SOURCES
        src/AssetPaths.cpp
    PUBLIC_HEADERS
        include/JetsonQtApp/QtUiCommon/AssetPaths.hpp
    PUBLIC_LIBS
        Qt${QT_VERSION_MAJOR}::Core
)
