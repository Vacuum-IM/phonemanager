include(qmake/debug.inc)
include(qmake/config.inc)

#Project configuration
TARGET       = phonemanager
QT           = core gui xml sql
INCLUDEPATH += .
INCLUDEPATH += $$SIPPHONE_SDK_PATH
include(phonemanager.pri) 

#Default progect configuration
include(qmake/plugin.inc)

#Translation
TRANS_SOURCE_ROOT = .
TRANS_BUILD_ROOT  = $${OUT_PWD}
include(translations/languages.inc)
