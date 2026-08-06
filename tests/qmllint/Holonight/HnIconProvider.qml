pragma Singleton
import QtQuick

QtObject {
    function sourceUrl(source: url, size: int, color: color, highlight: color, positive: color, neutral: color,
                       negative: color, palette_revision: int): string {
        return ""
    }

    function supportsSemanticColors(source: url): bool {
        return false
    }
}
