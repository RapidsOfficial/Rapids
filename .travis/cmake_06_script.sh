#!/usr/bin/env bash
#
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C.UTF-8

BEGIN_FOLD config
mkdir -p $TRAVIS_BUILD_DIR/cmake-build-debug && cd $TRAVIS_BUILD_DIR/cmake-build-debug
cmake -DCMAKE_BUILD_TYPE=Debug -G "CodeBlocks - Unix Makefiles" $TRAVIS_BUILD_DIR
END_FOLD

BEGIN_FOLD build
cmake --build $TRAVIS_BUILD_DIR/cmake-build-debug --target all -- -j 3
END_FOLD