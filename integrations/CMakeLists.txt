
foreach(integration IN ITEMS hyprland sway wayland labwc)
  add_subdirectory(${integration})
endforeach()
