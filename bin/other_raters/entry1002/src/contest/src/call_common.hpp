#include <iomanip>
#include <sstream>
#include <type_traits>

namespace tgvoipcontest {

enum class CallMode : char {
    CALLER = 0, CALLEE = 1
};


template <class T>
std::enable_if_t<
    std::disjunction_v<std::is_same<T, uint8_t>, std::is_same<T, int8_t>, std::is_same<T, char>>,
    std::vector<T>
>
from_hex(const std::string& s) {
    std::vector<T> res;
    res.reserve(s.size() / 2);

    for (size_t i = 0; i < s.size(); i += 2) {
        res.push_back(std::stoul(s.substr(i, 2), nullptr, 16));
    }

    return res;
}

template <class T>
std::enable_if_t<
    std::disjunction_v<std::is_same<T, uint8_t>, std::is_same<T, int8_t>, std::is_same<T, char>>,
    std::string
>
to_hex(const std::vector<T>& data) {
    std::ostringstream res;

    for (size_t i = 0; i < data.size(); i += 2) {
        res << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(data[i]);
    }

    return res.str();
}
}