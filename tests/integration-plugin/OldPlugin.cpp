#include <QObject>
#include <QtPlugin>

// Metadata discovery must reject the previous ABI before constructing this object.
class OldPlugin final : public QObject {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID "org.holonight.Integration/2.0")
};
#include "OldPlugin.moc"
