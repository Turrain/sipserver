#include "core/configuration.h"

namespace core {

template<>
inline std::string Configuration::convert_value<std::string>(const std::string &str) const
{
    return str;
}

template<>
inline bool Configuration::convert_value<bool>(const std::string &str) const
{
    std::istringstream iss(str);
    bool value;
    iss >> std::boolalpha >> value;
    return value;
}

} // namespace core
