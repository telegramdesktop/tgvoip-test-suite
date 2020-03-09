#pragma once

#include <string>
#include <condition_variable>

namespace tgvoiprate {

void throwIf(bool condition, const std::string& errorMessage);

}
