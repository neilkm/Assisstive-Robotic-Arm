#include <QtGlobal>

#ifdef Q_OS_DARWIN
#include <QtPlugin>
Q_IMPORT_PLUGIN(QDarwinCameraPermissionPlugin)
#endif
