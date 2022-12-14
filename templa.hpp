#pragma once

typedef std::wstring string_t;
typedef std::unordered_map<string_t, string_t> mapping_t;
typedef std::vector<string_t> string_list_t;
typedef std::string binary_t;

const char *templa_get_version(void);
const char *templa_get_usage(void);

enum TEMPLA_RET
{
    TEMPLA_RET_OK = 0,
    TEMPLA_RET_SYNTAXERROR,
    TEMPLA_RET_READERROR,
    TEMPLA_RET_WRITEERROR,
    TEMPLA_RET_LOGICALERROR,
};

TEMPLA_RET
templa(string_t source, string_t destination, const mapping_t& mapping,
       const string_list_t& exclude);

TEMPLA_RET templa_main(int argc, wchar_t **argv);

bool templa_load_file(const string_t& filename, binary_t& data);
bool templa_save_file(const string_t& filename, const void *ptr, size_t data_size);

enum TEMPLA_ENCODING_TYPE
{
    TET_BINARY,
    TET_UTF8,
    TET_UTF16,
    TET_UTF16BE,
    TET_ANSI,
    TET_ASCII,
};

enum TEMPLA_NEWLINE
{
    TNL_CRLF,
    TNL_LF,
    TNL_CR,
    TNL_UNKNOWN,
};

struct TEMPLA_ENCODING
{
    TEMPLA_ENCODING_TYPE type = TET_BINARY;
    TEMPLA_NEWLINE newline = TNL_CRLF;
    bool bom = false;
};

bool templa_load_file_ex(const string_t& filename, string_t& string, TEMPLA_ENCODING& encoding);
bool templa_save_file_ex(const string_t& filename, string_t& string, const TEMPLA_ENCODING& encoding);
