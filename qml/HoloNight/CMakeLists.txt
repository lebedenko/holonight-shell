qt6_add_library(holonight_components STATIC)

set(HOLONIGHT_COMPONENT_QML_FILES
    Components/ExternalIcon.qml
)

foreach(qml_file IN LISTS HOLONIGHT_COMPONENT_QML_FILES)
    get_filename_component(qml_alias "${qml_file}" NAME)
    set_source_files_properties("${qml_file}" PROPERTIES QT_RESOURCE_ALIAS "${qml_alias}")
endforeach()

qt6_add_qml_module(holonight_components
    URI Holonight.Components
    VERSION 1.0
    RESOURCE_PREFIX "/qt-project.org/imports"
    NO_IMPORT_SCAN
    OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Holonight/Components"
    QML_FILES ${HOLONIGHT_COMPONENT_QML_FILES}
)

target_link_libraries(holonight_components PUBLIC
    Qt6::Quick
)
