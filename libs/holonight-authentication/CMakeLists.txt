add_library(holonight_authentication_core STATIC
  src/AuthenticationPromptModel.h
  src/AuthenticationPromptModel.cpp
  src/ExternalText.cpp
  src/IdentityListModel.h
  src/IdentityListModel.cpp
  src/MessageListModel.h
  src/MessageListModel.cpp
  src/ProtocolWriter.cpp
  src/PolkitRequestCoordinator.cpp
  src/PolkitStartupPolicy.cpp
  src/SecretValidator.cpp
)
target_include_directories(holonight_authentication_core PUBLIC src)
target_link_libraries(holonight_authentication_core PUBLIC Qt6::Core)
target_compile_features(holonight_authentication_core PUBLIC cxx_std_23)

# Export model metadata for the authentication QML foreign-type registrar.
qt6_extract_metatypes(holonight_authentication_core)
