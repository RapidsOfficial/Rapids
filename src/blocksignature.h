// Copyright (c) 2017-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef PIVX_BLOCKSIGNATURE_H
#define PIVX_BLOCKSIGNATURE_H

#include "key.h"
#include "primitives/block.h"
#include "keystore.h"

bool SignBlockWithKey(CBlock& block, const CKey& key);
bool SignBlock(CBlock& block, const CKeyStore& keystore, int nHeight, int reductionHeight);
bool CheckBlockSignature(const CBlock& block, const bool enableP2PKH, const int nHeight, const int reductionHeight);

#endif //PIVX_BLOCKSIGNATURE_H
