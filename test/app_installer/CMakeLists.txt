set(ARDUINOJSON_SRC_DIR "${REPO_ROOT}/.pio/libdeps/x4pro/ArduinoJson/src")
if(NOT EXISTS "${ARDUINOJSON_SRC_DIR}/ArduinoJson.h")
  include(FetchContent)
  FetchContent_Declare(
    ArduinoJson
    GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
    GIT_TAG v7.4.2
  )
  FetchContent_MakeAvailable(ArduinoJson)
  set(ARDUINOJSON_SRC_DIR "${arduinojson_SOURCE_DIR}/src")
endif()

add_executable(AppCatalogTest
  AppCatalogTest.cpp
  ${REPO_ROOT}/src/apps/installer/AppCatalog.cpp
  ${REPO_ROOT}/src/apps/installer/AppSourceStore.cpp
  stubs/PersistableStoreStub.cpp
)

target_include_directories(AppCatalogTest PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/stubs
  ${REPO_ROOT}/src
  ${REPO_ROOT}/lib/Serialization
  ${ARDUINOJSON_SRC_DIR}
)

target_link_libraries(AppCatalogTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(AppCatalogTest)
