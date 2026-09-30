foreach(integration IN ITEMS hyprland sway wayland)
  add_subdirectory(${integration})
endforeach()
