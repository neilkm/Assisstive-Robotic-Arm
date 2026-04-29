#include "JetsonQtApp/QtUiCommon/Resources.h"

#include <QtResource>

void initializeQtUiCommonResources() {
    Q_INIT_RESOURCE(qt_ui_common);
}

namespace jetsonqt::qtui {

void initializeResources() {
    initializeQtUiCommonResources();
}

}  // namespace jetsonqt::qtui
