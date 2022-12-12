#pragma once

typedef std::wstring string_t;
typedef std::string binary_t;

#include <unordered_map>
typedef std::unordered_map<string_t, string_t> mapping_t;

enum ENCODING_TYPE
{
    ET_BINARY,
    ET_UTF8,
    ET_UTF16,
    ET_UTF16BE,
    ET_ANSI,
    ET_ASCII
};

enum ENCODING_NEWLINE
{
    EN_CRLF,
    EN_LF,
    EN_CR,
    EN_UNKNOWN,
};

struct ENCODING
{
    ENCODING_TYPE type = ET_BINARY;
    bool bom = false;
    ENCODING_NEWLINE newline = EN_CRLF;
};
