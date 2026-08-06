# POC Remediation Phase 0 — Architecture & Implementation Design

| Field | Value |
|---|---|
| Document ID | poc-remediation-phase0/DESIGN.md |
| Cycle | Remediation Phase 0 of the POC Readiness Review |
| Input | `docs/sdd/poc-remediation-phase0/SPEC.md` (23 EARS requirements, 6 items), `docs/sdd/poc-readiness-review/REPORT.md` §4 (U-02, U-06, U-07, U-11), live repo at commit `8dda3a3` |
| Status | Ready for implementation |

---

## How to read this document

Each of the 6 items has: current code path (with verified `file:line` citations), proposed change, interfaces, key decisions, alternatives considered, known risks, and a test plan. A cross-cutting **Implementation Order** and **Files Touched Summary** follow at the end.

**Important — two items diverge from SPEC.md's assumed root cause after direct code verification (all file paths and line numbers below were read directly from the working tree, not inferred from the review report):**

- **Item 4 (HolonightTheme)**: the class is **not missing** — it already exists, compiled, `QML_SINGLETON`-registered, and present in the currently-installed `Holonight` QML module (`/usr/lib/qt6/qml/Holonight/holonight_qml.qmltypes` contains `HolonightTheme` today). The fix in SPEC.md (add `qmlRegisterSingletonType<HolonightTheme>(...)` in Settings' C++) is **not applicable** — there is no header available to `apps/settings` for that class, and no CMake target in this repo links the plugin that owns it (resolution happens purely through QML's runtime import mechanism, identically to the sibling `HoloniightPalette` singleton that already works from the same plugin). See Item 4 below for the corrected design.
- **Item 1 (tray)**: SPEC.md's root location `libs/holonight-surfaces/src/tray/` does not exist as a directory — the tray subsystem is a flat set of files directly under `libs/holonight-surfaces/src/` (`TrayItem.cpp/h`, `TrayItemProperties.cpp/h`, `TrayModel.cpp/h`, `TrayWatcher.cpp/h`, `TrayMenuSurface.cpp/h`). Not a functional discrepancy, just a path correction.

Additionally, this repo's actual test-file naming convention is `tests/test_*.cpp` (see `tests/CMakeLists.txt`), not the `tests/unit_*.cpp` prefix SPEC.md's acceptance criteria use as examples. This design follows the repo's real convention (`test_*.cpp`) throughout; SPEC.md's REQ IDs are cited for traceability regardless of filename.

---

## Item 1: Tray-Icon Pixmap Integer Overflow

### Current code path

`libs/holonight-surfaces/src/TrayItem.h` defines the wire struct:

```cpp
struct SniIconPixel {
  int width{0};
  int height{0};
  QByteArray data;
};
using SniIconPixmapList = QList<SniIconPixel>;
QImage decodePixmapList(const SniIconPixmapList& pixmaps, int target_size = 22);
```

`width`/`height` are `int` because the StatusNotifierItem D-Bus wire type for `IconPixmap` is `a(iiay)` — signed 32-bit ints. This means the realistic overflow input is `INT32_MAX` (2147483647), not `UINT32_MAX` — SPEC.md's own REQ-F-1.2 example message ("width 2147483647 exceeds maximum 512") uses the correct int32 value; the acceptance-criterion prose elsewhere ("Width overflow (e.g. `UINT32_MAX`)") is inconsistent with the wire type and should be read as `INT32_MAX` in the GTest.

`libs/holonight-surfaces/src/TrayItem.cpp:44-100`, `decodePixmapList()`:

```cpp
QImage decodePixmapList(const SniIconPixmapList& pixmaps, int target_size) {
  if (pixmaps.isEmpty()) { ... return {}; }

  // Pick the pixmap whose size is closest to target_size (prefer larger).
  const SniIconPixel* best = pixmaps.constData();
  ...
  if (best->width == 0 || best->height == 0) {
    qCWarning(lcTrayItem) << "IconPixmap decode failed: selected pixmap has invalid dimensions" ...;
    return {};
  }

  int pixel_count = best->width * best->height;                      // line 73 — int*int overflow
  if (best->data.size() < pixel_count * 4) {                          // line 74 — int overflow again
    qCWarning(lcTrayItem) << "IconPixmap decode failed: selected pixmap data too short" ...;
    return {};
  }

  QImage img(best->width, best->height, QImage::Format_ARGB32);       // line 82 — allocation
  const QByteArray& raw = best->data;
  for (int i = 0; i < pixel_count; ++i) { ... img.setPixel(...); }
  ...
}
```

The broken guard is `best->data.size() < pixel_count * 4` (line 74): `pixel_count` is `int` (line 73, `best->width * best->height`), and `pixel_count * 4` is also computed in 32-bit `int`. For `width = height = 46341`, `width * height ≈ 2.147×10⁹` already overflows a signed 32-bit int (UB, typically wraps negative); for `width = 2147483647, height = 1`, `pixel_count * 4` overflows outright. A negative/wrapped `pixel_count` can make the `data.size() < pixel_count * 4` comparison spuriously **pass** (reject-check bypassed), after which `QImage img(best->width, best->height, ...)` (line 82) attempts to allocate a multi-gigabyte (or nonsensical) image — this is the crash. There is **no upper bound on `width`/`height` at all** today; only a zero-check (line 67) and the broken length check (line 74).

