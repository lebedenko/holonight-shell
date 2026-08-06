add_library(holonight_surfaces STATIC
    src/ShellConstants.h
    src/PerMonitorLayerManager.h
    src/PerMonitorLayerManager.cpp
    src/BackgroundManager.h
    src/BackgroundManager.cpp
    src/WidgetCountdown.h
    src/WidgetCountdown.cpp
    src/WidgetClock.h
    src/WidgetClock.cpp
    src/WidgetManager.h
    src/WidgetManager.cpp
    src/MprisWidgetManager.h
    src/MprisWidgetManager.cpp
    src/WidgetSurfacePolicy.h
    src/WidgetSurfacePolicy.cpp
    src/LayerShellManager.h
    src/LayerShellManager.cpp
    src/LauncherSurfaceLifecycle.h
    src/LauncherSurfaceLifecycle.cpp
    src/LauncherSurface.h
    src/LauncherSurface.cpp
    src/StatusPopupGeometry.h
    src/StatusPopupGeometry.cpp
    src/StatusPopupSurface.h
    src/StatusPopupSurface.cpp
    src/SidebarManager.h
    src/SidebarManager.cpp
    src/SidebarSurfacePolicy.h
    src/SidebarSurfacePolicy.cpp
    src/TooltipGeometry.h
    src/TooltipGeometry.cpp
    src/TooltipSurface.h
    src/TooltipSurface.cpp
    src/NotificationToastSurface.h
    src/NotificationToastSurface.cpp
    src/OsdSurface.h
    src/OsdSurface.cpp
    src/NotificationManager.h
    src/NotificationManager.cpp
    src/TrayItem.h
    src/TrayItem.cpp
    src/TrayItemProperties.h
    src/TrayItemProperties.cpp
    src/QmlSourceLoader.h
    src/TrayModel.h
    src/TrayModel.cpp
    src/TrayWatcher.h
    src/TrayWatcher.cpp
    src/DbusMenuItem.h
    src/DbusMenuItem.cpp
    src/DbusMenuClient.h
    src/DbusMenuClient.cpp
    src/TrayMenuSurface.h
    src/TrayMenuSurface.cpp
    src/IconImageProvider.h
)

target_link_libraries(holonight_surfaces PUBLIC
    holonight_core
    holonight_platform
    holonight_services
    Qt6::Core
    Qt6::Qml
    Qt6::Gui
    Qt6::Quick
    Qt6::DBus
)

target_link_libraries(holonight_surfaces PRIVATE
    holonight_qt_gui_private
)

target_include_directories(holonight_surfaces PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(holonight_surfaces PUBLIC cxx_std_23)
