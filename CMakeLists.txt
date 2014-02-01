project(phonemanager)

set(QT_USE_QTSQL YES) 

set(PLUGIN_NAME "phonemanager")
set(PLUGIN_DISPLAY_NAME "Phone Manager")
set(PLUGIN_DEPENDENCIES roster) # used only in CPack

include("phonemanager.cmake")
include("${CMAKE_SOURCE_DIR}/src/plugins/plugins.cmake")
