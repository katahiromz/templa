#pragma once

typedef std::wstring string_t;
typedef std::string binary_t;
typedef std::unordered_map<string_t, string_t> mapping_t;
typedef std::vector<string_t> string_vector_t;

enum ENCODING_TYPE
{
    ET_BINARY,
    ET_UTF8,
    ET_UTF16,
    ET_UTF16BE,
    ET_ANSI,
    ET_ASCII,
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

const char *templa_get_version(void);
const char *templa_get_usage(void);

enum TEMPLA_RET
{
    TEMPLA_RET_OK = 0,
    TEMPLA_RET_SYNTAXERROR,
    TEMPLA_RET_INPUTERROR,
    TEMPLA_RET_OUTPUTERROR,
};

TEMPLA_RET
templa(string_t file1, string_t destination, const mapping_t& mapping,
       const string_vector_t& exclude);

TEMPLA_RET templa_main(int argc, wchar_t **argv);
