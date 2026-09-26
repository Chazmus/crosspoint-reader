#pragma once

#include <vector>

#include "apps/AppDescriptor.h"

namespace lua_host {

std::vector<AppDescriptor> scanSdApps();

}  // namespace lua_host
