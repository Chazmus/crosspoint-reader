set(LUA_SOURCES
  ${REPO_ROOT}/lib/lua/lapi.c
  ${REPO_ROOT}/lib/lua/lauxlib.c
  ${REPO_ROOT}/lib/lua/lbaselib.c
  ${REPO_ROOT}/lib/lua/lcode.c
  ${REPO_ROOT}/lib/lua/lcorolib.c
  ${REPO_ROOT}/lib/lua/lctype.c
  ${REPO_ROOT}/lib/lua/ldblib.c
  ${REPO_ROOT}/lib/lua/ldebug.c
  ${REPO_ROOT}/lib/lua/ldo.c
  ${REPO_ROOT}/lib/lua/ldump.c
  ${REPO_ROOT}/lib/lua/lfunc.c
  ${REPO_ROOT}/lib/lua/lgc.c
  ${REPO_ROOT}/lib/lua/linit.c
  ${REPO_ROOT}/lib/lua/liolib.c
  ${REPO_ROOT}/lib/lua/llex.c
  ${REPO_ROOT}/lib/lua/lmathlib.c
  ${REPO_ROOT}/lib/lua/lmem.c
  ${REPO_ROOT}/lib/lua/loadlib.c
  ${REPO_ROOT}/lib/lua/lobject.c
  ${REPO_ROOT}/lib/lua/lopcodes.c
  ${REPO_ROOT}/lib/lua/loslib.c
  ${REPO_ROOT}/lib/lua/lparser.c
  ${REPO_ROOT}/lib/lua/lstate.c
  ${REPO_ROOT}/lib/lua/lstring.c
  ${REPO_ROOT}/lib/lua/lstrlib.c
  ${REPO_ROOT}/lib/lua/ltable.c
  ${REPO_ROOT}/lib/lua/ltablib.c
  ${REPO_ROOT}/lib/lua/ltm.c
  ${REPO_ROOT}/lib/lua/lundump.c
  ${REPO_ROOT}/lib/lua/lutf8lib.c
  ${REPO_ROOT}/lib/lua/lvm.c
  ${REPO_ROOT}/lib/lua/lzio.c
)

add_executable(LuaHostTest
  LuaHostTest.cpp
  ${LUA_SOURCES}
)

target_include_directories(LuaHostTest PRIVATE
  ${REPO_ROOT}/lib/lua
  ${REPO_ROOT}/src
)

target_compile_definitions(LuaHostTest PRIVATE
  REPO_ROOT="${REPO_ROOT}"
)

target_link_libraries(LuaHostTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(LuaHostTest)
