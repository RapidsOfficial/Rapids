// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2014 The Bitcoin developers
// Copyright (c) 2016-2019 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "utilstrencodings.h"

#include "tinyformat.h"

#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <limits>

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>



static const std::string CHARS_ALPHA_NUM = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static const std::string SAFE_CHARS[] =
{
    CHARS_ALPHA_NUM + " .,;-_/:?@()", // SAFE_CHARS_DEFAULT
    CHARS_ALPHA_NUM + " .,;-_?@", // SAFE_CHARS_UA_COMMENT
    CHARS_ALPHA_NUM + ".-_", // SAFE_CHARS_FILENAME
};

std::string SanitizeString(const std::string& str, int rule)
{
    std::string strResult;
    for (std::string::size_type i = 0; i < str.size(); i++)
    {
        if (SAFE_CHARS[rule].find(str[i]) != std::string::npos)
            strResult.push_back(str[i]);
    }
    return strResult;
}

bool validateURL(std::string strURL, std::string& strErr, unsigned int maxSize) {

    // Check URL size
    if (strURL.size() > maxSize) {
        strErr = strprintf("Invalid URL: %d exceeds limit of %d characters.", strURL.size(), maxSize);
        return false;
    }

    std::vector<std::string> reqPre;

    // Required initial strings; URL must contain one
    reqPre.push_back("http://");
    reqPre.push_back("https://");

    // check fronts
    bool found = false;
    for (int i=0; i < (int) reqPre.size() && !found; i++) {
        if (strURL.find(reqPre[i]) == 0) found = true;
    }
    if ((!found) && (reqPre.size() > 0)) {
        strErr = "Invalid URL, check scheme (e.g. https://)";
        return false;
    }

    return true;
}

const signed char p_util_hexdigit[256] =
    {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1,
        -1, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, 0xa, 0xb, 0xc, 0xd, 0xe, 0xf, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

signed char HexDigit(char c)
{
    return p_util_hexdigit[(unsigned char)c];
}

bool IsHex(const std::string& str)
{
    for (std::string::const_iterator it(str.begin()); it != str.end(); ++it) {
        if (HexDigit(*it) < 0)
            return false;
    }
    return (str.size() > 0) && (str.size() % 2 == 0);
}

std::vector<unsigned char> ParseHex(const char* psz)
{
    // convert hex dump to vector
    std::vector<unsigned char> vch;
    while (true) {
        while (isspace(*psz))
            psz++;
        signed char c = HexDigit(*psz++);
        if (c == (signed char)-1)
            break;
        unsigned char n = (c << 4);
        c = HexDigit(*psz++);
        if (c == (signed char)-1)
            break;
        n |= c;
        vch.push_back(n);
    }
    return vch;
}

std::vector<unsigned char> ParseHex(const std::string& str)
{
    return ParseHex(str.c_str());
}

std::string EncodeBase64(const unsigned char* pch, size_t len)
{
    static const char *pbase64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string str;
    str.reserve(((len + 2) / 3) * 4);
    ConvertBits<8, 6, true>([&](int v) { str += pbase64[v]; }, pch, pch + len);
    while (str.size() % 4) str += '=';
    return str;
}

std::string EncodeBase64(const std::string& str)
{
    return EncodeBase64((const unsigned char*)str.c_str(), str.size());
}

std::vector<unsigned char> DecodeBase64(const char* p, bool* pfInvalid)
{
    static const int decode64_table[256] =
    {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1,
        -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27, 28,
        29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48,
        49, 50, 51, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
    };

    const char* e = p;
    std::vector<uint8_t> val;
    val.reserve(strlen(p));
    while (*p != 0) {
        int x = decode64_table[(unsigned char)*p];
        if (x == -1) break;
        val.push_back(x);
        ++p;
    }

    std::vector<unsigned char> ret;
    ret.reserve((val.size() * 3) / 4);
    bool valid = ConvertBits<6, 8, false>([&](unsigned char c) { ret.push_back(c); }, val.begin(), val.end());

    const char* q = p;
    while (valid && *p != 0) {
        if (*p != '=') {
            valid = false;
            break;
        }
        ++p;
    }
    valid = valid && (p - e) % 4 == 0 && p - q < 4;
    if (pfInvalid) *pfInvalid = !valid;

    return ret;
}

std::string DecodeBase64(const std::string& str)
{
    std::vector<unsigned char> vchRet = DecodeBase64(str.c_str());
    return (vchRet.size() == 0) ? std::string() : std::string((const char*)&vchRet[0], vchRet.size());
}

// Base64 encoding with secure memory allocation
SecureString EncodeBase64Secure(const SecureString& input)
{
    // Init openssl BIO with base64 filter and memory output
    BIO *b64, *mem;
    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL); // No newlines in output
    mem = BIO_new(BIO_s_mem());
    BIO_push(b64, mem);

    // Decode the string
    BIO_write(b64, &input[0], input.size());
    (void)BIO_flush(b64);

    // Create output variable from buffer mem ptr
    BUF_MEM* bptr;
    BIO_get_mem_ptr(b64, &bptr);
    SecureString output(bptr->data, bptr->length);

    // Cleanse secure data buffer from memory
    memory_cleanse((void*)bptr->data, bptr->length);

    // Free memory
    BIO_free_all(b64);
    return output;
}

