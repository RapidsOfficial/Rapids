#include "tokencore/version.h"

#include "clientversion.h"
#include "tinyformat.h"

#include <string>

#ifdef HAVE_BUILD_INFO
#    include "build.h"
#endif

#ifdef TOKENCORE_VERSION_STATUS
#    define TOKENCORE_VERSION_SUFFIX STRINGIZE(TOKENCORE_VERSION_STATUS)
#else
#    define TOKENCORE_VERSION_SUFFIX ""
#endif

//! Returns formatted Token Core version, e.g. "1.2.0" or "1.3.4.1"
const std::string TokenCoreVersion()
{
    if (TOKENCORE_VERSION_BUILD) {
        return strprintf("%d.%d.%d.%d",
                TOKENCORE_VERSION_MAJOR,
                TOKENCORE_VERSION_MINOR,
                TOKENCORE_VERSION_PATCH,
                TOKENCORE_VERSION_BUILD);
    } else {
        return strprintf("%d.%d.%d",
                TOKENCORE_VERSION_MAJOR,
                TOKENCORE_VERSION_MINOR,
                TOKENCORE_VERSION_PATCH);
    }
}

//! Returns formatted Bitcoin Core version, e.g. "0.10", "0.9.3"
const std::string BitcoinCoreVersion()
{
    if (CLIENT_VERSION_BUILD) {
        return strprintf("%d.%d.%d.%d",
                CLIENT_VERSION_MAJOR,
                CLIENT_VERSION_MINOR,
                CLIENT_VERSION_REVISION,
                CLIENT_VERSION_BUILD);
    } else {
        return strprintf("%d.%d.%d",
                CLIENT_VERSION_MAJOR,
                CLIENT_VERSION_MINOR,
                CLIENT_VERSION_REVISION);
    }
}
