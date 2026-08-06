#include "TrayItemProperties.h"

#include <QDBusArgument>
#include <QLoggingCategory>
#include <QMetaType>
#include <QRegularExpression>

#include <optional>

Q_LOGGING_CATEGORY(lcTrayItemProperties, "holonight.tray.properties")

static QStringList sortedKeys(const QVariantMap& values) {
  QStringList keys = values.keys();
  keys.sort();
  return keys;
}

static std::optional<QImage> trayPixmapFromVariant(const QVariant& raw, const QString& service) {
  if (!raw.isValid()) {
    return std::nullopt;
  }
  if (raw.userType() == qMetaTypeId<SniIconPixmapList>()) {
    return decodePixmapList(raw.value<SniIconPixmapList>(), 22, service);
  }
  if (raw.canConvert<QDBusArgument>()) {
    SniIconPixmapList pixmaps;
    raw.value<QDBusArgument>() >> pixmaps;
    return decodePixmapList(pixmaps, 22, service);
  }
  return std::nullopt;
}

static bool textHasKnownUnreadCounter(const QString& text) {
  static const QRegularExpression teams_counter_re(
      QStringLiteral(R"(^\s*Microsoft Teams(?:\s+\([^)]*\))?\s+\(([1-9][0-9]*)\)\s*$)"));
  return teams_counter_re.match(text).hasMatch();
}

static bool textBelongsToKnownUnreadApp(const QString& text) {
  return text.trimmed().startsWith(QStringLiteral("Microsoft Teams"));
}

static bool textHasSlackUnreadCounter(const QString& text) {
  static const QRegularExpression slack_counter_re(
      QStringLiteral(R"(^\s*You have\s+([1-9][0-9]*)\s+notifications?\s*$)"),
      QRegularExpression::CaseInsensitiveOption);
  return slack_counter_re.match(text).hasMatch();
}

static bool itemBelongsToSlack(const TrayItem& item) {
  return item.id.startsWith(QStringLiteral("Slack_status_icon"), Qt::CaseInsensitive) ||
         item.title.trimmed().startsWith(QStringLiteral("Slack"), Qt::CaseInsensitive) ||
         item.tooltip.title.trimmed().startsWith(QStringLiteral("Slack"), Qt::CaseInsensitive);
}

static bool deriveUnreadState(const TrayItem& item) {
  const QString tooltip_title = item.tooltip.title.trimmed();
  if (textBelongsToKnownUnreadApp(tooltip_title)) {
    return textHasKnownUnreadCounter(tooltip_title);
  }

  const QString title = item.title.trimmed();
  if (textBelongsToKnownUnreadApp(title)) {
    return textHasKnownUnreadCounter(title);
  }

  if (itemBelongsToSlack(item)) {
    return textHasSlackUnreadCounter(tooltip_title) || textHasSlackUnreadCounter(item.tooltip.description) ||
           textHasSlackUnreadCounter(title);
  }

  return item.has_unread;
}