**Call sites and service-name context** (`libs/holonight-surfaces/src/TrayItemProperties.cpp`):
```
:23   return decodePixmapList(raw.value<SniIconPixmapList>());        // IconPixmap
:28   return decodePixmapList(pixmaps);                                // AttentionIconPixmap variant
```
Both call sites execute inside `TrayItemProperties::mergeProperties(const TrayItem* existing, const QString& service, const QString& path, const QVariantMap& changed)` (signature at `TrayItemProperties.cpp:76`), which already has `service` (the StatusNotifierItem's D-Bus bus name) in scope and uses it pervasively for logging (e.g. `TrayItemProperties.cpp:87,91,95,99,103,107,125,128,149,152,161,169,173,178`). `decodePixmapList()` itself has no access to `service` today — it's a free function called from inside a lambda/local helper in `mergeProperties`.

**Fallback rendering already works correctly on rejection.** `TrayModel.cpp:88-93` (`data()`, role `IconPixmapUrlRole`/`AttentionIconPixmapUrlRole`) already branches on `item.icon_pixmap_cache.isNull()` to prefer an icon-name/theme-based render over the custom pixmap URL. Since `decodePixmapList()` already returns a default-constructed (`isNull() == true`) `QImage` on every current rejection path, **REQ-F-1.3's "tray entry remains visible with a fallback glyph" is already satisfied by existing QML/model logic** — the fix does not need to add new fallback rendering, only correct the validation that decides when to reject.

### Proposed change

1. Extract the validation into a small, pure, directly-testable free function in `TrayItem.h`/`TrayItem.cpp`:

```cpp
// Named limit: StatusNotifierItem pixmaps are small UI glyphs; every implementation observed in
// the wild (KDE, GNOME, XEmbed trays) ships icons well under 256x256. 512x512 covers all
// practical tray use cases with generous headroom, while still bounding worst-case allocation to
// 512*512*4 = 1 MiB per candidate pixmap.
constexpr int kMaxTrayPixmapDim{512};

enum class PixmapRejectReason : std::uint8_t {
  None,
  NonPositiveDimensions,
  DimensionTooLarge,
  DataLengthMismatch,
};

// Pure validation: arithmetic and comparisons only, no allocation, no I/O (REQ-NF-1.1).
// All arithmetic promotes to qint64 before multiplying, so no 32-bit overflow is possible even
// for adversarial int32 inputs up to INT32_MAX.
[[nodiscard]] PixmapRejectReason validateTrayPixmapDimensions(int width, int height, qsizetype data_size);
```

`validateTrayPixmapDimensions()` implementation (in `TrayItem.cpp`, next to `decodePixmapList`):

```cpp
PixmapRejectReason validateTrayPixmapDimensions(int width, int height, qsizetype data_size) {
  if (width <= 0 || height <= 0) {
    return PixmapRejectReason::NonPositiveDimensions;
  }
  if (width > kMaxTrayPixmapDim || height > kMaxTrayPixmapDim) {
    return PixmapRejectReason::DimensionTooLarge;
  }
  const qint64 expected = static_cast<qint64>(width) * static_cast<qint64>(height) * 4;
  if (static_cast<qint64>(data_size) != expected) {
    return PixmapRejectReason::DataLengthMismatch;
  }
  return PixmapRejectReason::None;
}
```

  - `width > kMaxTrayPixmapDim` check runs before the multiplication that computes `expected`, so no 32-bit path is ever exercised with attacker-controlled magnitudes — even the widening to `qint64` is defense in depth, not the sole guard.
  - Per REQ-F-1.1 point 3 ("Data length must match ... exactly"), the check changes from the current `<` (accepts oversized/padded data) to `!=` (exact match only). This is a deliberate behavior tightening under REQ-C-2 (no back-compat shim needed); flagged as a **known risk** below since it's stricter than today.

2. `decodePixmapList()` gains a `service` parameter (for logging) and calls the extracted validator before any allocation:

```cpp
QImage decodePixmapList(const SniIconPixmapList& pixmaps, int target_size, const QString& service) {
  ...
  const PixmapRejectReason reason =
      validateTrayPixmapDimensions(best->width, best->height, best->data.size());
  if (reason != PixmapRejectReason::None) {
    qCWarning(lcTrayItem) << "Rejecting tray icon from" << service << "- width" << best->width << "height"
                          << best->height << "dataLen" << best->data.size() << "reason"
                          << static_cast<int>(reason);
    return {};
  }
  int pixel_count = best->width * best->height;   // now provably safe: width,height <= 512
  QImage img(best->width, best->height, QImage::Format_ARGB32);
  ...
}
```

   The old `if (best->data.size() < pixel_count * 4)` block (lines 73-78) is deleted — fully superseded by the new validator call.

3. Update the two call sites in `TrayItemProperties.cpp:23,28` to pass `service` (already in scope in the enclosing `mergeProperties`).

4. Update the header default parameter: `QImage decodePixmapList(const SniIconPixmapList& pixmaps, int target_size = 22, const QString& service = {});` — default empty string keeps existing unit tests (`test_tray_item.cpp`) that call it without a service name compiling unchanged.

### Interfaces / APIs

```cpp
// TrayItem.h
constexpr int kMaxTrayPixmapDim{512};
enum class PixmapRejectReason : std::uint8_t { None, NonPositiveDimensions, DimensionTooLarge, DataLengthMismatch };
[[nodiscard]] PixmapRejectReason validateTrayPixmapDimensions(int width, int height, qsizetype data_size);
QImage decodePixmapList(const SniIconPixmapList& pixmaps, int target_size = 22, const QString& service = {});
```

### Key decisions with rationale

- **Extract a pure validator function rather than inlining checks in `decodePixmapList()`**: REQ-NF-1.1 and REQ-C-3 both require the validation logic to be independently GTest-able without constructing a `QImage` or touching D-Bus. A free function taking three primitives (`int, int, qsizetype`) is the simplest testable seam; it also lets the acceptance-criterion GTest assert on the specific `PixmapRejectReason` rather than only a boolean.
- **Enum return over `bool`**: SPEC.md's own acceptance criterion says "bool false, or a result enum" — an enum lets REQ-F-1.2's log message cite the specific rejection reason without re-deriving it from raw values at the log call site.
- **Exact-match (`!=`) instead of `>=`/`<=` for data length**: matches REQ-F-1.1 point 3's literal wording ("must match ... exactly"). Alternative considered: keep `>=` semantics (tolerate padded/oversized buffers some senders might emit) — rejected because REQ-C-2 explicitly permits stricter behavior for a security fix, and a well-behaved StatusNotifierItem sender has no reason to over-pad pixel data.
- **`qint64` widening only at the multiplication, with the `> 512` bound checked first**: the 512 cap alone already makes `width * height * 4` fit safely inside `int` (512×512×4 = 1,048,576), so the `qint64` cast is a second, independent layer of overflow-proofing rather than the only one — belt-and-suspenders per REQ-F-1.1's explicit ask for "64-bit arithmetic."

### Alternatives considered

- **Reject inside `TrayItemProperties::mergeProperties()` instead of inside `decodePixmapList()`**: would let the caller log with `service` without changing `decodePixmapList()`'s signature. Rejected because `decodePixmapList()` is also called from `test_tray_item.cpp` directly and from the `AttentionIconPixmap` path — duplicating the check at every call site reintroduces exactly the divergence risk (§5.2 of the review report) this remediation cycle is trying to avoid. A single validation point inside `decodePixmapList()` is safer.
- **Clamp/scale down oversized pixmaps instead of rejecting them**: REQ-F-1.1 point 4 explicitly requires rejection with fallback, not silent resizing (which would still require allocating at the original, attacker-controlled size before scaling down — defeating the point of the guard).

### Known risks

- Tightening the data-length check from `<` to `!=` could reject a legitimate (if unusual) sender that pads its pixel buffer. No such sender is known in this codebase's test fixtures; `test_tray_item.cpp`'s existing fixtures should be checked for any padded-buffer test case that would need updating.
- `decodePixmapList()`'s signature change (`target_size` now optionally followed by `service`) is source-compatible for existing 2-argument callers (default `service = {}`), so no caller outside `TrayItemProperties.cpp` needs updating — verified only `TrayItemProperties.cpp:23,28` and `test_tray_item.cpp` call it.

### Test plan

- New file: `tests/test_tray_pixmap_validation.cpp` (added to the existing `test_holonight_surfaces` target in `tests/CMakeLists.txt:119-134`, which already links `holonight_surfaces` and already builds `test_tray_item.cpp`/`test_tray_item_properties.cpp` from the same directory).
  - Top-of-file comment: purpose = REQ-F-1.1/REQ-NF-1.1 regression coverage for `validateTrayPixmapDimensions()`.
  - Cases (calling `validateTrayPixmapDimensions()` directly — no `QImage`, no D-Bus, satisfies REQ-C-3):
    - `32x32`, data size `32*32*4` → `PixmapRejectReason::None`.
    - `512x512`, data size `512*512*4` → `None` (boundary-valid).
    - `513x513` → `DimensionTooLarge`.
    - `width = INT32_MAX, height = 1` → `DimensionTooLarge` (caught by the `> 512` check before any multiplication).
    - data length one byte short of `width*height*4` → `DataLengthMismatch`.
    - `width = 0` / `height = -1` → `NonPositiveDimensions`.
  - One additional case calling `decodePixmapList()` end-to-end with an oversized pixel list, asserting the returned `QImage::isNull()` and that no exception/crash occurs (covers REQ-F-1.3 at the unit level; full manual verification per REQ-F-1.2/1.3 remains as specified in SPEC.md).
- Register in `tests/CMakeLists.txt`: add `test_tray_pixmap_validation.cpp` to the `holonight_add_test_exe(test_holonight_surfaces ...)` file list (`tests/CMakeLists.txt:119-134`).
- Run `task configure-tests` then `task test`, confirm `test_holonight_surfaces` passes (SPEC.md names the target `test_tray_pixmap_validation` — this repo's convention is one executable per library with multiple `test_*.cpp` files linked in, so the actual invocation is `ctest -R test_holonight_surfaces --output-on-failure`, not a separately-named binary).

---

## Item 2: CalDAV Dead Timeout Guard + Duplicated HTTP-Sync Scaffolding

### Current code path

`libs/holonight-services/src/calendar/CalDavProvider.cpp:14-33`:

```cpp
Q_LOGGING_CATEGORY(lcCalDav, "holonight.calendar.caldav")
namespace {
constexpr int kHttpTimeoutMs{10000};
...
// Runs a synchronous HTTP request on the calling thread (worker thread, never main thread).
// Returns nullptr on timeout; caller owns the reply.
QNetworkReply* sendSync(QNetworkAccessManager& nam, const QNetworkRequest& req, const QByteArray& verb,
                        const QByteArray& body = {}) {
  QNetworkReply* reply = nam.sendCustomRequest(req, verb, body);
  QEventLoop loop;
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QTimer::singleShot(kHttpTimeoutMs, &loop, &QEventLoop::quit);
  loop.exec();
  return reply;
}
}
```

`QNetworkAccessManager::sendCustomRequest()` always returns a non-null `QNetworkReply*` synchronously (Qt's documented contract); the reply only transitions to finished/error state asynchronously. `sendSync()` races a `QEventLoop` against a 10s `QTimer::singleShot`, both of which call `loop.quit()`, and then **unconditionally returns the same non-null `reply`** regardless of which one fired. There is no branch that distinguishes "finished in time" from "timer fired first," and `reply->abort()` is never called on the hung reply — so a hung request is left running in the background, and the caller receives a reply that is still `isRunning() == true`, `error() == QNetworkReply::NoError` (nothing failed yet), and `readAll()` returns whatever partial/empty bytes have arrived so far.

Dead-code timeout checks fed by this, all in `CalDavProvider.cpp`:
- `resolvePrincipalUrl()`: line 204, `if (reply == nullptr || reply->error() != QNetworkReply::NoError)`
- `discoverCalendars()`: line 235, `if (reply == nullptr) { return std::unexpected(... "PROPFIND timed out") ...}`
- `fetchCalendarEvents()`: line 278, `... "REPORT timed out" ...`
- `testConnection()`: line 316, `... "connection timed out" ...`

None of these `reply == nullptr` branches is reachable. On an actual hang, the code falls through to the "success" path and returns an empty (or garbage-partial) result as if the server legitimately had nothing to say — exactly REQ-F-2.1/2.2's defect.

**The correct reference pattern**, `libs/holonight-services/src/calendar/IcsProvider.cpp:37-64`, `IcsProvider::httpGet()`:

```cpp
std::expected<QByteArray, SyncError> IcsProvider::httpGet() const {
  QNetworkAccessManager nam;
  QNetworkRequest req{QUrl(config_.url)};
  QNetworkReply* reply = nam.get(req);
  QEventLoop loop;
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QTimer::singleShot(kHttpTimeoutMs, &loop, &QEventLoop::quit);
  loop.exec();

  if (reply->isRunning()) {                      // <-- the check CalDavProvider is missing
    reply->abort();                              // <-- the call CalDavProvider is missing
    reply->deleteLater();
    return std::unexpected(makeConnectError(config_.account_name, QStringLiteral("connection timed out")));
  }
  if (reply->error() != QNetworkReply::NoError) {
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString detail = status > 0 ? QStringLiteral("HTTP %1").arg(status) : reply->errorString();
    reply->deleteLater();
    return std::unexpected(makeConnectError(config_.account_name, detail));
  }
  QByteArray body = reply->readAll();
  reply->deleteLater();
  return body;
}
```

`IcsProvider.cpp:17` independently declares its own `constexpr int kHttpTimeoutMs{10000}` — the exact same value as `CalDavProvider.cpp:18`, defined twice. `IcsProvider` uses plain `nam.get()` (GET-only, no custom verb needed for an ICS feed); `CalDavProvider` needs `sendCustomRequest()` for PROPFIND/REPORT verbs, which is the sole call site of `sendCustomRequest` in the entire repo (confirmed by repo-wide grep).

**`ICalendarProvider` interface** (`libs/holonight-services/src/calendar/ICalendarProvider.h`, full file):
```cpp
class ICalendarProvider {
 public:
  virtual ~ICalendarProvider() = default;
  ICalendarProvider(const ICalendarProvider&) = delete;
  ICalendarProvider& operator=(const ICalendarProvider&) = delete;
  ICalendarProvider(ICalendarProvider&&) = delete;
  ICalendarProvider& operator=(ICalendarProvider&&) = delete;
  [[nodiscard]] virtual QString accountName() const = 0;
  [[nodiscard]] virtual QString providerType() const = 0;
  [[nodiscard]] virtual std::expected<void, SyncError> testConnection() = 0;
  [[nodiscard]] virtual std::expected<QList<CalendarEvent>, SyncError> fetchEvents(const DateRange& range) = 0;
 protected:
  ICalendarProvider() = default;
};
```
There is **no `SyncResult` struct anywhere in the codebase** — the existing, consistent pattern is `std::expected<T, SyncError>`, with `SyncError` (`CalendarTypes.h:53-62`):
```cpp
struct SyncError {
  enum class Kind : std::uint8_t { ConnectError, NetworkError, ParseError, StorageError };
  Kind kind{Kind::ConnectError};
  QString message;
};
```

**`syncError`/`lastError` plumbing already exists end-to-end** and needs no new signal/property work:
- `CalendarSyncManager::syncError(SyncError::Kind, const QString&)` (declared `CalendarSyncManager.h:62`) is emitted from `onSyncFinished()`'s failure branch (`CalendarSyncManager.cpp:168-184`) and from `runTestConnections()` (`CalendarSyncManager.cpp:92`).
- `CalendarService` connects to it (`CalendarService.cpp:111-112`), maps `SyncError::Kind` to a QML-facing `UpcomingState` enum in `onSyncError()` (`CalendarService.cpp:144-164`), and exposes `Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged FINAL)` (`CalendarService.h:26,48,63`).
- Consumed today in `apps/shell/qml/RightSidebar/Tabs/Overview/SidebarOverviewUpcoming.qml:288,292`.

So the only genuinely new code required by REQ-F-2.2 is **making `CalDavProvider` actually produce a failing `SyncError` on timeout** (via the new `HttpSyncClient`) — the propagation path from `SyncError` to QML is already correct and unchanged.

### Proposed change

New class `HttpSyncClient`, files `libs/holonight-services/src/calendar/HttpSyncClient.h` / `.cpp` (same directory as the two providers it serves):

```cpp
// HttpSyncClient.h
#pragma once
#include "CalendarTypes.h"
#include <QByteArray>
#include <QNetworkRequest>
#include <QString>
#include <expected>

// Synchronous (blocking-the-calling-thread) HTTP request with an active timeout guard, shared by
// CalDavProvider and IcsProvider. Must be called from a worker thread (QtConcurrent::run), never
// the main/UI thread — it runs a nested QEventLoop.
//
// Timeout semantics: races a QEventLoop against QTimer::singleShot(timeout_ms). If the timer
// fires first, the in-flight QNetworkReply is actively aborted via reply->abort() (not just
// "given up on") before this function returns failure — this is the guard IcsProvider already
// implements correctly and CalDavProvider previously did not (see CalDavProvider history).
class HttpSyncClient {
 public:
  explicit HttpSyncClient(int timeout_ms = kDefaultTimeoutMs);

  // GET requests (used by IcsProvider). account_name is used only to label SyncError::message.
  [[nodiscard]] std::expected<QByteArray, SyncError> get(const QUrl& url, const QString& account_name) const;

  // Custom-verb requests (PROPFIND/REPORT, used by CalDavProvider). body may be empty.
  [[nodiscard]] std::expected<QByteArray, SyncError> sendCustomRequest(const QNetworkRequest& request,
                                                                       const QByteArray& verb,
                                                                       const QByteArray& body,
                                                                       const QString& account_name) const;

  static constexpr int kDefaultTimeoutMs{10000};  // matches both providers' current shared constant

 private:
  [[nodiscard]] std::expected<QByteArray, SyncError> runAndAwait(QNetworkReply* reply,
                                                                  const QString& account_name) const;
  int timeout_ms_;
};
```

`HttpSyncClient.cpp` — `runAndAwait()` is the single, shared implementation of the correct pattern (lifted from `IcsProvider::httpGet()`, generalized to take an already-issued `QNetworkReply*`):

```cpp
std::expected<QByteArray, SyncError> HttpSyncClient::runAndAwait(QNetworkReply* reply,
                                                                  const QString& account_name) const {
  QEventLoop loop;
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QTimer::singleShot(timeout_ms_, &loop, &QEventLoop::quit);
  loop.exec();

  if (reply->isRunning()) {
    reply->abort();
    reply->deleteLater();
    return std::unexpected(SyncError{.kind = SyncError::Kind::NetworkError,
                                      .message = QStringLiteral("%1: connection timed out").arg(account_name)});
  }
  if (reply->error() != QNetworkReply::NoError) {
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString detail = status > 0 ? QStringLiteral("HTTP %1").arg(status) : reply->errorString();
    reply->deleteLater();
    return std::unexpected(
        SyncError{.kind = SyncError::Kind::NetworkError, .message = QStringLiteral("%1: %2").arg(account_name, detail)});
  }
  QByteArray body = reply->readAll();
  reply->deleteLater();
  return body;
}

std::expected<QByteArray, SyncError> HttpSyncClient::get(const QUrl& url, const QString& account_name) const {
  QNetworkAccessManager nam;
  return runAndAwait(nam.get(QNetworkRequest{url}), account_name);
}

std::expected<QByteArray, SyncError> HttpSyncClient::sendCustomRequest(const QNetworkRequest& request,
                                                                       const QByteArray& verb, const QByteArray& body,
                                                                       const QString& account_name) const {
  QNetworkAccessManager nam;
  return runAndAwait(nam.sendCustomRequest(request, verb, body), account_name);
}
```

`CalDavProvider` changes:
- Delete the local `sendSync()` free function and `kHttpTimeoutMs` constant (`CalDavProvider.cpp:16-33`, minus the XML-parsing helpers that stay).
- Add a `HttpSyncClient http_client_;` member (or construct one per call — it's stateless besides `timeout_ms_`, so a single member is fine).
- Replace each `sendSync(...)` call + its dead `reply == nullptr` check with a call to `http_client_.sendCustomRequest(request, verb, body, config_.account_name)`, propagating `std::unexpected` directly instead of the current `reply == nullptr` / `reply->error() != NoError` two-step. E.g. `resolvePrincipalUrl()` and friends change their return type from raw `QNetworkReply*`-derived string-scraping to consuming `std::expected<QByteArray, SyncError>` up front, then parsing the body only on the success path.

`IcsProvider` changes:
- Delete `IcsProvider::httpGet()`'s body (`IcsProvider.cpp:37-64`) and its local `kHttpTimeoutMs` (`IcsProvider.cpp:17`); replace with a call to a `HttpSyncClient` member: `return http_client_.get(QUrl(config_.url), config_.account_name);`

After this, exactly two `HttpSyncClient` call sites remain doing the underlying Qt network calls (inside `HttpSyncClient::get`/`sendCustomRequest` themselves) — satisfying REQ-F-2.3's grep-for-`sendCustomRequest` acceptance criterion (one call site, inside `HttpSyncClient.cpp`, used by both providers via composition rather than two independent `sendCustomRequest`/`get` call sites in the providers themselves).

### Interfaces / APIs

```cpp
class HttpSyncClient {
 public:
  explicit HttpSyncClient(int timeout_ms = kDefaultTimeoutMs);
  [[nodiscard]] std::expected<QByteArray, SyncError> get(const QUrl& url, const QString& account_name) const;
  [[nodiscard]] std::expected<QByteArray, SyncError> sendCustomRequest(const QNetworkRequest& request,
                                                                       const QByteArray& verb, const QByteArray& body,
                                                                       const QString& account_name) const;
  static constexpr int kDefaultTimeoutMs{10000};
};
```
Both methods return `std::expected<QByteArray, SyncError>` — matching the existing `ICalendarProvider`/`SyncError` convention exactly, so no downstream consumer (`CalendarSyncManager`, `CalendarService`) needs any interface change.

### Key decisions with rationale

- **Extract as a class, not free functions**: SPEC.md explicitly asks this question (REQ-F-2.3: "helper class (e.g. `HttpSyncClient`)"). A class over free functions specifically because `timeout_ms` is per-instance configurable (needed by the GTest in REQ-F-2.1, which must inject a 1-second timeout rather than the production 10s), and because a class gives a natural single place to attach future cross-cutting concerns (e.g. a shared `QNetworkAccessManager` instance, request logging) without changing every call site's signature again.
- **Keep `std::expected<QByteArray, SyncError>` rather than inventing a `SyncResult` struct**: SPEC.md's REQ-F-2.2 prose says "SyncResult struct or signal" as an example, but the codebase has no such struct anywhere, and `std::expected<T, SyncError>` is already the established, consistent idiom across `ICalendarProvider`. Introducing a parallel `SyncResult` type would violate this project's own consistency and gain nothing.
- **`runAndAwait()` takes an already-issued `QNetworkReply*` rather than constructing the request itself**: keeps `get()` (GET-only) and `sendCustomRequest()` (verb-parameterized) as thin, semantically distinct entry points while sharing 100% of the timeout/abort/error-mapping logic — avoids a combinatorial `get-with-timeout` vs `customRequest-with-timeout` duplication inside the new class itself.
- **No new signals added to `CalendarSyncManager`/`CalendarService`**: the `syncError`/`lastError` pipeline already exists and is already correctly wired (verified above); adding a second, parallel error-reporting path would be needless surface area.

### Alternatives considered

- **Fix `CalDavProvider::sendSync()` in place (add the missing `isRunning()`/`abort()` check) without extracting a shared class.** Rejected: this satisfies REQ-F-2.1/2.2 alone but not REQ-F-2.3, and the review report (§5.2) explicitly flags this exact duplication pattern as the root cause of the bug — patching one copy without deleting the duplicate leaves the same silent-divergence risk for the *next* maintainer who touches only one of the two copies.
- **Make `HttpSyncClient` a `QObject` with signals instead of a synchronous blocking call.** Rejected for this phase: both providers are explicitly documented as running "synchronously on worker threads via `QtConcurrent::run`" (`ICalendarProvider.h` class comment) — converting to signal/slot async would be a much larger refactor of `CalendarSyncManager`'s threading model, out of scope for a Phase-0 bug fix under REQ-C-2's "aggressive but scoped" framing. Async-ification of blocking I/O is explicitly deferred to Architecture Gap #5 (REPORT.md §5.5), not this cycle.

### Known risks

- **Breaking change to `CalDavProvider`'s and `IcsProvider`'s private helper signatures** (`sendSync()`, `httpGet()` are deleted). Both are private/anonymous-namespace or private-member functions with **zero external callers** — verified via grep, the only call sites are within their own `.cpp` files — so this is a safe, contained breaking change per REQ-C-2, not a public API break.
- `resolvePrincipalUrl()`, `discoverCalendars()`, `fetchCalendarEvents()`, `testConnection()` in `CalDavProvider` all need their internal control flow rewritten from "get raw `QNetworkReply*`, branch on nullptr/error, then read/parse" to "get `std::expected<QByteArray, SyncError>`, branch on `has_value()`, then parse the body." This is a larger diff than the other 5 items in this design doc — budget accordingly; it's the same class of change repeated four times, not four independent designs.
- The new exact-timeout unit test (REQ-F-2.1) needs a fake/mock `QNetworkAccessManager` or reply that never emits `finished()`. Qt does not provide a built-in fake network reply; the test will need a minimal `QNetworkReply` subclass or a local `QTcpServer` that accepts but never responds (the latter is closer to a real integration test and slower — prefer a `QNetworkReply` test double if `HttpSyncClient` is refactored to accept an injected reply-producer, or accept the `QTcpServer`-based approach if that refactor is judged excessive for Phase 0). This repo has no precedent fake for `QNetworkAccessManager`; expect this to be the trickiest test to write.

### Test plan

- New file: `tests/test_http_sync_client.cpp`, added to `test_holonight_services` (`tests/CMakeLists.txt:76-108`, which already links `holonight_services`, `Qt6::Test`, `Qt6::Network` transitively via `holonight_services`).
  - Top-of-file comment: purpose = REQ-F-2.1 (active timeout/abort) + REQ-F-2.3 (shared helper correctness).
  - Case: construct `HttpSyncClient(1000)` (1s timeout), issue a request against a local `QTcpServer` bound to `127.0.0.1` that accepts the connection but never writes a response; assert the call returns `std::unexpected` with `SyncError::Kind::NetworkError` and message containing "timed out"; assert total wall-clock time is close to 1s (bounded upper check, e.g. `< 3000ms`) to confirm the timer actually fired rather than hanging.
  - Case: same server immediately closes the connection — assert `NetworkError` with a non-timeout message (exercises the `reply->error() != NoError` branch).
  - Case: server responds with a valid small body — assert success and correct bytes returned.
- Update/replace calendar test coverage: `tests/test_calendar_integration.cpp` (already exists, listed at `tests/CMakeLists.txt:85`) should gain a case asserting that a `CalDavProvider`/`IcsProvider` sync failure (via a fake/short-timeout `HttpSyncClient`) results in `CalendarSyncManager::syncError` firing and `CalendarService::lastError` becoming non-empty — closing the loop from REQ-F-2.2's manual criterion into an automated one, since the wiring already exists and is cheap to assert against.
- Register both files' additions in `tests/CMakeLists.txt` under `test_holonight_services`'s source list; run `task configure-tests` (mandatory per CLAUDE.md's stale-configure-dep gotcha) then `task test`.

---

## Item 3: ConfigWriter Silently Deletes Weather Coordinates on Every Save

### Current code path

`libs/holonight-config/src/ConfigWriter.cpp:168-177` (inline in `ConfigWriter::write()`, not a separate `writeWeather()` helper unlike sibling sections):

```cpp
out << "[weather]\n";
out << "api_key = " << tomlQuote(config.weather.api_key)
    << " # openweathermap.org key; empty = weather widget disabled\n";
out << "geo_api_key = " << tomlQuote(config.weather.geo_api_key)
    << " # ipgeolocation.io key; used only when latitude/longitude are unset\n";
out << "# latitude = 49.83968  # uncomment to pin location and skip IP geolocation\n";   // line 173
out << "# longitude = 24.02972\n";                                                        // line 174
out << "units = " << tomlQuote(config.weather.units) << " # metric | imperial | standard\n";
out << "lang = " << tomlQuote(config.weather.lang) << "\n";
out << "refresh_interval = " << config.weather.refresh_interval << " # seconds; must be > 0\n\n";
```

Lines 173-174 are **hardcoded example text**, completely ignoring `config.weather.latitude`/`longitude` — real values are silently discarded on every write. There is **no line at all for `config.weather.city`** — it is dropped unconditionally regardless of whether it holds a value.

`WeatherConfig` (`libs/holonight-config/include/holonight_config/config_structs.h:119-132`):
```cpp
struct WeatherConfig {
  QString api_key;
  QString geo_api_key;
  std::optional<double> latitude;   // absence is meaningful: triggers IP geolocation
  std::optional<double> longitude;
  QString city;                     // informational display label
  QString units{"metric"};
  QString lang{"en"};
  int refresh_interval{600};
  bool operator==(const WeatherConfig&) const = default;
};
```

**Critically, by the time `ConfigWriter::write()` is called, `config.weather.latitude`/`longitude`/`city` already hold the correct on-disk values** — this is not a "we lost track of the user's edit" bug, it's a pure write-side bug:
- `parseWeather()` (`libs/holonight-config/src/ConfigParsers.cpp:254-287`) correctly reads `latitude`/`longitude`/`city` from disk into `WeatherConfig`.
- `SettingsEditModel` (`apps/settings/src/SettingsEditModel.h`) has **no dedicated weather properties at all** — no `Q_PROPERTY` for latitude/longitude/city, no setters, no touched-tracking. It only carries `current_`/`snapshot_` as whole `ParsedConfig` value copies (which embed `WeatherConfig weather;`); `ConfigFileService::load()` does `current_ = config; snapshot_ = config;` on load, and since nothing in the Settings UI mutates weather fields, `current_.weather.latitude/longitude/city` still hold the loaded values verbatim at save time.
- `ConfigFileService::save()` calls `ConfigWriter::write(model_->toParsedConfig(), path)`, and `toParsedConfig()` is simply `return current_;` — so the writer receives a `ParsedConfig` whose `weather` member is already correct. **The bug is that the writer ignores the data it was handed and prints placeholder text instead.**

This means REQ-F-3.1's "if the user's edit model has no explicit value ... the ConfigWriter must preserve the existing on-disk value" is **already true by construction** — there is no touched/untouched distinction to build, because `SettingsEditModel` doesn't expose per-field weather editing at all yet. The entire fix is: make the writer actually serialize the struct fields it already has.

`ConfigWriter::write()`'s signature (`libs/holonight-config/include/holonight_config/config_writer.h`):
```cpp
class ConfigWriter {
 public:
  ConfigWriter() = delete;
  [[nodiscard]] static bool write(const ParsedConfig& config, const QString& path);
};
```
It takes a fully-materialized `ParsedConfig` value and does a **full-file rewrite from scratch** (via `QSaveFile`/`QTextStream`, section by section) — it has no access to (and does not need) the original on-disk `toml::table`, since the value it's handed is already correct.

A separate, unrelated "preserve what's on disk" idiom exists (`writeMissingDefaults()`/`MissingDefaults`, `ConfigParsers.cpp:770-814`) but operates on raw text lines to backfill *missing* keys after parse, is invoked only from `ConfigService::parseFile()` (not the Settings save path), and — notably — that idiom **also** deliberately omits `latitude`/`longitude` from write-back (`weatherDefaultLines()`, `ConfigParsers.cpp:622-642`, by design, since their absence is meaningful) and never touches `city` either. It is not a template to copy for this fix; it solves a different problem (defaulting absent keys, not round-tripping present ones).

### Proposed change

Replace `ConfigWriter.cpp:173-174` (and add a `city` line) with conditional serialization of the actual struct fields:

```cpp
out << "api_key = " << tomlQuote(config.weather.api_key)
    << " # openweathermap.org key; empty = weather widget disabled\n";
out << "geo_api_key = " << tomlQuote(config.weather.geo_api_key)
    << " # ipgeolocation.io key; used only when latitude/longitude are unset\n";
if (config.weather.latitude) {
  out << "latitude = " << *config.weather.latitude << "\n";
} else {
  out << "# latitude = 49.83968  # uncomment to pin location and skip IP geolocation\n";
}
if (config.weather.longitude) {
  out << "longitude = " << *config.weather.longitude << "\n";
} else {
  out << "# longitude = 24.02972\n";
}
if (!config.weather.city.isEmpty()) {
  out << "city = " << tomlQuote(config.weather.city) << "\n";
}
out << "units = " << tomlQuote(config.weather.units) << " # metric | imperial | standard\n";
out << "lang = " << tomlQuote(config.weather.lang) << "\n";
out << "refresh_interval = " << config.weather.refresh_interval << " # seconds; must be > 0\n\n";
```

This preserves the existing "commented placeholder as documentation" behavior **only** for the genuinely-unset case (a brand-new config with no pinned location — `std::nullopt`), and writes the real value whenever one is present, whether it came from disk-round-trip or (in a future cycle) an actual Settings-UI edit. `city` is written only when non-empty (matching the optional/informational nature documented on the struct), with no comment-placeholder needed since it was never emitted before at all.

REQ-F-3.1's second acceptance-criterion variant ("model's weather fields explicitly cleared (set to empty string)") maps directly: an explicitly-cleared `QString city` (`isEmpty() == true`) is preserved as absent (no line emitted, matching "before" state for a config that never had `city` set) — there is no separate "cleared vs never-set" state possible for a plain `QString`, so "preserved as empty" is satisfied by simply not emitting the line, consistent with how `city` behaved (silently) before this fix for configs that never had it.

### Interfaces / APIs

No new class or function signature — this is a localized, in-place change to the body of `ConfigWriter::write()` at `libs/holonight-config/src/ConfigWriter.cpp:168-177`. No header changes.

### Key decisions with rationale

- **Fix the writer directly rather than adding touched-tracking to `SettingsEditModel`.** SPEC.md's REQ-F-3.1 prose ("If the user's edit model has no explicit value...") implies a dirty-tracking design, but direct inspection shows `SettingsEditModel` doesn't expose weather fields for editing at all yet — there is nothing to "touch." Building a touched-tracking mechanism for fields the UI can't even edit would be speculative, unrequested scope creep. If/when a future cycle adds a weather-location editor to Settings, `current_`/`snapshot_`'s existing whole-struct comparison (`recomputeDirty()`) already generalizes correctly to that case with zero further change here, since `WeatherConfig::operator==` is already `= default`.
- **Preserve the comment-placeholder text for the genuinely-absent case.** Alternative: always emit `latitude`/`longitude` (defaulting to some sentinel like `0.0`). Rejected — `0.0` is a valid coordinate ("null island," per the struct's own comment) and would silently turn "use IP geolocation" into "pin to the Gulf of Guinea," a worse bug than the one being fixed.

### Alternatives considered

- **Switch `ConfigWriter` to a toml++-table-based read-modify-write instead of full regeneration from a `ParsedConfig` struct.** This would generalize better to arbitrary future "preserve anything I don't understand" needs (e.g. hand-edited comments, unknown keys), but is a much larger architectural change to a writer used across the whole config-writing surface (not just weather), with no other current requirement demanding it. Rejected for Phase 0 scope; flagged as a reasonable direction for a later cycle if more silent-data-loss bugs surface elsewhere in `ConfigWriter`.

### Known risks

- `ConfigWriter::write()` has a second call site: `libs/holonight-core/src/ConfigService.cpp:59`, bootstrapping a brand-new default config (`ConfigWriter::write(ParsedConfig{}, config_path_)`) when none exists. A default-constructed `WeatherConfig` has `latitude`/`longitude` as `std::nullopt` and `city` empty, so this path is unaffected by the fix (falls into the "still emit commented placeholder" branch) — verified safe, no behavior change for first-run bootstrap.
- None of the existing `ConfigWriter`/`ConfigParsers` GTest fixtures (`tests/test_config_parsers.cpp`) currently assert on `latitude`/`longitude`/`city` round-tripping through a full write — the new test (below) is additive, not a modification of existing assertions, so no existing test should need updating.

### Test plan

- New file: `tests/test_configwriter_weather_preservation.cpp`, added to `test_holonight_settings` (`tests/CMakeLists.txt:54-73`, which already links `holonight_config` and builds against `apps/settings/src/SettingsEditModel.cpp`/`ConfigFileService.cpp` directly) — this is the correct target since the acceptance criterion (REQ-F-3.1) explicitly exercises `SettingsEditModel` + `ConfigWriter::write()` together, not `ConfigWriter` in isolation.
  - Top-of-file comment: purpose = REQ-F-3.1 regression coverage for weather-field preservation on save.
  - Case 1: build a `ParsedConfig` with `weather.latitude = 42.3`, `weather.longitude = -71.1`, `weather.city = "Boston"`; call `ConfigWriter::write()` to a temp file; re-parse it with `parseWeather()` (or `ConfigService`'s full parse path); assert `latitude == 42.3`, `longitude == -71.1`, `city == "Boston"`, and that the raw text does **not** contain the string `"# latitude ="` (i.e., confirms the real value was written, not the placeholder).
  - Case 2: build a `ParsedConfig` with `weather.latitude = std::nullopt`, `weather.longitude = std::nullopt`, `weather.city = ""`; write; re-parse; assert both remain `std::nullopt` and `city` remains empty, and that the commented placeholder text is present (documents the "never pinned" case still renders helpful example text).
  - Case 3 (SPEC.md's full load→no-op→save→reload flow): load a fixture TOML with weather fields set via `SettingsEditModel::setFromParsedConfig()`, immediately call `toParsedConfig()` + `ConfigWriter::write()` with no intervening mutation, reload, assert equality with the original fixture — directly automates REQ-F-3.2's manual scenario end-to-end (open/close Settings without editing weather).
- Register in `tests/CMakeLists.txt` under `test_holonight_settings`'s source list (`tests/CMakeLists.txt:54-61`).
- Run `task configure-tests` then `task test`.

---

## Item 4: HolonightTheme QML Singleton — Corrected Root-Cause Design

### Current code path (verified directly; diverges from SPEC.md's assumed cause)

Only one file references `HolonightTheme`, in exactly two places: `apps/settings/qml/AppearancePage.qml:58` (`model: HolonightTheme.themeFamilies`) and `:121` (`model: HolonightTheme.accentOptionsForScheme(editModel.themeScheme)`). The file's imports (`AppearancePage.qml:1-5`) already include `import Holonight` — the reference is not missing an import statement, and the same file successfully uses `HoloniightPalette` (double-i) from the identical import elsewhere.

Repo-wide grep for `qmlRegisterSingletonType.*HolonightTheme` or a `QML_SINGLETON`-tagged `HolonightTheme` class returns **nothing inside `holonight-shell`** — but that is because the class does not belong to this repo. `HolonightTheme` is a real, already-implemented class in the sibling repo `holonight-qt`, at `qml/holonighttheme.h:12-31`:

```cpp
class HolonightTheme : public QObject {
  Q_OBJECT
  QML_SINGLETON
  QML_ELEMENT
  Q_PROPERTY(QVariantList themeFamilies READ themeFamilies CONSTANT)
  Q_PROPERTY(QVariantList themeVariants READ themeVariants CONSTANT)
 public:
  explicit HolonightTheme(QObject* parent = nullptr);
  Q_INVOKABLE QVariantList accentOptionsForScheme(const QString& scheme_id) const;
  ...
};
```

registered automatically via that repo's own `qt_add_qml_module(holonight_qml URI "Holonight" ... SOURCES holoniightpalette.h/.cpp holonighttheme.h/.cpp ...)` (`holonight-qt/qml/CMakeLists.txt:1-11`) — `QML_SINGLETON`+`QML_ELEMENT` auto-register both classes when that plugin builds. **Verified present today** in the currently-installed system module: `/usr/lib/qt6/qml/Holonight/holonight_qml.qmltypes` contains `HolonightTheme` (confirmed via direct grep of the installed file on this machine), immediately alongside `HoloniightPalette`.

Crucially: **neither `apps/settings/CMakeLists.txt` nor `apps/shell/CMakeLists.txt` link the `holonight_qml` plugin target at all** (both link only `HolonightQt::Config`/`holonight_componentsplugin`, a *different*, separate `Holonight.Components` module for `ContentSeparator`/`ExternalIcon`). `HoloniightPalette` and `HolonightTheme` both resolve purely through Qt's runtime QML plugin auto-loading, triggered by `import Holonight`, via the `qmldir`'s `optional plugin holonight_qml` directive — not through any explicit C++ registration call in this repo. **There is no header available to `apps/settings/src/` for a class called `HolonightTheme`** (only compiled `.so`/`qmldir`/`.qmltypes`/`.qml` files are installed by `holonight-qt`), so SPEC.md's proposed fix — "add a `qmlRegisterSingletonType<HolonightTheme>(...)` call in `apps/settings/src/SettingsApp.cpp`" — **cannot be implemented as written**: the type isn't visible to this repo's C++ at all.

Also worth noting: `apps/settings/src/SettingsApplication.cpp:24-28` doesn't use `qmlRegisterSingletonType` for *anything* — it exposes its own C++ objects via `setContextProperty()` (`editModel`, `fileService`, `shellStatus`, `appVersion`). The `qmlRegisterSingletonType` pattern SPEC.md points to as "the existing convention to mirror" only exists in the **shell** app (`apps/shell/app/ShellApplication.cpp:143-166`, registering `AppearanceService`/`ThemeService` — different classes, different QML module `"HolonightShell"`, unrelated to `HolonightTheme`/`"Holonight"`). SPEC.md's REQ-F-4.1 acceptance criterion ("grep `SettingsApp.cpp` for a HolonightTheme registration ... same convention as other registered singletons") assumes a convention that doesn't exist in the Settings app.

**Given the installed plugin already exports `HolonightTheme` today, the described `ReferenceError` may no longer reproduce at all** — it's plausible the sibling `holonight-qt` repo fixed this after the review's snapshot (a commit titled "expose shared theme catalog" is visible in that repo's history, and the installed `.qmltypes`/`.so` timestamps are recent, 2-3 Jul 2026). This must be empirically re-verified before writing any code.

### Proposed change

**Step 0 (must run first, changes everything downstream): empirically re-verify the defect still reproduces.**
```
QT_FORCE_STDERR_LOGGING=1 ./build/holonight-settings 2>&1 | grep -i "ReferenceError.*HolonightTheme"
```
- **If no ReferenceError appears** (most likely given the installed plugin already contains the type): the runtime defect is already resolved upstream. The remaining work is purely the regression-test gap (Step 2 below) — do not write any "registration" C++ code, since there is nothing to register from this repo.
- **If a ReferenceError still appears**: the cause is an import-path/version resolution issue specific to how `holonight-settings` resolves the `Holonight` QML module at runtime (e.g., a stale locally-built `holonight_qml` plugin predating the "expose shared theme catalog" commit on `QML2_IMPORT_PATH`, or a version-pinning mismatch in whatever packages/vendors the `HolonightQt` dependency for this build). In that case, the fix is a build/dependency-pinning fix (bump the vendored/installed `HolonightQt` package to a version containing `HolonightTheme`), not new registration code in `holonight-shell` — there is no C++ call this repo can add, because the header is not distributed to it.

**Step 1 (only if Step 0 finds a real defect and it's within this repo's control):** confirm the `Holonight` QML import resolves identically for `HoloniightPalette` and `HolonightTheme` at the Settings app's actual runtime `QML2_IMPORT_PATH` (compare against the shell app's, which is known-working, per CLAUDE.md's `Holonight.Components` module notes) — this is a build/packaging investigation, not a code change, so it's scoped as a manual/CI verification step rather than a source diff.

**Step 2 (do regardless of Step 0's outcome — this is the one genuinely-missing regression-test seam):** add a `HolonightTheme` fake to the test harness, mirroring the existing `HoloniightPalette` stub:

`tests/FakeQmlServices.h:715` currently has:
```cpp
qmlRegisterSingletonInstance("Holonight", 1, 0, "HoloniightPalette", &palette_) >= 0 &&
```
Add an equivalent registration for a minimal `HolonightTheme` fake QObject (new small class in `FakeQmlServices.h`, e.g. `FakeHolonightTheme`) exposing stub `themeFamilies`/`themeVariants`/`accentOptionsForScheme()` matching the real class's shape closely enough for `AppearancePage.qml` to bind without a `ReferenceError`:

```cpp
class FakeHolonightTheme : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList themeFamilies READ themeFamilies CONSTANT)
 public:
  [[nodiscard]] QVariantList themeFamilies() const { return {}; }
  Q_INVOKABLE QVariantList accentOptionsForScheme(const QString&) const { return {}; }
};
```
registered alongside the existing fakes:
```cpp
qmlRegisterSingletonInstance("Holonight", 1, 0, "HolonightTheme", &fake_theme_) >= 0 &&
```

This closes the gap the exploration surfaced: no test currently loads `AppearancePage.qml` at all, so this class of regression (real or upstream-fixed) is currently invisible to CI either way.

### Interfaces / APIs

No production registration call — see Step 0/1 above; this item's "interface" is the test-only `FakeHolonightTheme` stub in `tests/FakeQmlServices.h`, registered via `qmlRegisterSingletonInstance("Holonight", 1, 0, "HolonightTheme", &fake_theme_)`, matching the exact convention already used one line above it for `HoloniightPalette`.

### Key decisions with rationale

- **Do not write speculative C++ registration code for a class this repo doesn't own and can't include.** Writing code against a premise a direct read of the repo disproves would be actively harmful — it would either fail to compile (no header) or require vendoring/duplicating the sibling repo's class, which is a much bigger, unrequested architectural change (forking a shared design-system component) that this Phase-0 bug-fix cycle has no mandate to make.
- **Treat this as "verify, then close the test gap" rather than "implement the fix described in SPEC.md."** This is the single most important judgment call in this document — flagged prominently per the task's request to surface any item where the code doesn't match the review's description.
- **Add the `FakeQmlServices.h` stub regardless of Step 0's outcome.** Even if the runtime bug is already gone, the test harness gap (zero coverage of `AppearancePage.qml`, and no fake for a singleton the page depends on) is real today and cheap to close — worth doing unconditionally as the durable regression guard REQ-F-4.3 asks for.

### Alternatives considered

- **Vendor/copy `HolonightTheme`'s implementation into `holonight-shell` as a local class, registered explicitly via `qmlRegisterSingletonType`.** Rejected: creates two divergent copies of the same design-system class across two repos — exactly the "copy-pasted logic diverges silently" pattern the review report (§5.2) flags as this codebase's second-most systemic risk. If the real class genuinely doesn't resolve at Settings-app runtime, fixing the *import*/*packaging* is correct; forking the class is not.
- **Extend `task qmltypes-check` to assert `HolonightTheme` appears in generated metatypes**, per REQ-F-4.3's "Alternatively" clause. Investigated: `task qmltypes-check` (`Taskfile.yml:170-174`, `scripts/check-qmltypes.sh`) is scoped specifically to the `HolonightShell` module generated by *this repo's* `apps/shell` build — it has no visibility into the external, separately-built `Holonight` module (owned by `holonight-qt`) that `HolonightTheme` lives in. Extending it to check an externally-installed `.qmltypes` file is possible but weaker signal than an actual QML-load smoke test (Step 2/Test plan below) and was judged not worth the extra task-runner complexity for this cycle — the `FakeQmlServices.h` stub plus a QML smoke test is the more direct, more valuable check.

### Known risks

- If Step 0 finds the `ReferenceError` **does** still reproduce, the actual root cause may sit outside this repo's build (e.g., a pinned/vendored `HolonightQt` package version), which this design cannot fully resolve without visibility into how that dependency is packaged/versioned for this build — flag to whoever owns the `holonight-qt` packaging/vendoring step if Step 0 turns up a real defect.
- The `FakeHolonightTheme` stub's property/method surface must be kept in sync with the real class if `holonight-qt` adds more theme-catalog properties later — same category of drift risk as any test double, no different from the existing `HoloniightPalette` fake's maintenance burden.

### Test plan

- New file: `tests/qml/tst_holonight_theme_singleton.qml`, following REQ-F-4.3's first (QML smoke test) option, registered with the existing QtQuickTest harness (`test_holonight_qml_harness` target, `tests/CMakeLists.txt:168-189`, which already loads `FakeQmlServices` globally and discovers `tests/qml/tst_*.qml` files).
  - `TestCase` that imports `Holonight`, references `HolonightTheme.themeFamilies` and calls `HolonightTheme.accentOptionsForScheme("dark")`, and asserts no error is thrown (a QML `TestCase` failure surfaces automatically if a `ReferenceError` occurs during property binding).
- Modify `tests/FakeQmlServices.h`: add `FakeHolonightTheme` class + registration (see Proposed Change, Step 2) so the harness's globally-registered singleton set includes `HolonightTheme` alongside `HoloniightPalette`.
- No `tests/CMakeLists.txt` change needed beyond what already exists — `test_holonight_qml_harness` already globs/discovers `tests/qml/tst_*.qml` at runtime via QtQuickTest; only `FakeQmlServices.h` (already a listed source at `tests/CMakeLists.txt:170`) needs the new class added.
- Run `task configure-tests` then `ctest -R test_holonight_qml_harness -V` to confirm the new case passes.

---

## Item 5: Control-Socket Sidebar DoS via Unvalidated Monitor Name

### Current code path

`apps/shell/app/ControlServer.cpp` (confirmed actual location — not `libs/holonight-app/src/` as project memory/SPEC.md's root-location note suggested; `ControlServer` lives under `apps/shell/app/`, part of the `holonight_app` CMake target).

`ControlServer::decodeCommand()` (`ControlServer.cpp:38-47`) parses `sidebar:toggle:<monitor>` into a `DecodedCommand{type: ToggleSidebar, argument: monitor_name}` — pure string parsing, no validation, and this part is already correctly unit-tested (`tests/test_control_server.cpp`, existing). `ControlServer::handleCommand()` (`ControlServer.cpp:60-72`) just re-emits `toggleSidebarRequested(decoded.argument)` — **`ControlServer` itself never validates the monitor name; it's a dumb parser/dispatcher.**

The actual bug is one layer up, in `apps/shell/app/ShellApplication.cpp:219-223`:
```cpp
connect(control_server_, &ControlServer::toggleSidebarRequested, this, [this](const QString& monitor_name) {
  if (sidebar_manager_ != nullptr) {
    sidebar_manager_->toggle(monitor_name);
  }
});
```
which forwards directly into `SidebarManager::toggle()` (`libs/holonight-surfaces/src/SidebarManager.cpp:88-96`):
```cpp
void SidebarManager::toggle(const QString& monitor_name) {
  qCInfo(lcSidebar) << "toggle" << monitor_name << "open" << open_state_.value(monitor_name, false);
  if (open_state_.value(monitor_name, false)) {
    closeOnMonitor(monitor_name);
    return;
  }
  closeAll();                        // <-- tears down whatever sidebar IS open, unconditionally
  openOnMonitor(monitor_name);       // <-- only NOW does createSurface() check the monitor exists
}
```

This is the exact bug: if `monitor_name` is not currently open (the common case for a malformed/nonexistent name), `closeAll()` (line 94) runs **before** any check that `monitor_name` is real — it tears down any sidebar currently open on any monitor. Only afterward does `openOnMonitor()` → `createSurface()` (`SidebarManager.cpp:183-232`) discover the name is bogus, via `findScreen(monitor_name) == nullptr` (`SidebarManager.cpp:188-192`, logs `qCWarning(lcSidebar) << "createSurface: no screen for monitor" << monitor_name;` and returns `false`) — but by then the damage (closing the user's real, open sidebar) is already done.

**There is no `MonitorManager` class.** The "known monitor list" is `QGuiApplication::screens()`, consulted directly and only inside the private `SidebarManager::findScreen()` (`SidebarManager.cpp:261-268`):
```cpp
QScreen* SidebarManager::findScreen(const QString& monitor_name) {
  for (QScreen* screen : QGuiApplication::screens()) {
    if (screen->name() == monitor_name) { return screen; }
  }
  return nullptr;
}
```

### Proposed change

Reorder `SidebarManager::toggle()` to validate before any teardown, using a new, explicitly-named, easily-testable predicate:

```cpp
// SidebarManager.h — new public method, alongside existing isOpen()/toggle()/close()
[[nodiscard]] Q_INVOKABLE bool isKnownMonitor(const QString& monitor_name) const;
```

```cpp
// SidebarManager.cpp
bool SidebarManager::isKnownMonitor(const QString& monitor_name) const {
  return findScreen(monitor_name) != nullptr;
}

void SidebarManager::toggle(const QString& monitor_name) {
  if (!isKnownMonitor(monitor_name)) {
    qCWarning(lcSidebar) << "toggle: rejected unknown monitor" << monitor_name;
    return;                                    // REQ-F-5.1: no state change on invalid name
  }
  qCInfo(lcSidebar) << "toggle" << monitor_name << "open" << open_state_.value(monitor_name, false);
  if (open_state_.value(monitor_name, false)) {
    closeOnMonitor(monitor_name);
    return;
  }
  closeAll();
  openOnMonitor(monitor_name);
}
```

For **GTest isolation without a live Wayland/screen session** (REQ-C-3), extract the pure name-matching logic out of `findScreen()`'s `QGuiApplication::screens()` dependency into a small free function that both `findScreen()` and a test can call with an explicit name list:

```cpp
// SidebarManager.cpp (anonymous namespace, next to findScreen)
bool isMonitorNameInList(const QString& monitor_name, const QStringList& known_names) {
  return known_names.contains(monitor_name);
}
```
```cpp
QScreen* SidebarManager::findScreen(const QString& monitor_name) {
  for (QScreen* screen : QGuiApplication::screens()) {
    if (screen->name() == monitor_name) { return screen; }
  }
  return nullptr;
}
```
Given the offscreen QPA platform used by this repo's tests (`tests/CMakeLists.txt:15`, `QT_QPA_PLATFORM=offscreen`) does create at least one real `QScreen`, `SidebarManager::isKnownMonitor()` can actually be tested directly against `QGuiApplication::screens()`'s real (offscreen-platform-provided) name for the "valid" case, and any string not matching it for the "invalid" case — no separate mock abstraction is strictly required. This is simpler than SPEC.md's suggested "mock `MonitorManager`" (no such class exists to mock) and keeps `SidebarManager` free of a speculative new seam it doesn't otherwise need.

### Interfaces / APIs

```cpp
// SidebarManager.h
[[nodiscard]] Q_INVOKABLE bool isKnownMonitor(const QString& monitor_name) const;
```
No other signature changes; `toggle()`'s signature is unchanged (`void toggle(const QString& monitor_name)`), only its body gains an early-return guard.

### Key decisions with rationale

- **Validate-before-teardown (reject the whole command atomically) rather than validate-and-rollback.** SPEC.md's own framing ("Item 5... Alternatives") anticipates this choice. Rollback (tear down, discover invalid, reopen the previous sidebar) is strictly worse: it still produces a visible flicker/close-reopen for the victim, requires remembering what was open before (extra state, extra bug surface), and there is no scenario where "close now, maybe reopen a moment later" is preferable to simply never closing anything for a request that was invalid from the start. Validate-first is both simpler and strictly more correct.
- **Add `isKnownMonitor()` as a thin public wrapper over the existing private `findScreen()`, not a new parallel monitor-list abstraction.** `findScreen()` already does exactly the lookup needed; duplicating a separate "monitor manager" class (as SPEC.md's acceptance criterion suggests mocking) would add a speculative abstraction this codebase doesn't otherwise have anywhere else for monitor identity (the review report §2.3 documents 7 recurring cross-cutting patterns; a new "MonitorManager" isn't one of them, and `ActiveWindowService`/`ExtWorkspaceManager` already own monitor-name-keyed state elsewhere without such a class).
- **Fix at the `SidebarManager::toggle()` level, not inside `ControlServer`.** `ControlServer` is a dumb transport-layer parser (correctly so — it doesn't know what a "monitor" even means, semantically); the DoS is a *state-machine* ordering bug in `SidebarManager`, which is where all the actual close/open sequencing lives. Fixing it in `ControlServer` would only protect the control-socket entry point, leaving any other future caller of `SidebarManager::toggle()` (e.g. a keyboard shortcut handler) equally vulnerable to the same ordering bug.

### Alternatives considered

- **Validate inside the `ShellApplication.cpp:219-223` lambda, before calling `sidebar_manager_->toggle()`.** Rejected — this only protects the one call site reachable from the control socket; `SidebarManager::toggle()` remains unsafe if called from anywhere else (and per the point above, `SidebarManager` is the correct owner of this invariant, not its caller).
- **Have `ControlServer` own a monitor-name allowlist itself (querying `QGuiApplication::screens()` directly in `ControlServer`).** Rejected: duplicates monitor-lookup logic that `SidebarManager` already has and would need to be kept in sync if screens are added/removed, reintroducing the "same logic in two places" divergence risk the review's §5.2 explicitly warns about.

### Known risks

- `isKnownMonitor()` being `Q_INVOKABLE` exposes it to QML — currently unused from QML, but consistent with the class's existing convention (`isOpen()`, `toggle()`, `close()`, `closeAll()` are all `Q_INVOKABLE` already), so no new precedent is set.
- No behavioral change for the already-open-and-valid case (`open_state_.value(monitor_name, false) == true` implies the monitor was valid when originally opened) — the new check only activates for the previously-unguarded "open a sidebar on a name we don't currently have open" branch, so no regression risk for the common toggle-closed path.

### Test plan

- New file: `tests/test_sidebar_manager.cpp`, added to `test_holonight_surfaces` (`tests/CMakeLists.txt:119-134`, already links `holonight_surfaces`; no existing `test_sidebar_manager.cpp` today — confirmed via the current file list, which has `test_sidebar_surface_policy.cpp` but nothing exercising `SidebarManager` itself).
  - Top-of-file comment: purpose = REQ-F-5.1/5.2 regression coverage for the validate-before-teardown ordering fix.
  - Construct a `SidebarManager` against a fake/no-op `LayerShell` (constructor already takes `LayerShell&`; check whether a test double already exists for `LayerShell` elsewhere in `tests/` — if not, this test may need a minimal fake `LayerShell` first, or may be scoped to testing `isKnownMonitor()` + `toggle()`'s early-return path without a live `QQuickView` being created, i.e. asserting `sidebarOpened`/`sidebarClosed` signals do NOT fire for a bogus name via `QSignalSpy`).
  - Case: call `toggle("DEFINITELY-NOT-A-REAL-MONITOR")`; assert via `QSignalSpy` that neither `sidebarOpened` nor `sidebarClosed` fires, and `isOpen()` remains `false`.
  - Case: with a sidebar already open on the offscreen platform's real screen name, call `toggle()` with a bogus name; assert the previously-open sidebar's `isOpen(<real name>)` is still `true` afterward (directly encodes "the bug" — before the fix, this would have called `closeAll()` and flipped it to `false`).
  - Case: call `toggle()` with the offscreen platform's actual screen name (from `QGuiApplication::screens().first()->name()`); assert the toggle proceeds (open_state flips, `sidebarOpened` fires) — the valid-name control case.
  - Capture a `qCWarning` assertion (via `QTest::ignoreMessage` or a custom message handler) confirming the rejected-name log line appears (REQ-F-5.1 point 3).
- Register `test_sidebar_manager.cpp` in `tests/CMakeLists.txt` under `test_holonight_surfaces`'s source list.
- Run `task configure-tests` then `task test`.

---

## Item 6: Unbounded Notification Payload Logging

### Current code path

`libs/holonight-services/src/notifications/NotificationServer.h` — `Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")` is present and correct (line ~16, per project memory's prior verification — re-confirmed present, matching CLAUDE.md's D-Bus-interface-declaration gotcha).

`NotificationServer::Notify()` (`libs/holonight-services/src/notifications/NotificationServer.cpp:115-130`):
```cpp
uint NotificationServer::Notify(const QString& app_name, uint replaces_id, const QString& app_icon,
                                const QString& summary, const QString& body, const QStringList& actions,
                                const QVariantMap& hints, int expire_timeout) {
  qCInfo(lcNotificationServer).nospace() << "Notify received - app_name: \"" << app_name
                                         << "\", replaces_id: " << replaces_id << ", app_icon: \"" << app_icon
                                         << "\", summary: \"" << summary << "\", body: \"" << body
                                         << "\", actions: " << actions << ", hints: " << hints
                                         << ", expire_timeout: " << expire_timeout;
  if (service_ == nullptr) { return 0; }
  NotificationData data =
      buildNotificationData(app_name, replaces_id, app_icon, summary, body, actions, hints, expire_timeout);
  return service_->addOrReplace(std::move(data));
}
```
Line 118-122 logs `summary`/`body`/`hints` completely unbounded, **before** any processing — a 10 MB `summary` is written to `~/.local/share/holonight/holonight-shell/holonight.log` verbatim, at `qCInfo` level (already meeting REQ-NF-2.1's "qCWarning or higher" bar is not required here since this is routine info logging, not a rejection diagnostic — but the payload itself must still be bounded per REQ-F-6.1).

`buildNotificationData()` (`NotificationServer.cpp:29-69`, anonymous namespace) constructs the `NotificationData` that flows onward to storage/display:
```cpp
NotificationData buildNotificationData(const QString& app_name, uint replaces_id, const QString& app_icon,
                                       const QString& summary, const QString& body, const QStringList& actions,
                                       const QVariantMap& hints, int expire_timeout) {
  NotificationData data;
  ...
  data.summary = summary;   // line 45 — unbounded
  data.body = body;         // line 46 — unbounded
  data.hints = hints;       // line 47 — unbounded (full raw map, including any oversized values)
  ...
  return data;
}
```
`NotificationData` (`libs/holonight-services/src/notifications/NotificationTypes.h:29-49`) is the single struct that flows to **all three** downstream consumers: the in-memory `NotificationModel` (live toast/tray display), the persisted notification-history store, and (per the log line above) the log file itself. Per REQ-F-6.1's explicit wording — "bound... before being logged, stored, or displayed" — bounding only the log call (leaving `data.summary`/`data.body`/`data.hints` unbounded for the model/history) would satisfy the letter of REQ-F-6.1's log-only acceptance criterion but not its "stored... or displayed" language, and would leave an oversized payload sitting in memory/on-disk history indefinitely. **This design bounds at `buildNotificationData()`'s construction site**, so every downstream consumer (log line, live model, persisted history) automatically receives the already-truncated value — one control point instead of three.

### Proposed change

New named constant + shared helper, in `NotificationServer.cpp`'s anonymous namespace (or a small header if reused elsewhere — checked, no existing `truncat*` utility exists anywhere in the repo, so this is a new, self-contained helper):

```cpp
// Freedesktop notifications carry no formal length limit; 4 KiB comfortably covers any
// legitimate summary/body/hint-value text (a full paragraph of body text is well under 1 KiB)
// while bounding worst-case per-field memory/disk impact from an untrusted D-Bus caller.
constexpr qsizetype kMaxNotificationFieldLength{4096};
constexpr auto kTruncationMarker = "...[truncated]";

QString truncateToMaxLength(const QString& value, qsizetype max_length = kMaxNotificationFieldLength) {
  if (value.length() <= max_length) {
    return value;
  }
  return value.left(max_length) + QString::fromLatin1(kTruncationMarker);
}
```

Apply it inside `buildNotificationData()`:
```cpp
data.summary = truncateToMaxLength(summary);
data.body = truncateToMaxLength(body);
...
QVariantMap bounded_hints;
for (auto it = hints.constBegin(); it != hints.constEnd(); ++it) {
  if (it.value().canConvert<QString>()) {
    bounded_hints.insert(it.key(), truncateToMaxLength(it.value().toString()));
  } else {
    bounded_hints.insert(it.key(), it.value());   // non-string hint values (bool/int/etc.) pass through unbounded-length, they have no "length" to bound
  }
}
data.hints = bounded_hints;
```
(REQ-F-6.3 requires "each hint value" bounded by the same max length — string-typed hint values are truncated identically to summary/body via the same `truncateToMaxLength()`; non-string hints such as the `urgency` uchar or `resident` bool have no meaningful string length and are left as-is, since REQ-F-6.1's concern — "arbitrarily large... writes into the persistent log file" — is specifically about string payloads.)

`Notify()`'s log line (lines 118-122) should log the **already-bounded** `data.summary`/`data.body`/`data.hints` (post-`buildNotificationData()`) rather than the raw D-Bus parameters, so the log line and storage are guaranteed to show the same (bounded) text — reorder slightly:
```cpp
uint NotificationServer::Notify(...) {
  if (service_ == nullptr) { return 0; }
  NotificationData data =
      buildNotificationData(app_name, replaces_id, app_icon, summary, body, actions, hints, expire_timeout);
  qCInfo(lcNotificationServer).nospace() << "Notify received - app_name: \"" << app_name
                                         << "\", replaces_id: " << replaces_id << ", app_icon: \"" << app_icon
                                         << "\", summary: \"" << data.summary << "\", body: \"" << data.body
                                         << "\", actions: " << actions << ", hints: " << data.hints
                                         << ", expire_timeout: " << expire_timeout;
  return service_->addOrReplace(std::move(data));
}
```
(`app_name`/`app_icon` are left unbounded per REQ-F-6.1's explicit field list — only summary/body/hints are named; `app_name`/`app_icon` come from the calling process's own declared identity, a materially smaller practical attack surface, and adding them would be free-standing scope beyond what REQ-F-6.1 asks. `actions` is a `QStringList`, not named in REQ-F-6.1 either, so left unbounded — flagged as a judgment call below.)

### Interfaces / APIs

```cpp
// NotificationServer.cpp, anonymous namespace
constexpr qsizetype kMaxNotificationFieldLength{4096};
constexpr auto kTruncationMarker = "...[truncated]";
[[nodiscard]] QString truncateToMaxLength(const QString& value, qsizetype max_length = kMaxNotificationFieldLength);
```
No header/public-API change — `truncateToMaxLength()` is a file-local helper, matching the existing `buildNotificationData()`'s own anonymous-namespace placement.

### Key decisions with rationale

- **Bound at `buildNotificationData()` (construction), not just at the log call site.** As argued above, this is the only way to satisfy REQ-F-6.1's "logged, stored, or displayed" wording with a single code path, avoiding a second, easy-to-forget truncation call at every future consumer of `NotificationData`.
- **4 KiB per field**, chosen because: (a) it's the exact figure SPEC.md's own example uses, (b) it comfortably exceeds any realistic legitimate notification body (a full paragraph of markup-formatted text is well under 1 KiB), and (c) it keeps worst-case per-notification memory/disk cost bounded to a small, predictable constant even under sustained hostile flooding (REQ-F-6.1's actual threat model).
- **A single named constant (`kMaxNotificationFieldLength`) used by one shared function**, directly satisfying REQ-F-6.3's explicit ask ("defined once... not repeated in multiple places where it could drift").

### Alternatives considered

- **Truncate at the D-Bus argument marshalling boundary (reject/truncate in `Notify()`'s parameters before they're even used) rather than inside `buildNotificationData()`.** Considered equivalent in effect but rejected as the primary mechanism because `Notify()`'s parameters are also passed to `buildNotificationData()` for icon-hint fallback logic (`hints.value("image-path")` etc., lines 37-43) — truncating too early risks corrupting a hint value's own semantic content (e.g. a path) before it's been read for a non-display purpose. Truncating specifically the fields destined for `data.summary`/`data.body`/`data.hints` (post their non-truncated use elsewhere in the same function) is more precise.
- **Truncate `actions` (the `QStringList` of key/label pairs) too, for defense-in-depth beyond REQ-F-6.1's literal scope.** Considered, but left out of this design to stay strictly within REQ-F-6.1's named fields — flagged as a known risk below rather than silently expanded, since REQ-F-6.3's acceptance criterion explicitly greps for "summary, body, and all hint values" and would not catch a missed `actions` bound either way.

### Known risks

- `actions` (the alternating key/label `QStringList`) and `app_name`/`app_icon` remain unbounded after this fix. REQ-F-6.1 explicitly scopes the bounded fields to "summary, body, each hint value" — `actions` is technically also attacker-controlled D-Bus input and not mentioned, so this is a narrower fix than the full attack surface, deliberately scoped to match the letter of the requirement. If a future audit wants `actions`/`app_name`/`app_icon` bounded too, `truncateToMaxLength()` is directly reusable.
- Truncating a hint value mid-UTF-16-surrogate-pair (via `QString::left()`) is a theoretical corruption risk for exotic Unicode input — `QString::left()` operates on UTF-16 code units, not grapheme clusters, so a truncation boundary landing inside a surrogate pair or combining-character sequence could produce a slightly malformed tail character right before the truncation marker. Low practical impact (cosmetic only, and the marker text makes clear truncation occurred) — not fixed in this design; flag if this needs to be exact-grapheme-safe on a later pass.

### Test plan

- New file: `tests/test_notification_payload_bounds.cpp`, added to `test_holonight_services` (`tests/CMakeLists.txt:76-108`, which already builds `test_notification_service.cpp`/`test_notification_filter.cpp`/etc. from the same `notifications/` subsystem).
  - Top-of-file comment: purpose = REQ-F-6.1/6.2/6.3 regression coverage for notification payload length bounding.
  - Case: call `NotificationServer::Notify()` (via a `NotificationServer` constructed with a real/test `NotificationService`) with `summary` = 10 MB of repeated `'A'`; assert the notification is still processed (`addOrReplace()` returns a valid nonzero id), and inspect the resulting `NotificationData`/model entry's `summary` — assert length `<= kMaxNotificationFieldLength + strlen(kTruncationMarker)` and that it ends with `"...[truncated]"`.
  - Repeat for `body` (10 MB) and a hint value (e.g. `hints["x-custom"] = QString(10*1024*1024, 'B')`) — assert the same bound and marker on each.
  - Case: a hint value under the 4 KiB limit is unchanged (no spurious truncation of small legitimate payloads).
  - Case: a non-string hint value (e.g. `urgency` as `uchar`) passes through unmodified (no crash/type-mismatch from attempting to truncate a non-string).
- Register `test_notification_payload_bounds.cpp` in `tests/CMakeLists.txt` under `test_holonight_services`'s source list (`tests/CMakeLists.txt:76-108`).
- Run `task configure-tests` then `task test`.

---

## Implementation Order

All 6 items are, as SPEC.md states, largely independent — verified true during code exploration: **no shared file is touched by two different items**, and no item's fix depends on another item's fix being landed first. The one soft coupling worth flagging:

- **Item 2 (CalDAV `HttpSyncClient`) and Item 1/5/6 all touch qCWarning-based diagnostics**, but purely by convention, not by shared code — no actual dependency.
- **Item 4 requires a Step 0 empirical check before any code is written** — this should be done *first*, since its outcome (bug already gone vs. still reproducing) determines whether Item 4 is "add one test file" (small) or "investigate cross-repo packaging" (potentially open-ended, may need to be re-scoped/escalated rather than fixed inline).

Recommended sequencing, by risk/size and to front-load the finding that most changes scope:

1. **Item 4, Step 0 only** (empirical re-verification, no code) — do this first since it determines whether Item 4 is trivial or needs re-scoping; costs minutes, unblocks planning for the rest of the cycle.
2. **Item 3 (ConfigWriter weather)** — smallest, most contained diff (9 lines in one function), zero new classes, clearest rollback story if anything goes wrong. Good first *code* change to land and build confidence in the cycle.
3. **Item 1 (tray pixmap overflow)** — second-smallest, one new pure function plus a signature change with a safe default parameter; highest severity (Critical), so worth landing early even though not the very first.
4. **Item 6 (notification payload bounds)** — similar size/shape to Item 1 (one new helper function, localized call-site changes), same "Critical-adjacent, contained diff" profile.
5. **Item 5 (control-socket sidebar DoS)** — slightly more test-infrastructure work (new `test_sidebar_manager.cpp` may need a `LayerShell` test double that may not exist yet — verify before starting), but the production fix itself is a small reordering.
6. **Item 2 (CalDAV `HttpSyncClient` extraction)** — largest diff of the six (new class + two providers' internals rewritten to consume `std::expected` instead of raw `QNetworkReply*`, plus the trickiest test to write — a fake network reply/server). Land last so the cycle's simpler items are already merged and the team has full attention for the one item genuinely deserving Critical-severity care.
7. **Item 4, Steps 1-2** (if Step 0 found a real defect: investigate packaging; regardless: add the `FakeQmlServices.h`/`tst_holonight_theme_singleton.qml` regression test) — do last since it's the smallest possible remaining task and has no dependency on anything else.

---

## Files Touched Summary

**Item 1 — Tray pixmap overflow:**
- Modify: `libs/holonight-surfaces/src/TrayItem.h` (add `kMaxTrayPixmapDim`, `PixmapRejectReason`, `validateTrayPixmapDimensions()` declaration; update `decodePixmapList()` signature)
- Modify: `libs/holonight-surfaces/src/TrayItem.cpp` (implement `validateTrayPixmapDimensions()`; rewrite `decodePixmapList()`'s validation block)
- Modify: `libs/holonight-surfaces/src/TrayItemProperties.cpp` (pass `service` into both `decodePixmapList()` call sites, lines 23/28)
- Create: `tests/test_tray_pixmap_validation.cpp`
- Modify: `tests/CMakeLists.txt` (add new test file to `test_holonight_surfaces`)

**Item 2 — CalDAV timeout + HttpSyncClient:**
- Create: `libs/holonight-services/src/calendar/HttpSyncClient.h`
- Create: `libs/holonight-services/src/calendar/HttpSyncClient.cpp`
- Modify: `libs/holonight-services/src/calendar/CalDavProvider.h` / `.cpp` (delete local `sendSync()`/`kHttpTimeoutMs`; add `HttpSyncClient` member; rewrite `resolvePrincipalUrl()`, `discoverCalendars()`, `fetchCalendarEvents()`, `testConnection()` to consume `std::expected`)
- Modify: `libs/holonight-services/src/calendar/IcsProvider.h` / `.cpp` (delete `httpGet()`'s body/local `kHttpTimeoutMs`; add `HttpSyncClient` member)
- Modify: `libs/holonight-services/CMakeLists.txt` (register new `HttpSyncClient.h/.cpp` sources)
- Create: `tests/test_http_sync_client.cpp`
- Modify: `tests/test_calendar_integration.cpp` (add sync-failure → `syncError`/`lastError` propagation case)
- Modify: `tests/CMakeLists.txt` (add new test file to `test_holonight_services`)

**Item 3 — ConfigWriter weather preservation:**
- Modify: `libs/holonight-config/src/ConfigWriter.cpp` (lines 168-177: conditional serialization of `latitude`/`longitude`/`city`)
- Create: `tests/test_configwriter_weather_preservation.cpp`
- Modify: `tests/CMakeLists.txt` (add new test file to `test_holonight_settings`)

**Item 4 — HolonightTheme singleton:**
- No production code change anticipated (pending Step 0 outcome; if Step 0 finds a real defect, scope of a packaging/version fix is outside this design's file list and should be re-scoped separately)
- Modify: `tests/FakeQmlServices.h` (add `FakeHolonightTheme` class + `qmlRegisterSingletonInstance` call)
- Create: `tests/qml/tst_holonight_theme_singleton.qml`

**Item 5 — Control-socket sidebar DoS:**
- Modify: `libs/holonight-surfaces/src/SidebarManager.h` (add `isKnownMonitor()` declaration)
- Modify: `libs/holonight-surfaces/src/SidebarManager.cpp` (implement `isKnownMonitor()`; add early-return guard to `toggle()`)
- Create: `tests/test_sidebar_manager.cpp`
- Modify: `tests/CMakeLists.txt` (add new test file to `test_holonight_surfaces`)

**Item 6 — Notification payload bounds:**
- Modify: `libs/holonight-services/src/notifications/NotificationServer.cpp` (add `kMaxNotificationFieldLength`, `kTruncationMarker`, `truncateToMaxLength()`; apply inside `buildNotificationData()`; reorder `Notify()` to log post-truncation values)
- Create: `tests/test_notification_payload_bounds.cpp`
- Modify: `tests/CMakeLists.txt` (add new test file to `test_holonight_services`)

**Cross-cutting:**
- `tests/CMakeLists.txt` receives 6 new `.cpp`/QML test-file registrations total across 3 existing targets (`test_holonight_surfaces` ×3, `test_holonight_services` ×2, `test_holonight_settings` ×1) plus the harness-registered QML test for Item 4 — no new test *executables* are created; all new tests join existing targets.
