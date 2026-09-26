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
  ${REPO_ROOT}/.pio/libdeps/x4pro/ArduinoJson/src
)

target_link_libraries(AppCatalogTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(AppCatalogTest)