std::string EncodeBase32(const unsigned char* pch, size_t len)
{
    static const char *pbase32 = "abcdefghijklmnopqrstuvwxyz234567";

    std::string str;
    str.reserve(((len + 4) / 5) * 8);
    ConvertBits<8, 5, true>([&](int v) { str += pbase32[v]; }, pch, pch + len);
    while (str.size() % 8) str += '=';
    return str;
}

std::string EncodeBase32(const std::string& str)
{
    return EncodeBase32((const unsigned char*)str.c_str(), str.size());
}

std::vector<unsigned char> DecodeBase32(const char* p, bool* pfInvalid)
{
    static const int decode32_table[256] =
    {
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 26, 27, 28, 29, 30, 31, -1, -1, -1, -1,
        -1, -1, -1, -1, -1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1,  0,  1,  2,
         3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22,
        23, 24, 25, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
        -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
    };

    const char* e = p;
    std::vector<uint8_t> val;
    val.reserve(strlen(p));
    while (*p != 0) {
        int x = decode32_table[(unsigned char)*p];
        if (x == -1) break;
        val.push_back(x);
        ++p;
    }

    std::vector<unsigned char> ret;
    ret.reserve((val.size() * 5) / 8);
    bool valid = ConvertBits<5, 8, false>([&](unsigned char c) { ret.push_back(c); }, val.begin(), val.end());

    const char* q = p;
    while (valid && *p != 0) {
        if (*p != '=') {
            valid = false;
            break;
        }
        ++p;
    }
    valid = valid && (p - e) % 8 == 0 && p - q < 8;
    if (pfInvalid) *pfInvalid = !valid;

    return ret;
}

std::string DecodeBase32(const std::string& str)
{
    std::vector<unsigned char> vchRet = DecodeBase32(str.c_str());
    return (vchRet.size() == 0) ? std::string() : std::string((const char*)&vchRet[0], vchRet.size());
}

static bool ParsePrechecks(const std::string& str)
{
    if (str.empty()) // No empty string allowed
        return false;
    if (str.size() >= 1 && (isspace(str[0]) || isspace(str[str.size()-1]))) // No padding allowed
        return false;
    if (str.size() != strlen(str.c_str())) // No embedded NUL characters allowed
        return false;
    return true;
}

bool ParseInt32(const std::string& str, int32_t *out)
{
    if (!ParsePrechecks(str))
        return false;
    char *endp = NULL;
    errno = 0; // strtol will not set errno if valid
    long int n = strtol(str.c_str(), &endp, 10);
    if(out) *out = (int32_t)n;
    // Note that strtol returns a *long int*, so even if strtol doesn't report a over/underflow
    // we still have to check that the returned value is within the range of an *int32_t*. On 64-bit
    // platforms the size of these types may be different.
    return endp && *endp == 0 && !errno &&
        n >= std::numeric_limits<int32_t>::min() &&
        n <= std::numeric_limits<int32_t>::max();
}

bool ParseInt64(const std::string& str, int64_t *out)
{
    if (!ParsePrechecks(str))
        return false;
    char *endp = NULL;
    errno = 0; // strtoll will not set errno if valid
    long long int n = strtoll(str.c_str(), &endp, 10);
    if(out) *out = (int64_t)n;
    // Note that strtoll returns a *long long int*, so even if strtol doesn't report a over/underflow
    // we still have to check that the returned value is within the range of an *int64_t*.
    return endp && *endp == 0 && !errno &&
        n >= std::numeric_limits<int64_t>::min() &&
        n <= std::numeric_limits<int64_t>::max();
}

bool ParseDouble(const std::string& str, double *out)
{
    if (!ParsePrechecks(str))
        return false;
    if (str.size() >= 2 && str[0] == '0' && str[1] == 'x') // No hexadecimal floats allowed
        return false;
    std::istringstream text(str);
    text.imbue(std::locale::classic());
    double result;
    text >> result;
    if(out) *out = result;
    return text.eof() && !text.fail();
}

std::string FormatParagraph(const std::string in, size_t width, size_t indent)
{
    std::stringstream out;
    size_t col = 0;
    size_t ptr = 0;
    while (ptr < in.size()) {
        // Find beginning of next word
        ptr = in.find_first_not_of(' ', ptr);
        if (ptr == std::string::npos)
            break;
        // Find end of next word
        size_t endword = in.find_first_of(' ', ptr);
        if (endword == std::string::npos)
            endword = in.size();
        // Add newline and indentation if this wraps over the allowed width
        if (col > 0) {
            if ((col + endword - ptr) > width) {
                out << '\n';
                for (size_t i = 0; i < indent; ++i)
                    out << ' ';
                col = 0;
            } else
                out << ' ';
        }
        // Append word
        out << in.substr(ptr, endword - ptr);
        col += endword - ptr + 1;
        ptr = endword;
    }
    return out.str();
}

