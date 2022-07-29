// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin developers
// Copyright (c) 2015-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparamsbase.h"
#include "logging.h"
#include "utiltime.h"


const char * const DEFAULT_DEBUGLOGFILE = "debug.log";

/**
 * NOTE: the logger instances is leaked on exit. This is ugly, but will be
 * cleaned up by the OS/libc. Defining a logger as a global object doesn't work
 * since the order of destruction of static/global objects is undefined.
 * Consider if the logger gets destroyed, and then some later destructor calls
 * LogPrintf, maybe indirectly, and you get a core dump at shutdown trying to
 * access the logger. When the shutdown sequence is fully audited and tested,
 * explicit destruction of these objects can be implemented by changing this
 * from a raw pointer to a std::unique_ptr.
 *
 * This method of initialization was originally introduced in
 * bitcoin@ee3374234c60aba2cc4c5cd5cac1c0aefc2d817c.
 */
BCLog::Logger* const g_logger = new BCLog::Logger();

bool fLogIPs = DEFAULT_LOGIPS;


static int FileWriteStr(const std::string &str, FILE *fp)
{
    return fwrite(str.data(), 1, str.size(), fp);
}

bool BCLog::Logger::OpenDebugLog()
{
    std::lock_guard<std::mutex> scoped_lock(m_file_mutex);

    assert(m_fileout == nullptr);
    assert(!m_file_path.empty());

    m_fileout = fsbridge::fopen(m_file_path, "a");
    if (!m_fileout) return false;

    setbuf(m_fileout, nullptr); // unbuffered
    // dump buffered messages from before we opened the log
    while (!m_msgs_before_open.empty()) {
        FileWriteStr(m_msgs_before_open.front(), m_fileout);
        m_msgs_before_open.pop_front();
    }

    return true;
}

void BCLog::Logger::EnableCategory(BCLog::LogFlags flag)
{
    m_categories |= flag;
}

bool BCLog::Logger::EnableCategory(const std::string& str)
{
    BCLog::LogFlags flag;
    if (!GetLogCategory(flag, str)) return false;
    EnableCategory(flag);
    return true;
}

void BCLog::Logger::DisableCategory(BCLog::LogFlags flag)
{
    m_categories &= ~flag;
}

bool BCLog::Logger::DisableCategory(const std::string& str)
{
    BCLog::LogFlags flag;
    if (!GetLogCategory(flag, str)) return false;
    DisableCategory(flag);
    return true;
}

bool BCLog::Logger::WillLogCategory(BCLog::LogFlags category) const
{
    return (m_categories.load(std::memory_order_relaxed) & category) != 0;
}

bool BCLog::Logger::DefaultShrinkDebugFile() const
{
    return m_categories == BCLog::NONE;
}

struct CLogCategoryDesc
{
    BCLog::LogFlags flag;
    std::string category;
};

const CLogCategoryDesc LogCategories[] = {
        {BCLog::NONE,           "0"},
        {BCLog::NET,            "net"},
        {BCLog::TOR,            "tor"},
        {BCLog::MEMPOOL,        "mempool"},
        {BCLog::HTTP,           "http"},
        {BCLog::BENCH,          "bench"},
        {BCLog::ZMQ,            "zmq"},
        {BCLog::DB,             "db"},
        {BCLog::RPC,            "rpc"},
        {BCLog::ESTIMATEFEE,    "estimatefee"},
        {BCLog::ADDRMAN,        "addrman"},
        {BCLog::SELECTCOINS,    "selectcoins"},
        {BCLog::REINDEX,        "reindex"},
        {BCLog::CMPCTBLOCK,     "cmpctblock"},
        {BCLog::RAND,           "rand"},
        {BCLog::PRUNE,          "prune"},
        {BCLog::PROXY,          "proxy"},
        {BCLog::MEMPOOLREJ,     "mempoolrej"},
        {BCLog::LIBEVENT,       "libevent"},
        {BCLog::COINDB,         "coindb"},
        {BCLog::QT,             "qt"},
        {BCLog::LEVELDB,        "leveldb"},
        {BCLog::STAKING,        "staking"},
        {BCLog::MASTERNODE,     "masternode"},
        {BCLog::MNBUDGET,       "mnbudget"},
        {BCLog::LEGACYZC,       "zero"},
        {BCLog::MNPING,         "mnping"},
        {BCLog::ALL,            "1"},
        {BCLog::ALL,            "all"},
};

