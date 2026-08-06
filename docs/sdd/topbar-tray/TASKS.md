# SDD Tasks — topbar-tray

## Foundation: Value Types & D-Bus Streaming

- [x] T-001: Create TrayItem value struct with service/path/icon fields
  - REQs: REQ-F-007, REQ-F-008, REQ-NF-002, REQ-C-001
  - Check: TrayItem.h defines service, objectPath, iconName, attentionIconName, status, and cached QImage fields; SniIconPixel and SniIconPixmapList types compile with QDBusArgument specialization.

- [x] T-002: Implement QDBusArgument streaming operators for icon types
  - REQs: REQ-F-007, REQ-F-008, REQ-C-001
  - Check: operator>> decodes ARGB32 to RGBA with little-endian byte-swap; QImage(w, h, QImage::Format_RGBA8888) loads decoded pixels; TrayWatcher compiles without D-Bus marshalling errors.

## Model & Host Registration

- [x] T-003: Create TrayModel skeleton as QAbstractListModel singleton
  - REQs: REQ-F-004, REQ-F-005, REQ-NF-002
  - Check: TrayModel.h declares roles (service, objectPath, iconName, attentionIconName, status, iconPixmapUrl, attentionPixmapUrl, title, itemKey); rowCount/data/roleNames compile; model builds as singleton.

- [x] T-004: Create TrayWatcher D-Bus host/watcher registration skeleton
  - REQs: REQ-F-001, REQ-F-003, REQ-NF-001
  - Check: TrayWatcher registers org.kde.StatusNotifierHost-holonight-<pid> and org.kde.StatusNotifierWatcher; both appear in busctl after instantiation.

## CMake & QML Module Integration

- [x] T-005: Wire TrayItem/TrayWatcher/TrayModel sources into CMakeLists.txt
  - REQs: REQ-C-004, REQ-C-005
  - Check: task build succeeds; holonight-shell binary links TrayItem.o, TrayWatcher.o, TrayModel.o.

- [x] T-006: Add TraySection.qml and TrayItem.qml with QT_RESOURCE_ALIAS stripping
  - REQs: REQ-C-004, REQ-C-005
  - Check: task qml-lint passes; QRC paths qrc:/HolonightShell/Tray/TraySection.qml and qrc:/HolonightShell/Tray/TrayItem.qml resolve in QML engine.

- [x] T-007: Create TrayImageProvider and register with QQmlEngine in LayerShellManager
  - REQs: REQ-F-007, REQ-F-008, REQ-NF-002
  - Check: Engine addImageProvider succeeds; TrayImageProvider::requestPixmap is called when QML Image source is image://holonight-tray/...

- [x] T-008: Register TrayModel singleton in main.cpp via qmlRegisterSingletonInstance
  - REQs: REQ-C-005, REQ-C-006
  - Check: qmlRegisterSingletonInstance returns valid handle; QML import HolonightShell can access TrayModel singleton without compile errors.

## Item Lifecycle & Service Tracking

- [x] T-009: Implement item add/remove pipeline (addItem, insertItem, removeItem)
  - REQs: REQ-F-001, REQ-F-004, REQ-F-005
  - Check: addItem calls QAbstractListModel::beginInsertRows/endInsertRows; removeItem calls beginRemoveRows/endRemoveRows; model row count reflects additions/removals.

- [x] T-010: Set up QDBusServiceWatcher for item service disappearance
  - REQs: REQ-F-006, REQ-NF-003
  - Check: TrayWatcher connects serviceUnregistered signal; removeItem is called when watched service vanishes; leaked items do not remain in model.

- [x] T-011: Implement PropertiesChanged subscription per item
  - REQs: REQ-F-019, REQ-NF-001
  - Check: TrayWatcher connects org.freedesktop.DBus.Properties.PropertiesChanged for each item's service/path; updateItemProperties is invoked on signal.

## Icon Rendering Pipeline

