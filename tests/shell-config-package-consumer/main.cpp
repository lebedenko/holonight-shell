#include <holonight_shell_config/config_parsers.h>

int main() {
  const HoloNight::ShellConfig::ProductConfig config;
  return config.bar_workspaces.count > 0 ? 0 : 1;
}