bool GetLogCategory(BCLog::LogFlags& flag, const std::string& str)
{
    if (str == "") {
        flag = BCLog::ALL;
        return true;
    }
    for (const CLogCategoryDesc& category_desc : LogCategories) {
        if (category_desc.category == str) {
            flag = category_desc.flag;
            return true;
        }
    }
    return false;
}

std::string ListLogCategories()
{
    std::string ret;
    int outcount = 0;
    for (const CLogCategoryDesc& category_desc : LogCategories) {
        // Omit the special cases.
        if (category_desc.flag != BCLog::NONE && category_desc.flag != BCLog::ALL) {
            if (outcount != 0) ret += ", ";
            ret += category_desc.category;
            outcount++;
        }
    }
    return ret;
}

std::vector<CLogCategoryActive> ListActiveLogCategories()
{
    std::vector<CLogCategoryActive> ret;
    for (const CLogCategoryDesc& category_desc : LogCategories) {
        // Omit the special cases.
        if (category_desc.flag != BCLog::NONE && category_desc.flag != BCLog::ALL) {
            CLogCategoryActive catActive;
            catActive.category = category_desc.category;
            catActive.active = LogAcceptCategory(category_desc.flag);
            ret.push_back(catActive);
        }
    }
    return ret;
}

std::string BCLog::Logger::LogTimestampStr(const std::string &str)
{
    std::string strStamped;

    if (!m_log_timestamps)
        return str;

    if (m_started_new_line)
        strStamped =  DateTimeStrFormat("%Y-%m-%d %H:%M:%S", GetTime()) + ' ' + str;
    else
        strStamped = str;

    if (!str.empty() && str[str.size()-1] == '\n')
        m_started_new_line = true;
    else
        m_started_new_line = false;

    return strStamped;
}

int BCLog::Logger::LogPrintStr(const std::string &str)
{
    int ret = 0; // Returns total number of characters written

    std::string strTimestamped = LogTimestampStr(str);

    if (m_print_to_console) {
        // print to console
        ret = fwrite(strTimestamped.data(), 1, strTimestamped.size(), stdout);
        fflush(stdout);
    } else if (m_print_to_file) {
        std::lock_guard<std::mutex> scoped_lock(m_file_mutex);

        // buffer if we haven't opened the log yet
        if (m_fileout == NULL) {
            ret = strTimestamped.length();
            m_msgs_before_open.push_back(strTimestamped);

        } else {
            // reopen the log file, if requested
            if (m_reopen_file) {
                m_reopen_file = false;
                if (fsbridge::freopen(m_file_path,"a",m_fileout) != NULL)
                    setbuf(m_fileout, NULL); // unbuffered
            }

            ret = FileWriteStr(strTimestamped, m_fileout);
        }
    }

    return ret;
}

void BCLog::Logger::ShrinkDebugFile()
{
    // Amount of debug.log to save at end when shrinking (must fit in memory)
    constexpr size_t RECENT_DEBUG_HISTORY_SIZE = 10 * 1000000;

    assert(!m_file_path.empty());

    // Scroll debug.log if it's getting too big
    FILE* file = fsbridge::fopen(m_file_path, "r");
    if (file && fs::file_size(m_file_path) > RECENT_DEBUG_HISTORY_SIZE) {
        // Restart the file with some of the end
        std::vector<char> vch(200000, 0);
        fseek(file, -((long)vch.size()), SEEK_END);
        int nBytes = fread(vch.data(), 1, vch.size(), file);
        fclose(file);

        file = fsbridge::fopen(m_file_path, "w");
        if (file) {
            fwrite(vch.data(), 1, nBytes, file);
            fclose(file);
        }
    } else if (file != NULL)
        fclose(file);
}