- [x] T-012: Implement icon theme lookup (QIcon::fromTheme) with fallback to pixmap
  - REQs: REQ-F-007, REQ-F-008, REQ-C-002, REQ-C-003
  - Check: requestIconPixmap tries theme first; if not found or pixmap is in model, returns QImage from cache; no D-Bus lookups on second call.

- [x] T-013: Decode and cache ARGB32 pixmap with little-endian byte-swap
  - REQs: REQ-F-008, REQ-C-001, REQ-NF-002
  - Check: SniIconPixmapList operator>> converts (width, height, [argb...]) to cached QImage(Format_RGBA8888); little-endian byte order is corrected in decode.

- [x] T-014: Implement updateItemProperties to refresh icon/status on PropertiesChanged
  - REQs: REQ-F-019, REQ-NF-002, REQ-C-006
  - Check: updateItemProperties re-caches pixmap if updated; emits dataChanged with changed role indices; qCInfo logs service+path+updated properties.

## Visual Layer & Status Display

- [x] T-015: Create TraySection.qml BarSection wrapper with Row + Repeater
  - REQs: REQ-F-014, REQ-F-016, REQ-F-017
  - Check: TraySection extends BarSection; Row layout with spacing:6; Repeater delegates to TrayItem for each TrayModel row; opacity Behavior animates fade in 100ms.

- [x] T-016: Create TrayItem.qml delegate with 22×22px Image and status display
  - REQs: REQ-F-009, REQ-F-010, REQ-F-011, REQ-F-012, REQ-F-014, REQ-C-002, REQ-C-003
  - Check: Image 22×22, source bound to model.iconPixmapUrl or model.attentionPixmapUrl (if status===NeedsAttention); visible/hidden/attention states match spec; glow applied via MultiEffect.

- [x] T-017: Implement cyan glow pulse on NeedsAttention status
  - REQs: REQ-F-011, REQ-C-002, REQ-C-003
  - Check: MultiEffect shadowEnabled, shadowOpacity animates 1.0→0.3 with 1.5s cycle when item.status==="NeedsAttention"; cyan color from HoloniightPalette.cyan.

- [x] T-018: Insert TraySection into TopBar.qml between BatterySection and StatusSection
  - REQs: REQ-C-004, REQ-C-005
  - Check: TopBar.qml compiles; qml-lint passes; tray renders horizontally after battery, before status on topbar.

## User Interaction & Activation

- [x] T-019: Implement Activate(0,0) call on TrayItem click
  - REQs: REQ-F-013, REQ-NF-001
  - Check: TrayItem.qml MouseArea.onClicked calls TrayModel.activate(model.itemKey); Q_INVOKABLE activate makes async D-Bus Activate call to item service+path with 5s timeout.

- [x] T-020: Implement Q_INVOKABLE activate(key) in TrayModel
  - REQs: REQ-F-013, REQ-NF-001, REQ-C-006
  - Check: activate method found via reflection; D-Bus call is async (no blocking); activation succeeds without error logs for non-broken items.

## Watcher Conflict Handling

- [x] T-021: Implement conflict fallback to read existing watcher's RegisteredStatusNotifierItems
  - REQs: REQ-F-002, REQ-NF-001
  - Check: If ReplaceExistingService fails, TrayWatcher queries existing watcher's RegisteredStatusNotifierItems D-Bus property; items are added to model from conflict result.

## Verification & Build

- [x] T-022: Run full build and verify zero errors
  - REQs: (all)
  - Check: task build completes without C++ compiler errors; holonight-shell binary is updated.

- [x] T-023: Run qml-lint and verify zero violations
  - REQs: REQ-C-004, REQ-C-005
  - Check: task qml-lint exits with code 0; no unqualified access in Canvas or handlers.

- [x] T-024: Manual smoke test — register test SNI item and verify display
  - REQs: (all)
  - Check: Test SNI item appears in tray section within 500ms; icon displays 22×22; click invokes Activate(0,0); service disappearance removes item cleanly.
