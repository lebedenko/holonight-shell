# OWM Condition ID → Icon Mapping

**Source**: `WeatherService::iconPath()` in `src/services/weather/WeatherService.cpp`  
**Icon assets**: `assets/weather/wsymbol_*.svg` (bundled at `qrc:/HolonightShell/weather/`)  
**Call sites**: `WeatherWidget.qml`, `WeatherCurrentSection.qml`, `WeatherHourlyStrip.qml`, `WeatherDailyCards.qml`

The function signature is `iconPath(int condition_id, bool is_day)`. Each table entry holds `{day icon, night icon}`. Unknown IDs fall back to `wsymbol_0999_unknown.svg`.

Note: daily forecast cards always pass `is_day = true` — they always render the day variant regardless of time.

---

## Thunderstorm (2xx)

| OWM IDs | Condition | Day Icon | Night Icon |
|---------|-----------|----------|------------|
| 200, 201, 202, 230, 231, 232 | Thunderstorm with rain/drizzle | `wsymbol_0016_thundery_showers` | `wsymbol_0032_thundery_showers_night` |
| 210, 211, 212, 221 | Thunderstorm (no rain) | `wsymbol_0024_thunderstorms` | `wsymbol_0040_thunderstorms_night` |

---

## Drizzle (3xx)

| OWM IDs | Condition | Day Icon | Night Icon |
|---------|-----------|----------|------------|
| 300, 301, 302, 310, 311 | Light/moderate drizzle | `wsymbol_0048_drizzle` | `wsymbol_0066_drizzle_night` |
| 312 | Heavy drizzle | `wsymbol_0081_heavy_drizzle` | `wsymbol_0082_heavy_drizzle_night` |
| 313, 314, 321 | Drizzle showers | `wsymbol_0009_light_rain_showers` | `wsymbol_0025_light_rain_showers_night` |

---

## Rain (5xx)

| OWM IDs | Condition | Day Icon | Night Icon |
|---------|-----------|----------|------------|
| 500, 501 | Light/moderate rain | `wsymbol_0017_cloudy_with_light_rain` | `wsymbol_0033_cloudy_with_light_rain_night` |
| 502, 503 | Heavy rain | `wsymbol_0018_cloudy_with_heavy_rain` | `wsymbol_0034_cloudy_with_heavy_rain_night` |
| 504 | Extreme rain | `wsymbol_0051_extreme_rain` | `wsymbol_0069_extreme_rain_night` |
| 511 | Freezing rain | `wsymbol_0050_freezing_rain` | `wsymbol_0068_freezing_rain_night` |
| 520, 521 | Light shower rain | `wsymbol_0009_light_rain_showers` | `wsymbol_0025_light_rain_showers_night` |
| 522 | Heavy shower rain | `wsymbol_0010_heavy_rain_showers` | `wsymbol_0026_heavy_rain_showers_night` |
| 531 | Extreme shower rain | `wsymbol_0085_extreme_rain_showers` | `wsymbol_0086_extreme_rain_showers_night` |

---

## Snow (6xx)

| OWM IDs | Condition | Day Icon | Night Icon |
|---------|-----------|----------|------------|
| 600, 601 | Light/moderate snow | `wsymbol_0019_cloudy_with_light_snow` | `wsymbol_0035_cloudy_with_light_snow_night` |
| 602 | Heavy snow | `wsymbol_0020_cloudy_with_heavy_snow` | `wsymbol_0036_cloudy_with_heavy_snow_night` |
| 611, 612, 615, 616 | Sleet | `wsymbol_0021_cloudy_with_sleet` | `wsymbol_0037_cloudy_with_sleet_night` |
| 613 | Heavy sleet showers | `wsymbol_0087_heavy_sleet_showers` | `wsymbol_0088_heavy_sleet_showers_night` |
| 620 | Light snow showers | `wsymbol_0011_light_snow_showers` | `wsymbol_0027_light_snow_showers_night` |
| 621 | Heavy snow showers | `wsymbol_0012_heavy_snow_showers` | `wsymbol_0028_heavy_snow_showers_night` |
| 622 | Extreme snow | `wsymbol_0052_extreme_snow` | `wsymbol_0070_extreme_snow_night` |

---

## Atmosphere (7xx)

| OWM IDs | Condition | Day Icon | Night Icon |
|---------|-----------|----------|------------|
| 701 | Mist | `wsymbol_0006_mist` | `wsymbol_0063_mist_night` |
| 711 | Smoke | `wsymbol_0055_smoke` | `wsymbol_0073_smoke_night` |
| 721 | Haze | `wsymbol_0005_hazy_sun` | `wsymbol_0063_mist_night` ¹ |
| 731, 751, 761 | Dust/sand | `wsymbol_0056_dust_sand` | `wsymbol_0074_dust_sand_night` |
| 741 | Fog | `wsymbol_0007_fog` | `wsymbol_0064_fog_night` |
| 762 | Volcanic ash | `wsymbol_0091_volcanic_ash` | `wsymbol_0091_volcanic_ash` (same) |
| 771 | Squalls / wind | `wsymbol_0060_windy` | `wsymbol_0078_windy_night` |
| 781 | Tornado | `wsymbol_0079_tornado` | `wsymbol_0079_tornado` (same) |

¹ Haze night reuses the mist-night icon — no dedicated hazy-night asset exists.

---

## Clear / Clouds (8xx)

| OWM IDs | Condition | Day Icon | Night Icon |
|---------|-----------|----------|------------|
| 800 | Clear sky | `wsymbol_0001_sunny` | `wsymbol_0008_clear_sky_night` |
| 801 | Few clouds (11–25%) | `wsymbol_0002_sunny_intervals` | `wsymbol_0041_partly_cloudy_night` |
| 802 | Scattered clouds (25–50%) | `wsymbol_0003_white_cloud` | `wsymbol_0042_cloudy_night` |
| 803 | Broken clouds (51–84%) | `wsymbol_0043_mostly_cloudy` | `wsymbol_0044_mostly_cloudy_night` |
| 804 | Overcast (85–100%) | `wsymbol_0004_black_low_cloud` | `wsymbol_0004_black_low_cloud` (same) |

---

## Unmapped OWM codes

These OWM 2.5 condition IDs have no entry in the table and will render `wsymbol_0999_unknown`:

- **6xx**: 614 (light shower sleet) — not mapped
- **7xx**: 745 (freezing fog — OWM uses 741 for all fog variants)
- **9xx**: special/legacy codes (900–906); tornado is already covered by 781
