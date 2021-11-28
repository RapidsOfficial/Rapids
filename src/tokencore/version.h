#ifndef TOKENCORE_VERSION_H
#define TOKENCORE_VERSION_H

#if defined(HAVE_CONFIG_H)
#include "config/pivx-config.h"
#else

//
// Token Core version information are also to be defined in configure.ac.
//
// During the configuration, this information are used for other places.
//

// Increase with every consensus affecting change
#define TOKENCORE_VERSION_MAJOR       0

// Increase with every non-consensus affecting feature
#define TOKENCORE_VERSION_MINOR       5

// Increase with every patch, which is not a feature or consensus affecting
#define TOKENCORE_VERSION_PATCH       0

// Non-public build number/revision (usually zero)
#define TOKENCORE_VERSION_BUILD       0

#endif // HAVE_CONFIG_H

#if !defined(WINDRES_PREPROC)

//
// *-res.rc includes this file, but it cannot cope with real c++ code.
// WINDRES_PREPROC is defined to indicate that its pre-processor is running.
// Anything other than a define should be guarded below:
//

#include <string>

//! Token Core client version
static const int TOKENCORE_VERSION =
                    +100000000000 * TOKENCORE_VERSION_MAJOR
                    +    10000000 * TOKENCORE_VERSION_MINOR
                    +        1000 * TOKENCORE_VERSION_PATCH
                    +           1 * TOKENCORE_VERSION_BUILD;

//! Returns formatted Token Core version, e.g. "1.2.0"
const std::string TokenCoreVersion();

//! Returns formatted Bitcoin Core version, e.g. "0.10", "0.9.3"
const std::string BitcoinCoreVersion();


#endif // WINDRES_PREPROC

#endif // TOKENCORE_VERSION_H