TrayItem mergeTrayItemProperties(  // NOLINT(readability-function-cognitive-complexity)
    const TrayItem* existing, const QString& service, const QString& path, const QVariantMap& changed) {
  TrayItem item = existing != nullptr ? *existing : TrayItem{};
  item.service = service;
  item.object_path = path;

  qCInfo(lcTrayItemProperties) << "merge properties for" << service << path << "existing" << (existing != nullptr)
                               << "changed keys" << sortedKeys(changed);
  const bool was_unread = item.has_unread;

  if (changed.contains(QStringLiteral("IconName"))) {
    item.icon_name = changed.value(QStringLiteral("IconName")).toString();
    qCInfo(lcTrayItemProperties) << "IconName" << item.icon_name << "for" << service << path;
  }
  if (changed.contains(QStringLiteral("Id"))) {
    item.id = changed.value(QStringLiteral("Id")).toString();
    qCInfo(lcTrayItemProperties) << "Id" << item.id << "for" << service << path;
  }
  if (changed.contains(QStringLiteral("AttentionIconName"))) {
    item.attention_icon_name = changed.value(QStringLiteral("AttentionIconName")).toString();
    qCInfo(lcTrayItemProperties) << "AttentionIconName" << item.attention_icon_name << "for" << service << path;
  }
  if (changed.contains(QStringLiteral("Status"))) {
    item.status = changed.value(QStringLiteral("Status")).toString();
    qCInfo(lcTrayItemProperties) << "Status" << item.status << "for" << service << path;
  }
  if (item.status.isEmpty()) {
    item.status = QStringLiteral("Active");
    qCInfo(lcTrayItemProperties) << "Status missing; defaulting to Active for" << service << path;
  }
  if (changed.contains(QStringLiteral("Title"))) {
    item.title = changed.value(QStringLiteral("Title")).toString();
    qCInfo(lcTrayItemProperties) << "Title" << item.title << "for" << service << path;
  }

  if (changed.contains(QStringLiteral("IconPixmap"))) {
    const QVariant raw = changed.value(QStringLiteral("IconPixmap"));
    if (std::optional<QImage> pixmap = trayPixmapFromVariant(raw, service); pixmap.has_value()) {
      if (item.has_unread) {
        if (!item.clean_icon_pixmap_cache.isNull()) {
          item.icon_pixmap_cache = item.clean_icon_pixmap_cache;
        } else {
          item.icon_pixmap_cache = *pixmap;
        }
      } else {
        item.previous_clean_icon_pixmap_cache =
            item.clean_icon_pixmap_cache.isNull() ? item.icon_pixmap_cache : item.clean_icon_pixmap_cache;
        item.clean_icon_pixmap_cache = *pixmap;
        item.icon_pixmap_cache = *pixmap;
      }
      qCInfo(lcTrayItemProperties) << "IconPixmap decoded for" << service << path << "result"
                                   << item.icon_pixmap_cache.size() << "isNull" << item.icon_pixmap_cache.isNull();
    } else {
      qCWarning(lcTrayItemProperties) << "IconPixmap present but not convertible to QDBusArgument for" << service
                                      << path << "type" << raw.typeName();
    }
  }

  if (changed.contains(QStringLiteral("ToolTip"))) {
    const QVariant raw = changed.value(QStringLiteral("ToolTip"));
    SniToolTip tip;
    bool parsed = false;
    if (raw.userType() == qMetaTypeId<SniToolTip>()) {
      tip = raw.value<SniToolTip>();
      parsed = true;
    } else if (raw.canConvert<QDBusArgument>()) {
      const auto& arg = raw.value<QDBusArgument>();
      if (arg.currentType() == QDBusArgument::StructureType) {
        arg >> tip;
        parsed = true;
      }
    }
    if (parsed) {
      item.tooltip = tip;
      qCInfo(lcTrayItemProperties) << "ToolTip parsed for" << service << path << "title" << tip.title << "description"
                                   << tip.description << "iconName" << tip.icon_name;
    } else {
      qCWarning(lcTrayItemProperties) << "ToolTip present but not parseable for" << service << path << "type"
                                      << raw.typeName();
    }
  }

  item.has_unread = deriveUnreadState(item);
  if (!was_unread && item.has_unread && !item.previous_clean_icon_pixmap_cache.isNull()) {
    item.clean_icon_pixmap_cache = item.previous_clean_icon_pixmap_cache;
    item.icon_pixmap_cache = item.previous_clean_icon_pixmap_cache;
    qCInfo(lcTrayItemProperties) << "Unread counter detected; restored clean icon for" << service << path
                                 << item.icon_pixmap_cache.size();
  }

  if (changed.contains(QStringLiteral("AttentionIconPixmap"))) {
    const QVariant raw = changed.value(QStringLiteral("AttentionIconPixmap"));
    if (std::optional<QImage> pixmap = trayPixmapFromVariant(raw, service); pixmap.has_value()) {
      item.attention_pixmap_cache = *pixmap;
      qCInfo(lcTrayItemProperties) << "AttentionIconPixmap decoded for" << service << path << "result"
                                   << item.attention_pixmap_cache.size() << "isNull"
                                   << item.attention_pixmap_cache.isNull();
    } else {
      qCWarning(lcTrayItemProperties) << "AttentionIconPixmap present but not convertible to QDBusArgument for"
                                      << service << path << "type" << raw.typeName();
    }
  }

  qCInfo(lcTrayItemProperties) << "merged tray item" << service << path << "id" << item.id << "title" << item.title
                               << "status" << item.status << "iconName" << item.icon_name << "attentionIconName"
                               << item.attention_icon_name << "iconPixmap" << item.icon_pixmap_cache.size()
                               << "attentionPixmap" << item.attention_pixmap_cache.size() << "hasUnread"
                               << item.has_unread;
  return item;
}
