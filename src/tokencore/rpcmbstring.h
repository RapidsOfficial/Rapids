#ifndef TOKENCORE_RPCMBSTRING_H
#define TOKENCORE_RPCMBSTRING_H

#include <stdint.h>
#include <string>

namespace mastercore
{
/** Replaces invalid UTF-8 characters or character sequences with question marks. */
std::string SanitizeInvalidUTF8(const std::string& s);
}

#endif // TOKENCORE_RPCMBSTRING_H
