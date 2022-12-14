#pragma once

typedef std::wstring string_t;
typedef std::unordered_map<string_t, string_t> mapping_t;
typedef std::vector<string_t> string_list_t;

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
templa(string_t source, string_t destination, const mapping_t& mapping,
       const string_list_t& exclude);

TEMPLA_RET templa_main(int argc, wchar_t **argv);
