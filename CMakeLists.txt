cmake_minimum_required(VERSION 3.10)
project(PIVX)

set(BDB_VER "4.8.30")
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/contrib/cmake")
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 ")
include(${CMAKE_ROOT}/Modules/ExternalProject.cmake)

if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    message(STATUS "Compiling on macOS")
    list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/qt5)
    list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/openssl)
    list(APPEND CMAKE_PREFIX_PATH /usr/local/Cellar/berkeley-db@4)
    set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl/")
    set(ENV{CPPFLAGS} "-I${OPENSSL_ROOT_DIR}/include")
    set(ENV{LDFLAGS} "-L${OPENSSL_ROOT_DIR}/lib")
    set(BerkeleyDB_ROOT_DIR "/usr/local/Cellar/berkeley-db@4/${BDB_VER}/")
elseif(${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    file(READ "/proc/version" _SYS_VERSION)
    string(REGEX MATCH "Microsoft" _SYSTEM_VERSION_PARSED "${_SYS_VERSION}")
    if(${_SYSTEM_VERSION_PARSED} MATCHES "Microsoft")
        message(STATUS "Compiling on WSL")
        set(ENV{triple} x86_64-w64-mingw32)
        set(ENV{target} Windows)
        execute_process(
                COMMAND make HOST=x86_64-w64-mingw32 -j16
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/depends
        )
        file(REMOVE ${CMAKE_BINARY_DIR}/CMakeFiles/${CMAKE_VERSION}/CMakeCXXCompiler.cmake)
        file(REMOVE ${CMAKE_BINARY_DIR}/CMakeFiles/${CMAKE_VERSION}/CMakeCCompiler.cmake)
        set(CMAKE_C_COMPILER /usr/bin/$ENV{triple}-gcc)
        set(CMAKE_CXX_COMPILER /usr/bin/$ENV{triple}-g++)
        set(CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/depends/$ENV{triple})
        set(BOOST_INCLUDEDIR ${CMAKE_CURRENT_SOURCE_DIR}/depends/$ENV{triple}/include/)
        set(BOOST_LIBRARYDIR ${CMAKE_CURRENT_SOURCE_DIR}/depends/$ENV{triple}/lib)
        set(Boost_USE_STATIC_RUNTIME ON)
        set(Boost_THREADAPI "win32")
    else()
        message(STATUS "Compiling on Linux")
        list(APPEND CMAKE_PREFIX_PATH /usr/lib/x86_64-linux-gnu/cmake/Qt5)
        set(Qt5core_DIR "/usr/lib/x86_64-linux-gnu/cmake/Qt5/QtCore")
        include_directories("/usr/include")
        include_directories("/usr/include/x86_64-linux-gnu")
        add_definitions("-D__BYTE_ORDER -D__LITTLE_ENDIAN")
    endif()
endif()

# Find Dependencies
find_package(BerkeleyDB ${BDB_VER} REQUIRED)
if (BerkeleyDB_FOUND)
    if(NOT ${BerkeleyDB_VERSION} MATCHES "${BDB_VER}")
        message(WARNING "BerkeleyDB version other than ${BDB_VER} found!")
        set(BDB_CONFIGURE_FLAGS "--with-incompatible-bdb")
    endif()
    include_directories( ${BerkeleyDB_INCLUDE_DIRS} )
    link_directories( ${BerkeleyDB_ROOT_DIR}/lib )
endif ()

find_package(OpenSSL REQUIRED)
if (OPENSSL_FOUND)
    message(STATUS "Found OpenSSL (${OPENSSL_VERSION}): ${OPENSSL_LIBRARIES}")
    include_directories( ${OPENSSL_INCLUDE_DIR} )
endif()

find_package(LibEvent REQUIRED)
if (LibEvent_FOUND)
    include_directories ( ${LibEvent_INCLUDE_DIR} )
    link_directories ( ${LibEvent_LIBRARY_DIRS} )
endif()

find_package(GMP)
if (GMP_FOUND)
    include_directories ( ${GMP_INCLUDE_DIR} )
    link_directories ( ${GMP_LIBRARY_DIRS} )
else()
    message(WARNING "GMP not found, falling back to OpenSSL for bignum!")
    set(BIGNUM_CONFIGURE_FLAGS "--with-zerocoin-bignum=openssl")
endif()

find_package(ZMQ)
if (ZMQ_Found)
    include_directories ( ${ZMQ_INCLUDE_DIR} )
    link_directories ( ${ZMQ_LIB_DIRS} )
endif()

find_package(Boost COMPONENTS system filesystem thread program_options REQUIRED)
include_directories( ${Boost_INCLUDE_DIRS} )
link_directories ( ${Boost_LIBRARY_DIRS} )

# run autogen.sh if missing header files from configure on Linux/Mac
if (EXISTS	"${CMAKE_CURRENT_SOURCE_DIR}/configure")
else()
    execute_process(
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/autogen.sh
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()

# run configure if pivx_config.h doesn't exist
if (EXISTS	"${CMAKE_CURRENT_SOURCE_DIR}/src/config/pivx-config.h")
else()
    execute_process(
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/configure ${BDB_CONFIGURE_FLAGS} ${BIGNUM_CONFIGURE_FLAGS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    )
endif()
add_definitions(-DHAVE_CONFIG_H)

ExternalProject_Add (
        libunivalue
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/univalue
        CONFIGURE_COMMAND ""
        BUILD_COMMAND make
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND ""
        )

ExternalProject_Add (
        libsecp256k1
        SOURCE_DIR ${CMAKE_SOURCE_DIR}/src/secp256k1
        CONFIGURE_COMMAND ""
        BUILD_COMMAND make
        BUILD_IN_SOURCE 1
        INSTALL_COMMAND ""
)

link_directories(
        ${CMAKE_SOURCE_DIR}/src/univalue/.libs
        ${CMAKE_SOURCE_DIR}/src/secp256k1/.libs
)

include_directories(
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/compat
        ${CMAKE_SOURCE_DIR}/src/config
        ${CMAKE_SOURCE_DIR}/src/obj
        ${CMAKE_SOURCE_DIR}/src/leveldb
        ${CMAKE_SOURCE_DIR}/src/leveldb/include
        ${CMAKE_SOURCE_DIR}/src/leveldb/helpers/memenv
        ${CMAKE_SOURCE_DIR}/src/secp256k1
        ${CMAKE_SOURCE_DIR}/src/secp256k1/include
        ${CMAKE_SOURCE_DIR}/src/univalue/include
)

set(libleveldb_a_headers
        ${CMAKE_CURRENT_SOURCE_DIR}/src/config/pivx-config.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/atomic_pointer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/port_example.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/port_posix.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/win/stdint.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/port.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/port_win.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/thread_annotations.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/db.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/options.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/comparator.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/filter_policy.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/slice.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/table_builder.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/env.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/c.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/iterator.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/cache.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/dumpfile.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/table.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/write_batch.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/include/leveldb/status.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/log_format.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/memtable.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/version_set.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/write_batch_internal.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/filename.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/version_edit.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/dbformat.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/builder.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/log_writer.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/db_iter.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/skiplist.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/db_impl.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/table_cache.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/snapshot.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/log_reader.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/filter_block.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/block_builder.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/block.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/two_level_iterator.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/merger.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/format.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/iterator_wrapper.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/crc32c.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/env_posix_test_helper.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/arena.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/random.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/posix_logger.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/hash.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/histogram.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/coding.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/testutil.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/mutexlock.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/logging.h
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/testharness.h
        )

set(libleveldb_a_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/builder.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/c.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/dbformat.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/db_impl.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/db_iter.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/dumpfile.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/filename.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/log_reader.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/log_writer.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/memtable.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/repair.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/table_cache.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/version_edit.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/version_set.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/db/write_batch.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/block_builder.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/block.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/filter_block.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/format.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/iterator.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/merger.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/table_builder.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/table.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/table/two_level_iterator.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/arena.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/bloom.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/cache.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/coding.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/comparator.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/crc32c.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/env.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/env_posix.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/filter_policy.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/hash.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/histogram.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/logging.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/options.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/status.cc
        )

set(libleveldb_cpp_flags)
if(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    set(libleveldb_cpp_flags -DOS_WINDOWS -DLEVELDB_PLATFORM_WINDOWS -DWINVER=0x0500 -D__USE_MINGW_ANSI_STDIO=1)
    list(APPEND libleveldb_a_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/util/env_win.cc)
    list(APPEND libleveldb_a_srouces ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/port_win.cc)
else()
    if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        set(libleveldb_cpp_flags -DOS_MACOSX -DLEVELDB_PLATFORM_POSIX)
    else()
        set(libleveldb_cpp_flags -DOS_LINUX -DLEVELDB_PLATFORM_POSIX)
    endif()
    list(APPEND libleveldb_a_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/port_posix.cc)
endif()

add_library(leveldb STATIC ${libleveldb_a_headers} ${libleveldb_a_sources})
target_compile_definitions(leveldb PUBLIC ${libleveldb_cpp_flags} -DLEVELDB_ATOMIC_PRESENT -D__STDC_LIMIT_MACROS)
target_compile_options(leveldb PUBLIC -I${CMAKE_CURRENT_SOURCE_DIR}/src/compat)

set(libmemenv_a_sources
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/helpers/memenv/memenv.cc
        ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/helpers/memenv/memenv.h
        )
add_library(memenv STATIC ${libmemenv_a_sources})
target_compile_definitions(memenv PUBLIC ${libleveldb_cpp_flags})

set(libleveldb_sse42_a_sources ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb/port/port_posix_sse.cc)
add_library(leveldb_sse42 ${libleveldb_sse42_a_sources})

link_directories ( ${CMAKE_CURRENT_SOURCE_DIR}/src/leveldb )

file(GLOB HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h)
file(GLOB CRYPTO_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto/*.h)
file(GLOB PRIMITIVE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/primitives/*.h)
file(GLOB ZMQ_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/zmq/*.h)
file(GLOB SCRIPT_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/script/*.h)
file(GLOB RPC_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/rpc/*.h)
file(GLOB COMPAT_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/src/compat/*.h)

source_group("BitcoinHeaders" FILES
        ${HEADERS}
        ${CRYPTO_HEADERS}
        ${PRIMITIVE_HEADERS}
        ${ZMQ_HEADERS}
        ${SCRIPT_HEADERS}
        ${RPC_HEADERS}
        ${COMPAT_HEADERS}
        ./src/support/cleanse.h
        )

set(SERVER_SOURCES
        ./src/addrman.cpp
        ./src/alert.cpp
        ./src/bloom.cpp
        ./src/blocksignature.cpp
        ./src/chain.cpp
        ./src/checkpoints.cpp
        ./src/httprpc.cpp
        ./src/httpserver.cpp
        ./src/init.cpp
        ./src/leveldbwrapper.cpp
        ./src/main.cpp
        ./src/merkleblock.cpp
        ./src/miner.cpp
        ./src/net.cpp
        ./src/noui.cpp
        ./src/pow.cpp
        ./src/rest.cpp
        ./src/rpc/blockchain.cpp
        ./src/rpc/masternode.cpp
        ./src/rpc/budget.cpp
        ./src/rpc/mining.cpp
        ./src/rpc/misc.cpp
        ./src/rpc/net.cpp
        ./src/rpc/rawtransaction.cpp
        ./src/rpc/server.cpp
        ./src/script/sigcache.cpp
        ./src/sporkdb.cpp
        ./src/timedata.cpp
        ./src/torcontrol.cpp
        ./src/txdb.cpp
        ./src/txmempool.cpp
        ./src/validationinterface.cpp
        ./src/zpivchain.cpp
        )
add_library(SERVER_A STATIC ${BitcoinHeaders} ${SERVER_SOURCES})

if (ZMQ_FOUND)
    set(ZMQ_SOURCES
        ./src/zmq/zmqabstractnotifier.cpp
        ./src/zmq/zmqnotificationinterface.cpp
        ./src/zmq/zmqpublishnotifier.cpp
    )
    add_library(ZMQ_A STATIC ${BitcoinHeaders} ${ZMQ_SOURCES})
endif()

set(WALLET_SOURCES
        ./src/activemasternode.cpp
        ./src/bip38.cpp
        ./src/denomination_functions.cpp
        ./src/obfuscation.cpp
        ./src/obfuscation-relay.cpp
        ./src/wallet/db.cpp
        ./src/crypter.cpp
        ./src/swifttx.cpp
        ./src/masternode.cpp
        ./src/masternode-budget.cpp
        ./src/masternode-payments.cpp
        ./src/masternode-sync.cpp
        ./src/masternodeconfig.cpp
        ./src/masternodeman.cpp
        ./src/zpiv/mintpool.cpp
        ./src/wallet/rpcdump.cpp
        ./src/zpiv/deterministicmint.cpp
        ./src/zpiv/zerocoin.cpp
        ./src/wallet/rpcwallet.cpp
        ./src/kernel.cpp
        ./src/wallet/wallet.cpp
        ./src/wallet/wallet_ismine.cpp
        ./src/wallet/walletdb.cpp
        ./src/zpiv/zpivwallet.cpp
        ./src/zpiv/zpivtracker.cpp
        ./src/zpiv/zpivmodule.cpp
        ./src/stakeinput.cpp
        ./src/genwit.cpp
        ./src/lightzpivthread.cpp
        )
add_library(WALLET_A STATIC ${BitcoinHeaders} ${WALLET_SOURCES})

set(BITCOIN_CRYPTO_SOURCES
        ./src/crypto/sha1.cpp
        ./src/crypto/sha256.cpp
        ./src/crypto/sha512.cpp
        ./src/crypto/hmac_sha256.cpp
        ./src/crypto/rfc6979_hmac_sha256.cpp
        ./src/crypto/hmac_sha512.cpp
        ./src/crypto/scrypt.cpp
        ./src/crypto/ripemd160.cpp
        ./src/crypto/aes_helper.c
        ./src/crypto/blake.c
        ./src/crypto/bmw.c
        ./src/crypto/groestl.c
        ./src/crypto/jh.c
        ./src/crypto/keccak.c
        ./src/crypto/skein.c
        ./src/crypto/common.h
        ./src/crypto/sha256.h
        ./src/crypto/sha512.h
        ./src/crypto/hmac_sha256.h
        ./src/crypto/rfc6979_hmac_sha256.h
        ./src/crypto/hmac_sha512.h
        ./src/crypto/scrypt.h
        ./src/crypto/sha1.h
        ./src/crypto/ripemd160.h
        ./src/crypto/sph_blake.h
        ./src/crypto/sph_bmw.h
        ./src/crypto/sph_groestl.h
        ./src/crypto/sph_jh.h
        ./src/crypto/sph_keccak.h
        ./src/crypto/sph_skein.h
        ./src/crypto/sph_types.h
        )
add_library(BITCOIN_CRYPTO_A STATIC ${BITCOIN_CRYPTO_SOURCES})

set(ZEROCOIN_SOURCES
        ./src/libzerocoin/Accumulator.h
        ./src/libzerocoin/AccumulatorProofOfKnowledge.h
        ./src/libzerocoin/bignum.h
        ./src/libzerocoin/bignum.cpp
        ./src/libzerocoin/Coin.h
        ./src/libzerocoin/CoinSpend.h
        ./src/libzerocoin/Commitment.h
        ./src/libzerocoin/Denominations.h
        ./src/libzerocoin/ParamGeneration.h
        ./src/libzerocoin/Params.h
        ./src/libzerocoin/SerialNumberSignatureOfKnowledge.h
        ./src/libzerocoin/SpendType.h
        ./src/libzerocoin/ZerocoinDefines.h
        ./src/libzerocoin/Accumulator.cpp
        ./src/libzerocoin/AccumulatorProofOfKnowledge.cpp
        ./src/libzerocoin/Coin.cpp
        ./src/libzerocoin/Denominations.cpp
        ./src/libzerocoin/CoinSpend.cpp
        ./src/libzerocoin/Commitment.cpp
        ./src/libzerocoin/ParamGeneration.cpp
        ./src/libzerocoin/Params.cpp
        ./src/libzerocoin/SerialNumberSignatureOfKnowledge.cpp
        )
if(GMP_FOUND)
    list(APPEND ZEROCOIN_SOURCES ./src/libzerocoin/bignum_gmp.cpp)
else()
    list(APPEND ZEROCOIN_SOURCES ./src/libzerocoin/bignum_openssl.cpp)
endif()
add_library(ZEROCOIN_A STATIC ${ZEROCOIN_SOURCES})

set(COMMON_SOURCES
        ./src/zpiv/accumulators.cpp
        ./src/zpiv/accumulatorcheckpoints.cpp
        ./src/zpiv/accumulatormap.cpp
        ./src/zpiv/witness.cpp
        ./src/allocators.cpp
        ./src/amount.cpp
        ./src/base58.cpp
        ./src/bip38.cpp
        ./src/chainparams.cpp
        ./src/coins.cpp
        ./src/compressor.cpp
        ./src/primitives/block.cpp
        ./src/zpiv/deterministicmint.cpp
        ./src/primitives/transaction.cpp
        ./src/zpiv/zerocoin.cpp
        ./src/core_read.cpp
        ./src/core_write.cpp
        ./src/hash.cpp
        ./src/invalid.cpp
        ./src/key.cpp
        ./src/keystore.cpp
        ./src/netbase.cpp
        ./src/protocol.cpp
        ./src/pubkey.cpp
        ./src/scheduler.cpp
        ./src/script/interpreter.cpp
        ./src/script/script.cpp
        ./src/script/sign.cpp
        ./src/script/standard.cpp
        ./src/script/script_error.cpp
        ./src/spork.cpp
        ./src/sporkdb.cpp
        )
add_library(COMMON_A STATIC ${BitcoinHeaders} ${COMMON_SOURCES})

set(UTIL_SOURCES
        ./src/allocators.cpp
        ./src/compat/strnlen.cpp
        ./src/compat/glibc_sanity.cpp
        ./src/compat/glibcxx_sanity.cpp
        ./src/chainparamsbase.cpp
        ./src/clientversion.cpp
        ./src/random.cpp
        ./src/rpc/protocol.cpp
        ./src/sync.cpp
        ./src/uint256.cpp
        ./src/util.cpp
        ./src/utilstrencodings.cpp
        ./src/utilmoneystr.cpp
        ./src/utiltime.cpp
        ./src/support/cleanse.cpp
        )
add_library(UTIL_A STATIC ${BitcoinHeaders} ${UTIL_SOURCES})

set(CLI_SOURCES ./src/rpc/client.cpp)
add_library(CLI_A STATIC ${BitcoinHeaders} ${CLI_SOURCES})

add_executable(pivx-cli ${CMAKE_CURRENT_SOURCE_DIR}/src/pivx-cli.cpp)
add_dependencies(pivx-cli libunivalue)
target_link_libraries(pivx-cli
        CLI_A
        univalue
        UTIL_A
        BITCOIN_CRYPTO_A
        ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${LIBEVENT_LIB}
        )

add_executable(pivx-tx ${CMAKE_CURRENT_SOURCE_DIR}/src/pivx-tx.cpp)
add_dependencies(pivx-tx libunivalue libsecp256k1)
target_link_libraries(pivx-tx
        univalue
        COMMON_A
        ZEROCOIN_A
        UTIL_A
        BITCOIN_CRYPTO_A
        secp256k1
        ${Boost_LIBRARIES} ${OPENSSL_LIBRARIES} ${LIBEVENT_LIB}
        )
if(GMP_FOUND)
    target_link_libraries(pivx-tx ${GMP_LIBRARY})
endif()

add_executable(pivxd ${CMAKE_CURRENT_SOURCE_DIR}/src/pivxd.cpp)
add_dependencies(pivxd libunivalue libsecp256k1 leveldb leveldb_sse42 memenv)
target_link_libraries(pivxd
        SERVER_A
        COMMON_A
        univalue
        ZEROCOIN_A
        UTIL_A
        WALLET_A
        BITCOIN_CRYPTO_A
        leveldb leveldb_sse42 memenv secp256k1
        ${BerkeleyDB_LIBRARIES} ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${LIBEVENT_LIB} miniupnpc pthread
        )
if(GMP_FOUND)
    target_link_libraries(pivxd ${GMP_LIBRARY})
endif()
if(ZMQ_FOUND)
    target_link_libraries(pivxd ZMQ_A ${ZMQ_LIB})
endif ()


add_subdirectory(src/qt)
