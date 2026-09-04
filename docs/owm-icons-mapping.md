# OpenWeather condition icon mapping

`WeatherService::iconPath(int condition_id, bool is_day)` selects one of the 18 first-party SVGs in
`assets/weather/`. Day and night variants use the `d` and `n` suffixes respectively.

| OpenWeather condition | Icon family |
|---|---|
| `2xx` | `11d/n` |
| `3xx` | `09d/n` |
| `500–504` | `10d/n` |
| `511` | `13d/n` |
| `520–531` | `09d/n` |
| `6xx` | `13d/n` |
| `7xx` | `50d/n` |
| `800` | `01d/n` |
| `801` | `02d/n` |
| `802` | `03d/n` |
| `803–804` | `04d/n` |
| Unknown | `03d/n` |

Daily forecast cards continue to request the day variant. The PNG forecast compositor and moon-phase assets are
independent of this mapping.
