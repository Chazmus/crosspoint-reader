#include <gtest/gtest.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

#ifndef REPO_ROOT
#define REPO_ROOT "."
#endif

std::string readFile(const std::string& path) {
  std::string fullPath = std::string(REPO_ROOT) + "/" + path;
  std::ifstream f(fullPath);
  if (!f.is_open()) {
    f.open(path);
  }
  std::stringstream ss;
  ss << f.rdbuf();
  return ss.str();
}

}  // namespace

class LuaHostTest : public ::testing::Test {
 protected:
  lua_State* L = nullptr;

  void SetUp() override {
    L = luaL_newstate();
    ASSERT_NE(L, nullptr);
    luaL_openlibs(L);
  }

  void TearDown() override {
    if (L) {
      lua_close(L);
      L = nullptr;
    }
  }
};

TEST_F(LuaHostTest, VMInitializationAnd32BitArchitecture) {
  // Verify 32-bit integer configuration matches ESP32-S3 hardware registers
  EXPECT_EQ(sizeof(lua_Integer), sizeof(int32_t));
  EXPECT_EQ(sizeof(lua_Number), sizeof(float));

  // Run basic arithmetic and verify evaluation
  const char* script = "result = 40 + 2";
  ASSERT_EQ(luaL_dostring(L, script), LUA_OK);

  lua_getglobal(L, "result");
  EXPECT_TRUE(lua_isinteger(L, -1));
  EXPECT_EQ(lua_tointeger(L, -1), 42);
  lua_pop(L, 1);
}

TEST_F(LuaHostTest, ProtectedCallCatchesSyntaxErrorSafely) {
  const char* invalidScript = "function foo( invalid syntax here !!!";
  const int status = luaL_loadbuffer(L, invalidScript, strlen(invalidScript), "invalid");
  EXPECT_EQ(status, LUA_ERRSYNTAX);

  const char* err = lua_tostring(L, -1);
  ASSERT_NE(err, nullptr);
  EXPECT_GT(strlen(err), 0u);
  lua_pop(L, 1);
}

TEST_F(LuaHostTest, ProtectedCallCatchesRuntimeErrorSafely) {
  const char* runtimeErrScript =
      "function crash() local t = nil; t.foo = 1 end\n"
      "crash()";

  const int status = luaL_dostring(L, runtimeErrScript);
  EXPECT_NE(status, LUA_OK);

  const char* err = lua_tostring(L, -1);
  ASSERT_NE(err, nullptr);
  EXPECT_NE(strstr(err, "attempt to index a nil value"), nullptr);
  lua_pop(L, 1);
}

TEST_F(LuaHostTest, SpriteBitmaskArithmetic) {
  // Test 8x8 sprite bitmap logic:
  // Ink bit 1 -> black (true)
  // Mask bit 1 (when ink is 0) -> white (false)
  // Both 0 -> transparent/untouched
  const uint8_t ink = 0b10000000;   // pixel 0 is ink (black)
  const uint8_t mask = 0b01000000;  // pixel 1 is mask (white cutout)

  bool pixel0 = false, pixel1 = false;

  // Pixel 0: ink
  if ((ink >> 7) & 1) pixel0 = true;
  EXPECT_TRUE(pixel0);

  // Pixel 1: mask
  if (!((ink >> 6) & 1) && ((mask >> 6) & 1)) pixel1 = false;
  EXPECT_FALSE(pixel1);

  // Pixel 2: neither
  const bool isInk2 = (ink >> 5) & 1;
  const bool isMask2 = (mask >> 5) & 1;
  EXPECT_FALSE(isInk2);
  EXPECT_FALSE(isMask2);
}

TEST_F(LuaHostTest, JsonParserDecodesManifest) {
  // Load our pure-Lua json parser
  const std::string jsonLuaSrc = readFile("sdcard/apps/chess/json.lua");
  ASSERT_FALSE(jsonLuaSrc.empty());

  ASSERT_EQ(luaL_loadbuffer(L, jsonLuaSrc.data(), jsonLuaSrc.size(), "json.lua"), LUA_OK);
  ASSERT_EQ(lua_pcall(L, 0, 1, 0), LUA_OK);
  lua_setglobal(L, "json");

  // Decode a manifest string
  const char* testManifest =
      "local str = '{\"id\":\"chess\",\"title\":\"Chess Puzzles\",\"rating\":1580,\"sleepScreen\":true}'\n"
      "manifest = json.decode(str)";

  ASSERT_EQ(luaL_dostring(L, testManifest), LUA_OK);

  lua_getglobal(L, "manifest");
  ASSERT_TRUE(lua_istable(L, -1));

  lua_getfield(L, -1, "id");
  EXPECT_STREQ(lua_tostring(L, -1), "chess");
  lua_pop(L, 1);

  lua_getfield(L, -1, "title");
  EXPECT_STREQ(lua_tostring(L, -1), "Chess Puzzles");
  lua_pop(L, 1);

  lua_getfield(L, -1, "rating");
  EXPECT_EQ(lua_tointeger(L, -1), 1580);
  lua_pop(L, 1);

  lua_getfield(L, -1, "sleepScreen");
  EXPECT_TRUE(lua_toboolean(L, -1));
  lua_pop(L, 1);

  lua_pop(L, 1);  // pop manifest
}

