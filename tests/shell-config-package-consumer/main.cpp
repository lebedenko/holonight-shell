#include <holonight_shell_config/config_schema.h>

int main() {
  const auto snapshot = HoloNight::Config::parseDocument("[bar.workspaces]\ncount = 7\n");
  if (!snapshot) {
    return 1;
  }
  const auto config = HoloNight::ShellConfig::decodeDocument(*snapshot.value);
  return config && config.value->bar_workspaces.count == 7 && !HoloNight::ShellConfig::settingMetadata().empty() ? 0
                                                                                                                 : 1;
}
