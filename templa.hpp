#pragma once

#include <string>
#include <map>
#include <vector>

typedef std::wstring string_t;
typedef std::map<string_t, string_t> mapping_t;
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
    TEMPLA_RET_CANCELED,
};

typedef bool (*templa_canceler_t)(); // return true to cancel

TEMPLA_RET
templa(string_t source, string_t destination, const mapping_t& mapping,
       const string_list_t& ignore, templa_canceler_t canceler = NULL);

TEMPLA_RET templa_main(int argc, wchar_t **argv);

bool templa_load_file(const string_t& filename, binary_t& data);
bool templa_save_file(const string_t& filename, const void *ptr, size_t data_size);

inline bool templa_save_file(const string_t& filename, const binary_t& data)
{
    return templa_save_file(filename, &data[0], data.size());
}

enum TEMPLA_ENCODING
{
    TE_BINARY,
    TE_UTF8,
    TE_UTF16,
    TE_UTF16BE,
    TE_ANSI,
    TE_ASCII,
};

enum TEMPLA_NEWLINE
{
    TNL_CRLF,
    TNL_LF,
    TNL_CR,
    TNL_UNKNOWN,
};

struct TEMPLA_FILE
{
    binary_t m_binary;
    string_t m_string;
    TEMPLA_ENCODING m_encoding = TE_BINARY;
    TEMPLA_NEWLINE m_newline = TNL_UNKNOWN;
    bool m_bom = false;

    bool load(const string_t& filename);
    bool save(const string_t& filename);
    void detect_encoding();
    void detect_newline();
};

bool templa_wildcard(const string_t& str, const string_t& pat, bool ignore_case = true);

inline void str_replace(string_t& data, const string_t& from, const string_t& to)
{
    if (from.empty())
        return;

    size_t i = 0;
    for (;;)
    {
        i = data.find(from, i);
        if (i == string_t::npos)
            break;
        data.replace(i, from.size(), to);
        i += to.size();
    }
}

inline void str_replace(string_t& data, const wchar_t *from, const wchar_t *to)
{
    str_replace(data, string_t(from), string_t(to));
}

template <typename T_STR_CONTAINER>
inline void
str_split(T_STR_CONTAINER& container,
          const typename T_STR_CONTAINER::value_type& str,
          const typename T_STR_CONTAINER::value_type& chars)
{
    container.clear();
    size_t i = 0, k = str.find_first_of(chars);
    while (k != T_STR_CONTAINER::value_type::npos)
    {
        container.push_back(str.substr(i, k - i));
        i = k + 1;
        k = str.find_first_of(chars, i);
    }
    container.push_back(str.substr(i));
}

inline void backslash_to_slash(string_t& string)
{
    for (auto& ch : string)
    {
        if (ch == L'/')
            ch = L'\\';
    }
}

inline void add_backslash(string_t& string)
{
    if (string.size() && string[string.size() - 1] != L'\\')
        string += L'\\';
}

template <typename T_CHAR>
inline void str_trim(std::basic_string<T_CHAR>& str, const T_CHAR *spaces)
{
    typedef std::basic_string<T_CHAR> string_type;
    size_t i = str.find_first_not_of(spaces);
    size_t j = str.find_last_not_of(spaces);
    if ((i == string_type::npos) || (j == string_type::npos))
    {
        str.clear();
    }
    else
    {
        str = str.substr(i, j - i + 1);
    }
}

template <typename T_CHAR, size_t siz>
inline void str_trim(T_CHAR (&str)[siz], const T_CHAR *spaces)
{
    typedef std::basic_string<T_CHAR> string_type;
    string_type s = str;
    str_trim(s, spaces);
    mstrcpy(str, s.c_str());
}

bool templa_validate_filename(string_t& filename);

template <typename T_CHAR>
inline void str_trim_left(std::basic_string<T_CHAR>& str, const T_CHAR *spaces)
{
    typedef std::basic_string<T_CHAR> string_type;
    size_t i = str.find_first_not_of(spaces);
    if (i == string_type::npos)
    {
        str.clear();
    }
    else
    {
        str = str.substr(i);
    }
}

template <typename T_CHAR>
inline void str_trim_right(std::basic_string<T_CHAR>& str, const T_CHAR *spaces)
{
    typedef std::basic_string<T_CHAR> string_type;
    size_t j = str.find_last_not_of(spaces);
    if (j == string_type::npos)
    {
        str.clear();
    }
    else
    {
        str = str.substr(0, j + 1);
    }
}