std::string i64tostr(int64_t n)
{
    return strprintf("%d", n);
}

std::string itostr(int n)
{
    return strprintf("%d", n);
}

int64_t atoi64(const char* psz)
{
#ifdef _TOKEN_VER
    return _atoi64(psz);
#else
    return strtoll(psz, NULL, 10);
#endif
}

int64_t atoi64(const std::string& str)
{
#ifdef _TOKEN_VER
    return _atoi64(str.c_str());
#else
    return strtoll(str.c_str(), NULL, 10);
#endif
}

int atoi(const std::string& str)
{
    return atoi(str.c_str());
}


/** Upper bound for mantissa.
* 10^18-1 is the largest arbitrary decimal that will fit in a signed 64-bit integer.
* Larger integers cannot consist of arbitrary combinations of 0-9:
*
*   999999999999999999  1^18-1
*  9223372036854775807  (1<<63)-1  (max int64_t)
*  9999999999999999999  1^19-1     (would overflow)
*/
static const int64_t UPPER_BOUND = 1000000000000000000LL - 1LL;

/** Helper function for ParseFixedPoint */
static inline bool ProcessMantissaDigit(char ch, int64_t &mantissa, int &mantissa_tzeros)
{
    if(ch == '0')
        ++mantissa_tzeros;
    else {
        for (int i=0; i<=mantissa_tzeros; ++i) {
            if (mantissa > (UPPER_BOUND / 10LL))
                return false; /* overflow */
            mantissa *= 10;
        }
        mantissa += ch - '0';
        mantissa_tzeros = 0;
    }
    return true;
}

bool ParseFixedPoint(const std::string &val, int decimals, int64_t *amount_out)
{
    int64_t mantissa = 0;
    int64_t exponent = 0;
    int mantissa_tzeros = 0;
    bool mantissa_sign = false;
    bool exponent_sign = false;
    int ptr = 0;
    int end = val.size();
    int point_ofs = 0;

    if (ptr < end && val[ptr] == '-') {
        mantissa_sign = true;
        ++ptr;
    }
    if (ptr < end)
    {
        if (val[ptr] == '0') {
            /* pass single 0 */
            ++ptr;
        } else if (val[ptr] >= '1' && val[ptr] <= '9') {
            while (ptr < end && val[ptr] >= '0' && val[ptr] <= '9') {
                if (!ProcessMantissaDigit(val[ptr], mantissa, mantissa_tzeros))
                    return false; /* overflow */
                ++ptr;
            }
        } else return false; /* missing expected digit */
    } else return false; /* empty string or loose '-' */
    if (ptr < end && val[ptr] == '.')
    {
        ++ptr;
        if (ptr < end && val[ptr] >= '0' && val[ptr] <= '9')
        {
            while (ptr < end && val[ptr] >= '0' && val[ptr] <= '9') {
                if (!ProcessMantissaDigit(val[ptr], mantissa, mantissa_tzeros))
                    return false; /* overflow */
                ++ptr;
                ++point_ofs;
            }
        } else return false; /* missing expected digit */
    }
    if (ptr < end && (val[ptr] == 'e' || val[ptr] == 'E'))
    {
        ++ptr;
        if (ptr < end && val[ptr] == '+')
            ++ptr;
        else if (ptr < end && val[ptr] == '-') {
            exponent_sign = true;
            ++ptr;
        }
        if (ptr < end && val[ptr] >= '0' && val[ptr] <= '9') {
            while (ptr < end && val[ptr] >= '0' && val[ptr] <= '9') {
                if (exponent > (UPPER_BOUND / 10LL))
                    return false; /* overflow */
                exponent = exponent * 10 + val[ptr] - '0';
                ++ptr;
            }
        } else return false; /* missing expected digit */
    }
    if (ptr != end)
        return false; /* trailing garbage */

    /* finalize exponent */
    if (exponent_sign)
        exponent = -exponent;
    exponent = exponent - point_ofs + mantissa_tzeros;

    /* finalize mantissa */
    if (mantissa_sign)
        mantissa = -mantissa;

    /* convert to one 64-bit fixed-point value */
    exponent += decimals;
    if (exponent < 0)
        return false; /* cannot represent values smaller than 10^-decimals */
    if (exponent >= 18)
        return false; /* cannot represent values larger than or equal to 10^(18-decimals) */

    for (int i=0; i < exponent; ++i) {
        if (mantissa > (UPPER_BOUND / 10LL) || mantissa < -(UPPER_BOUND / 10LL))
            return false; /* overflow */
        mantissa *= 10;
    }
    if (mantissa > UPPER_BOUND || mantissa < -UPPER_BOUND)
        return false; /* overflow */

    if (amount_out)
        *amount_out = mantissa;

    return true;
}
