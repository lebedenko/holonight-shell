#include <QDir>
#include <QImage>
#include <QSet>

#include <gtest/gtest.h>
TEST(WeatherAssets, LoadsExactApprovedInventory) {
  const QStringList expected{"clear-night.png",
                             "cloudy.png",
                             "drizzle.png",
                             "dust-sand.png",
                             "fog.png",
                             "fog-day.png",
                             "fog-night.png",
                             "freezing-drizzle.png",
                             "freezing-rain.png",
                             "heavy-rain.png",
                             "heavy-sleet.png",
                             "heavy-snow.png",
                             "moon-first-quarter.png",
                             "moon-full.png",
                             "moon-last-quarter.png",
                             "moon-new.png",
                             "moon-waning-crescent.png",
                             "moon-waning-gibbous.png",
                             "moon-waxing-crescent.png",
                             "moon-waxing-gibbous.png",
                             "mostly-clear-night.png",
                             "mostly-cloudy.png",
                             "mostly-cloudy-night.png",
                             "mostly-sunny.png",
                             "overcast.png",
                             "partly-cloudy.png",
                             "partly-cloudy-night.png",
                             "rain-showers.png",
                             "rain-snow-mix.png",
                             "rainy.png",
                             "severe-thunderstorm.png",
                             "showers-day.png",
                             "showers-night.png",
                             "sleet.png",
                             "smoke.png",
                             "snow.png",
                             "snow-showers.png",
                             "snow-showers-day.png",
                             "snow-showers-night.png",
                             "sunny.png",
                             "sunrise.png",
                             "sunset.png",
                             "thunderstorm.png",
                             "thunderstorm-day.png",
                             "thunderstorm-hail.png",
                             "thunderstorm-night.png",
                             "tornado.png",
                             "unavailable.png",
                             "volcanic-ash.png",
                             "windy.png"};
  const auto actual = QDir(":/HolonightShell/weather").entryList({"*.png"}, QDir::Files, QDir::Name);
  EXPECT_EQ(QSet<QString>(actual.begin(), actual.end()), QSet<QString>(expected.begin(), expected.end()));
  ASSERT_EQ(actual.size(), 50);
  for (const auto& name : expected) {
    const QImage image(":/HolonightShell/weather/" + name);
    EXPECT_FALSE(image.isNull()) << name.toStdString();
    EXPECT_EQ(image.size(), QSize(1024, 1024));
    EXPECT_TRUE(image.hasAlphaChannel());
  }
  EXPECT_FALSE(QDir(":/HolonightShell/weather-png").exists());
}
