function(holonight_shell_qml target)
  set(module_directory "${CMAKE_CURRENT_BINARY_DIR}/HolonightShell")
  if(NOT target STREQUAL "holonight-shell")
    set(module_directory "${CMAKE_CURRENT_BINARY_DIR}/${target}-qml/HolonightShell")
  endif()
  file(GLOB_RECURSE HOLONIGHT_QML_FILES
      LIST_DIRECTORIES false
      CONFIGURE_DEPENDS
      "${PROJECT_SOURCE_DIR}/apps/shell/qml/*.qml"
  )
  list(SORT HOLONIGHT_QML_FILES)

  set(HOLONIGHT_QML_TEST_ENTRIES "")
  foreach(qml_file IN LISTS HOLONIGHT_QML_FILES)
    get_filename_component(qml_type "${qml_file}" NAME_WE)
    file(RELATIVE_PATH qml_relative "${PROJECT_SOURCE_DIR}/apps/shell" "${qml_file}")
    string(APPEND HOLONIGHT_QML_TEST_ENTRIES
        "    QStringLiteral(\"${qml_type} 1.0 %1\").arg(sourceUrl(QStringLiteral(\"apps/shell/${qml_relative}\"))),\n")
  endforeach()
  configure_file(
    ${PROJECT_SOURCE_DIR}/tests/GeneratedQmlFiles.h.in
    ${PROJECT_BINARY_DIR}/GeneratedQmlFiles.h
    @ONLY
  )

  foreach(qml_file IN LISTS HOLONIGHT_QML_FILES)
    cmake_path(ABSOLUTE_PATH qml_file BASE_DIRECTORY "${PROJECT_SOURCE_DIR}/apps/shell" NORMALIZE OUTPUT_VARIABLE qml_absolute)
    cmake_path(RELATIVE_PATH qml_absolute BASE_DIRECTORY "${PROJECT_SOURCE_DIR}/apps/shell/qml" OUTPUT_VARIABLE qml_alias)
    set_source_files_properties(${qml_file} PROPERTIES QT_RESOURCE_ALIAS "${qml_alias}")
  endforeach()

  qt6_add_qml_module(${target}
      OUTPUT_DIRECTORY "${module_directory}"
      URI HolonightShell
      VERSION 1.0
      RESOURCE_PREFIX "/"
      TYPEINFO holonight-shell.qmltypes
      NO_GENERATE_QMLTYPES
      NO_IMPORT_SCAN
      QML_FILES ${HOLONIGHT_QML_FILES}
  )

  # Plain JS files (pragma-library helpers imported via relative "import \"foo.js\"" — not
  # versioned QML component types) must NOT go through QML_FILES above: qt6_add_qml_module treats
  # every QML_FILES entry as an importable module type and auto-adds a "Name 1.0 path.js" line to
  # the generated qmldir, which makes this Qt build report the whole HolonightShell module as
  # ambiguous ("Found in qrc:/HolonightShell/ and in <empty>") and fail to load ANY type from it.
  # Bundle them as plain resources instead, at the same qrc alias the module's .qml files use, so
  # relative imports between the two still resolve.
  file(GLOB_RECURSE HOLONIGHT_QML_JS_FILES
      LIST_DIRECTORIES false
      CONFIGURE_DEPENDS
      "${PROJECT_SOURCE_DIR}/apps/shell/qml/*.js"
  )
  qt6_add_resources(${target} "qml_js_files"
      PREFIX "/HolonightShell"
      BASE "${PROJECT_SOURCE_DIR}/apps/shell/qml"
      FILES ${HOLONIGHT_QML_JS_FILES}
  )

  file(GLOB WEATHER_SVG_FILES
      LIST_DIRECTORIES false
      CONFIGURE_DEPENDS
      "${PROJECT_SOURCE_DIR}/assets/weather/*.svg"
  )
  qt6_add_resources(${target} "weather_icons"
      PREFIX "/HolonightShell"
      BASE "${PROJECT_SOURCE_DIR}/assets"
      FILES ${WEATHER_SVG_FILES}
  )

  file(GLOB WEATHER_PNG_FILES
      LIST_DIRECTORIES false
      CONFIGURE_DEPENDS
      "${PROJECT_SOURCE_DIR}/assets/weather-png/512x512/*.png"
  )
  qt6_add_resources(${target} "weather_png_icons"
      PREFIX "/HolonightShell"
      BASE "${PROJECT_SOURCE_DIR}/assets"
      FILES ${WEATHER_PNG_FILES}
  )

  file(GLOB WEATHER_UI_FILES
      LIST_DIRECTORIES false
      CONFIGURE_DEPENDS
      "${PROJECT_SOURCE_DIR}/assets/weather-ui/*.svg"
  )
  qt6_add_resources(${target} "weather_ui_assets"
      PREFIX "/HolonightShell"
      BASE "${PROJECT_SOURCE_DIR}/assets"
      FILES ${WEATHER_UI_FILES}
  )

  file(GLOB BAR_ICON_FILES
      LIST_DIRECTORIES false
      CONFIGURE_DEPENDS
      "${PROJECT_SOURCE_DIR}/assets/bar-icons/*.svg"
  )
  qt6_add_resources(${target} "bar_icons"
      PREFIX "/HolonightShell"
      BASE "${PROJECT_SOURCE_DIR}/assets"
      FILES ${BAR_ICON_FILES}
  )

  file(GLOB COMMON_ICON_FILES
      LIST_DIRECTORIES false
      CONFIGURE_DEPENDS
      "${PROJECT_SOURCE_DIR}/assets/common/*.svg"
  )
  qt6_add_resources(${target} "common_icons"
      PREFIX "/HolonightShell"
      BASE "${PROJECT_SOURCE_DIR}/assets"
      FILES ${COMMON_ICON_FILES}
  )

  file(GLOB LINUX_LOGO_FILES
      LIST_DIRECTORIES false
      CONFIGURE_DEPENDS
      "${PROJECT_SOURCE_DIR}/assets/linux-logo/*.svg"
  )
  qt6_add_resources(${target} "linux_logo_icons"
      PREFIX "/HolonightShell"
      BASE "${PROJECT_SOURCE_DIR}/assets"
      FILES ${LINUX_LOGO_FILES}
  )

  qt6_add_resources(${target} "logo_resource"
      PREFIX "/HolonightShell"
      BASE "${PROJECT_SOURCE_DIR}/assets"
      FILES
          "${PROJECT_SOURCE_DIR}/assets/holonight-logo.svg"
          "${PROJECT_SOURCE_DIR}/assets/holonight-shell.svg"
          "${PROJECT_SOURCE_DIR}/assets/logo.png"
  )

  qt6_add_resources(${target} "media_assets"
      PREFIX "/HolonightShell"
      BASE "${PROJECT_SOURCE_DIR}/assets"
      FILES
          "${PROJECT_SOURCE_DIR}/assets/media/artwork-fallback.svg"
          "${PROJECT_SOURCE_DIR}/assets/media/artwork-frame.svg"
          "${PROJECT_SOURCE_DIR}/assets/media/media-placeholder.svg"
          "${PROJECT_SOURCE_DIR}/assets/media/now-playing-glyph.svg"
  )

endfunction()
