// Copyright (c) 2016-2020 The ZCash developers
// Copyright (c) 2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sapling/util.h"
#include "sync.h"

#include <algorithm>
#include <librustzcash.h>
#include <stdexcept>
#include <iostream>

static fs::path zc_paramsPathCached;
static RecursiveMutex csPathCached;

/**
 * Ignores exceptions thrown by Boost's create_directory if the requested directory exists.
 * Specifically handles case where path p exists, but it wasn't possible for the user to
 * write to the parent directory.
 */
bool TryCreateDir(const fs::path& p)
{
    try {
        return fs::create_directory(p);
    } catch (const fs::filesystem_error&) {
        if (!fs::exists(p) || !fs::is_directory(p))
            throw;
    }

    // create_directory didn't create the directory, it had to have existed already
    return false;
}

static fs::path ZC_GetBaseParamsDir()
{
    // Copied from GetDefaultDataDir and adapter for zcash params.
    // Windows < Vista: C:\Documents and Settings\Username\Application Data\PIVXParams
    // Windows >= Vista: C:\Users\Username\AppData\Roaming\PIVXParams
    // Mac: ~/Library/Application Support/PIVXParams
    // Unix: ~/.pivx-params
#ifdef WIN32
    // Windows
    return GetSpecialFolderPath(CSIDL_APPDATA) / "PIVXParams";
#else
    fs::path pathRet;
    char* pszHome = getenv("HOME");
    if (pszHome == NULL || strlen(pszHome) == 0)
        pathRet = fs::path("/");
    else
        pathRet = fs::path(pszHome);
#ifdef MAC_OSX
    // Mac
    pathRet /= "Library/Application Support";
    TryCreateDir(pathRet);
    return pathRet / "PIVXParams";
#else
    // Unix
    return pathRet / ".pivx-params";
#endif
#endif
}

const fs::path &ZC_GetParamsDir()
{
    LOCK(csPathCached); // Reuse the same lock as upstream.

    fs::path &path = zc_paramsPathCached;

    // This can be called during exceptions by LogPrintf(), so we cache the
    // value so we don't have to do memory allocations after that.
    if (!path.empty())
        return path;

    path = ZC_GetBaseParamsDir();

    return path;
}

void initZKSNARKS()
{
    fs::path sapling_spend = ZC_GetParamsDir() / "sapling-spend.params";
    fs::path sapling_output = ZC_GetParamsDir() / "sapling-output.params";
    fs::path sprout_groth16 = ZC_GetParamsDir() / "sprout-groth16.params";

    if (!(fs::exists(sapling_spend) &&
          fs::exists(sapling_output) &&
          fs::exists(sprout_groth16)
    )) {
        throw std::runtime_error("Sapling params don't exist");
    }

    static_assert(
            sizeof(fs::path::value_type) == sizeof(codeunit),
            "librustzcash not configured correctly");
    auto sapling_spend_str = sapling_spend.native();
    auto sapling_output_str = sapling_output.native();
    auto sprout_groth16_str = sprout_groth16.native();

    //LogPrintf("Loading Sapling (Spend) parameters from %s\n", sapling_spend.string().c_str());

    librustzcash_init_zksnark_params(
            reinterpret_cast<const codeunit*>(sapling_spend_str.c_str()),
            sapling_spend_str.length(),
            "8270785a1a0d0bc77196f000ee6d221c9c9894f55307bd9357c3f0105d31ca63991ab91324160d8f53e2bbd3c2633a6eb8bdf5205d822e7f3f73edac51b2b70c",
            reinterpret_cast<const codeunit*>(sapling_output_str.c_str()),
            sapling_output_str.length(),
            "657e3d38dbb5cb5e7dd2970e8b03d69b4787dd907285b5a7f0790dcc8072f60bf593b32cc2d1c030e00ff5ae64bf84c5c3beb84ddc841d48264b4a171744d028",
            reinterpret_cast<const codeunit*>(sprout_groth16_str.c_str()),
            sprout_groth16_str.length(),
            "e9b238411bd6c0ec4791e9d04245ec350c9c5744f5610dfcce4365d5ca49dfefd5054e371842b3f88fa1b9d7e8e075249b3ebabd167fa8b0f3161292d36c180a"
    );

    std::cout << "### Sapling params initialized ###" << std::endl;
}

std::vector<unsigned char> convertIntToVectorLE(const uint64_t val_int) {
    std::vector<unsigned char> bytes;

    for(size_t i = 0; i < 8; i++) {
        bytes.push_back(val_int >> (i * 8));
    }

    return bytes;
}

// Convert bytes into boolean vector. (MSB to LSB)
std::vector<bool> convertBytesVectorToVector(const std::vector<unsigned char>& bytes) {
    std::vector<bool> ret;
    ret.resize(bytes.size() * 8);

    unsigned char c;
    for (size_t i = 0; i < bytes.size(); i++) {
        c = bytes.at(i);
        for (size_t j = 0; j < 8; j++) {
            ret.at((i*8)+j) = (c >> (7-j)) & 1;
        }
    }

    return ret;
}

// Convert boolean vector (big endian) to integer
uint64_t convertVectorToInt(const std::vector<bool>& v) {
    if (v.size() > 64) {
        throw std::length_error ("boolean vector can't be larger than 64 bits");
    }

    uint64_t result = 0;
    for (size_t i=0; i<v.size();i++) {
        if (v.at(i)) {
            result |= (uint64_t)1 << ((v.size() - 1) - i);
        }
    }

    return result;
}

uint256 random_uint256() {
    uint256 ret;
    randombytes_buf(ret.begin(), 32);

    return ret;
}

uint252 random_uint252() {
    uint256 rand = random_uint256();
    (*rand.begin()) &= 0x0F;

    return uint252(rand);
}
