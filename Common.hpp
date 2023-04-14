#ifndef CLIENSERVERECN_COMMON_HPP
#define CLIENSERVERECN_COMMON_HPP

#include <string>

static const short port = 5555;
static const size_t MAX_LENGTH = 1024;
static const std::string serverIp = "127.0.0.1";
enum orderType { sell, buy };

namespace Requests
{
    static std::string Registration = "Reg";
    static std::string Hello = "Hel";
    static std::string AddOrder = "Add";
}

#endif //CLIENSERVERECN_COMMON_HPP
