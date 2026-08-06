import QtQuick
import QtTest
import HolonightShell

TestCase {
  id: root

  name: "NetworkCurrentCard"
  when: windowShown

  Component {
    id: cardComponent

    NetworkCurrentCard {
      width: 560
      height: 160
    }
  }

  function createCard() {
    const card = createTemporaryObject(cardComponent, root)
    verify(card)
    return card
  }

  function test_band_formatting() {
    const card = createCard()

    compare(card.formatBand(0), "Unavailable")
    compare(card.formatBand(2412), "2.4 GHz")
    compare(card.formatBand(5180), "5 GHz")
    compare(card.formatBand(6115), "6 GHz")
    compare(card.formatBand(7300), "7300 MHz")
  }

  function test_link_speed_formatting() {
    const card = createCard()

    compare(card.formatLinkSpeed(0), "Unavailable")
    compare(card.formatLinkSpeed(866), "866 Mbps")
  }

  function test_connected_subtitle_includes_band_and_vpn() {
    const card = createCard()
    const subtitle = findChild(card, "connectionSubtitle")

    verify(subtitle)
    compare(subtitle.text, "Connected · 5 GHz · VPN secured")
  }
}
