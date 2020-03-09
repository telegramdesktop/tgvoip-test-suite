#include "utils.h"

#include <iostream>
#include <fstream>
#include <streambuf>

namespace tgvoiprate {

void throwIf(bool condition, const std::string& errorMessage)
{
    if (condition)
    {
        throw std::runtime_error(errorMessage);
    }
}

}