TEST_F(LuaHostTest, ChessNotationAndFenParsingInLua) {
  const char* chessScript = R"(
    local board = {}
    for i = 0, 63 do board[i] = "." end

    local function notationToSq(notStr)
      local f = string.byte(notStr, 1) - 97
      local r = string.byte(notStr, 2) - 49
      return r * 8 + f
    end

    local function sqToNotation(sq)
      local f = sq % 8
      local r = math.floor(sq / 8)
      return string.char(97 + f) .. string.char(49 + r)
    end

    -- e2 is file 4, rank 1 -> sq 12
    sq_e2 = notationToSq("e2")
    not_12 = sqToNotation(12)

    -- e4 is file 4, rank 3 -> sq 28
    sq_e4 = notationToSq("e4")
    not_28 = sqToNotation(28)
  )";

  ASSERT_EQ(luaL_dostring(L, chessScript), LUA_OK);

  lua_getglobal(L, "sq_e2");
  EXPECT_EQ(lua_tointeger(L, -1), 12);
  lua_pop(L, 1);

  lua_getglobal(L, "not_12");
  EXPECT_STREQ(lua_tostring(L, -1), "e2");
  lua_pop(L, 1);

  lua_getglobal(L, "sq_e4");
  EXPECT_EQ(lua_tointeger(L, -1), 28);
  lua_pop(L, 1);

  lua_getglobal(L, "not_28");
  EXPECT_STREQ(lua_tostring(L, -1), "e4");
  lua_pop(L, 1);
}

TEST_F(LuaHostTest, TracebackProvidesFileAndLineInformation) {
  static auto tracebackHandler = [](lua_State* state) -> int {
    const char* msg = lua_tostring(state, 1);
    luaL_traceback(state, state, msg, 1);
    return 1;
  };

  const char* script =
      "function inner() error('something went wrong') end\n"
      "function outer() inner() end\n";

  ASSERT_EQ(luaL_dostring(L, script), LUA_OK);

  const int errIdx = lua_gettop(L) + 1;
  lua_pushcfunction(L, tracebackHandler);
  lua_getglobal(L, "outer");

  const int status = lua_pcall(L, 0, 0, errIdx);
  EXPECT_NE(status, LUA_OK);

  const char* errWithTraceback = lua_tostring(L, -1);
  ASSERT_NE(errWithTraceback, nullptr);
  EXPECT_NE(strstr(errWithTraceback, "something went wrong"), nullptr);
  EXPECT_NE(strstr(errWithTraceback, "stack traceback:"), nullptr);
  EXPECT_NE(strstr(errWithTraceback, "in function 'inner'"), nullptr);
  EXPECT_NE(strstr(errWithTraceback, "in function 'outer'"), nullptr);
  lua_pop(L, 2);
}

TEST_F(LuaHostTest, PackageSearchersCanInjectCustomModuleLoader) {
  static auto customLoader = [](lua_State* state) -> int {
    const char* modName = luaL_checkstring(state, 1);
    if (strcmp(modName, "my_custom_mod") == 0) {
      const char* modSrc = "return { version = '2.0.0', answer = 42 }";
      luaL_loadbuffer(state, modSrc, strlen(modSrc), "my_custom_mod.lua");
      return 1;
    }
    lua_pushstring(state, "not found");
    return 1;
  };

  lua_getglobal(L, "package");
  ASSERT_TRUE(lua_istable(L, -1));
  lua_getfield(L, -1, "searchers");
  ASSERT_TRUE(lua_istable(L, -1));

  // Insert custom loader at index 2
  const int count = static_cast<int>(lua_rawlen(L, -1));
  for (int i = count; i >= 2; --i) {
    lua_rawgeti(L, -1, i);
    lua_rawseti(L, -2, i + 1);
  }
  lua_pushcfunction(L, customLoader);
  lua_rawseti(L, -2, 2);
  lua_pop(L, 2);

  // Now test require("my_custom_mod")
  const char* testScript =
      "local m = require('my_custom_mod')\n"
      "mod_ver = m.version\n"
      "mod_ans = m.answer\n";

  ASSERT_EQ(luaL_dostring(L, testScript), LUA_OK);

  lua_getglobal(L, "mod_ver");
  EXPECT_STREQ(lua_tostring(L, -1), "2.0.0");
  lua_pop(L, 1);

  lua_getglobal(L, "mod_ans");
  EXPECT_EQ(lua_tointeger(L, -1), 42);
  lua_pop(L, 1);
}
