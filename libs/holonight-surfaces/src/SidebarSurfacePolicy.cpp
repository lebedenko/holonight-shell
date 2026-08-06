#include "SidebarSurfacePolicy.h"

#include "ShellConstants.h"

#include <algorithm>

int sidebarDefaultHeight() { return kSidebarDefaultHeight; }

int boundedSidebarHeight(int requested_height, int screen_height) {
  const int max_height = screen_height <= 0
                             ? kSidebarDefaultHeight
                             : std::max(kSidebarMinHeight, screen_height - kSidebarTopMargin - kSidebarBottomMargin);
  return std::clamp(requested_height, kSidebarMinHeight, max_height);
}
