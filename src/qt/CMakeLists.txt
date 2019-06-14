CMAKE_MINIMUM_REQUIRED(VERSION 3.10)

set(CMAKE_INCLUDE_CURRENT_DIR OFF)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/forms)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (${CMAKE_SYSTEM_NAME} MATCHES "Linux")
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif ()

find_package(Qrcode)
if (QRCODE_FOUND)
    include_directories ( ${QRCODE_INCLUDE_DIR} )
    link_directories ( ${QRCODE_LIBRARY_DIRS} )
endif()

if($ENV{target} MATCHES "Windows")
    find_package(Qt5)
else()
    unset(Qt5Enables)
    foreach(comp Widgets Core Gui Network LinguistTools DBus)
        find_package(Qt5${comp})
        set(Qt5Enables "${Qt5Enables};Qt5${comp}_FOUND")
        if(Qt5${comp}_FOUND)
            MESSAGE(STATUS "FOUND QT5${comp}")
            include_directories(${include_directories} ${Qt5${comp}_INCLUDE_DIRS} ${QT_USE_FILE})
            add_definitions(${Qt5${comp}_DEFINITIONS})
            list(APPEND Qt5_LIBRARIES ${Qt5${comp}_LIBRARIES})
        else()
            MESSAGE(WARNING "Cant find Qt5${comp}")
        endif()
    endforeach(comp)

    find_package(Qt5DBus)
    if (Qt5DBus_FOUND)
        include_directories(${include_directories} "/usr/local/opt/qt5/include/QtDBus/")
        add_compile_options("-DUSE_DBUS")
    endif()
endif()

# Why isn't this done automatically??
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_AUTOMOC_MOC_OPTIONS "-DQ_OS_MAC")
endif()

file(GLOB LOCAL_QT_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)
source_group("QtHeaders" FILES ${LOCAL_QT_HEADERS})

find_package(Protobuf REQUIRED)
include_directories(${PROTOBUF_INCLUDE_DIRS})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

PROTOBUF_GENERATE_CPP(PROTO_SRCS PROTO_HDRS ${CMAKE_CURRENT_SOURCE_DIR}/paymentrequest.proto)

set(GUI_FORMS
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/addressbookpage.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/askpassphrasedialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/bip38tooldialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/coincontroldialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/governancepage.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/zpivcontroldialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/blockexplorer.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/editaddressdialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/helpmessagedialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/intro.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/masternodelist.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/multisenddialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/obfuscationconfig.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/privacydialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/openuridialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/optionsdialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/overviewpage.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/receivecoinsdialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/receiverequestdialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/rpcconsole.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/sendcoinsdialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/sendcoinsentry.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/signverifymessagedialog.ui
        ${CMAKE_CURRENT_SOURCE_DIR}/forms/transactiondescdialog.ui
        )

SET(QT_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/multisenddialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bantablemodel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bip38tooldialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/blockexplorer.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bitcoinaddressvalidator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bitcoinamountfield.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bitcoingui.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/bitcoinunits.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/clientmodel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/csvmodelwriter.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/governancepage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/guiutil.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/intro.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/multisigdialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/masternodelist.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/networkstyle.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/notificator.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/optionsdialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/optionsmodel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/peertablemodel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/platformstyle.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/qvalidatedlineedit.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/qvaluecombobox.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/rpcconsole.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/splashscreen.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/trafficgraphwidget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/utilitydialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/addressbookpage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/addresstablemodel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/askpassphrasedialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/coincontroldialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/zpivcontroldialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/coincontroltreewidget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/privacydialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/proposalframe.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/editaddressdialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/masternodelist.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/openuridialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/overviewpage.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/paymentrequestplus.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/paymentserver.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/receivecoinsdialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/receiverequestdialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/recentrequeststablemodel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sendcoinsdialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/sendcoinsentry.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/signverifymessagedialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/transactiondesc.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/transactiondescdialog.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/transactionfilterproxy.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/transactionrecord.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/transactiontablemodel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/transactionview.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/walletframe.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/walletmodel.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/walletmodeltransaction.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/walletview.cpp
        )

if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    list(APPEND QT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/macdockiconhandler.mm)
    list(APPEND QT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/macnotificationhandler.mm)
endif()

if (${CMAKE_SYSTEM_NAME} MATCHES "Windows")
    list(APPEND QT_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/winshutdownmonitor.cpp)
endif ()

add_library(qt_stuff STATIC ${BitcoinHeaders} ${QtHeaders} ${QT_SOURCES} ${PROTO_SRCS} ${PROTO_HDRS})
target_include_directories(qt_stuff PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
set_property(TARGET qt_stuff PROPERTY CXX_STANDARD 11)

file(GLOB QT_TRANSLATIONS_FILES ${CMAKE_CURRENT_SOURCE_DIR}/locale/*.ts)

get_filename_component(QM_FILES_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/locale" ABSOLUTE)
set_source_files_properties(${QT_TRANSLATIONS_FILES} PROPERTIES OUTPUT_LOCATION "${QM_FILES_BINARY_DIR}")

QT5_ADD_TRANSLATION(QM ${QT_TRANSLATIONS_FILES})
add_custom_target(translations_target ALL DEPENDS ${QM})

QT5_ADD_RESOURCES(QRC_RESOURCE pivx.qrc)
QT5_ADD_RESOURCES(QRC_LOCALE_RESOURCE pivx_locale.qrc)

add_executable(pivx-qt pivx.cpp ${QM} ${QRC_RESOURCE} ${QRC_LOCALE_RESOURCE})
add_dependencies(pivx-qt translations_target libunivalue libsecp256k1 leveldb leveldb_sse42 memenv)
target_link_libraries(pivx-qt
        qt_stuff
        univalue
        SERVER_A UTIL_A WALLET_A CLI_A COMMON_A BITCOIN_CRYPTO_A ZEROCOIN_A
        leveldb leveldb_sse42 memenv miniupnpc secp256k1
        ${BerkeleyDB_LIBRARIES} ${OPENSSL_LIBRARIES} ${Boost_LIBRARIES} ${PROTOBUF_LIBRARIES} ${LIBEVENT_LIB}
        pthread
        )
if (${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    target_link_libraries(pivx-qt "-framework Cocoa")
endif()
if(GMP_FOUND)
    target_link_libraries(pivx-qt ${GMP_LIBRARY})
endif()
if(ZMQ_FOUND)
    target_link_libraries(pivx-qt ZMQ_A ${ZMQ_LIB})
endif()
if (QRCODE_FOUND)
    target_link_libraries(pivx-qt ${QRCODE_LIB})
endif()

QT5_USE_Modules(pivx-qt Gui)
QT5_USE_Modules(pivx-qt Core)
QT5_USE_Modules(pivx-qt Widgets)
QT5_USE_Modules(pivx-qt Test)
QT5_USE_Modules(pivx-qt PrintSupport)
QT5_USE_Modules(pivx-qt Network)
if (Qt5DBus_FOUND)
    QT5_USE_Modules(pivx-qt DBus)
endif()
