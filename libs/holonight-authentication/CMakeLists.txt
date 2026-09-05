add_library(holonight_authentication_core STATIC
  src/AuthenticationPromptModel.cpp
  src/ExternalText.cpp
  src/IdentityListModel.cpp
  src/MessageListModel.cpp
  src/ProtocolWriter.cpp
  src/PolkitRequestCoordinator.cpp
  src/PolkitStartupPolicy.cpp
  src/SecretValidator.cpp
)
target_include_directories(holonight_authentication_core PUBLIC src)
target_link_libraries(holonight_authentication_core PUBLIC Qt6::Core)
target_compile_features(holonight_authentication_core PUBLIC cxx_std_23)
