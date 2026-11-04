# Weather condition artwork

The shared provider-aware resolver and selection policy are documented in
[Weather artwork and troubleshooting browser](sdd/weather-artwork-browser.md).
`WeatherIconMapper::catalogue()` lists each supported provider-native code, description,
and neutral/day artwork. `WeatherIconMapper::icon()` applies daylight availability.

Popup surfaces use `WeatherIcon` and preserve the artwork colors. No wind-speed or
English-description overrides are applied.

The topbar retains its existing SVG component and legacy condition mapping through `WeatherService.iconPath()`.
