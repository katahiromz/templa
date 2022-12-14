/* katahiromz/templa --- Copy files with replacing filenames and contents.
   License: MIT */
#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include "templa.hpp"

const char *templa_get_version(void)
{
    return
        "katahiromz/templa version 0.8.3\n"
        "Copyright (C) 2022 Katayama Hirofumi MZ. All Rights Reserved.\n"
        "License: MIT";
}

const char *templa_get_usage(void)
{
    return
        "templa -- Copy files with replacing filenames and contents\n"
        "\n"
        "Usage: templa [OPTIONS] source1 ... destination\n"
        "\n"
        "  source1 ...   Specify file(s) and/or folder(s).\n"
        "  destination   Specify the destination directory.\n"
        "\n"
        "Options:\n"
        "  --replace FROM TO    Replace strings in filename and file contents.\n"
        "  --ignore \"PATTERN\"   Ignore the wildcard patterns separated by semicolon.\n"
        "                       (default: \"q;*.bin;.git;.svg;.vs\")\n"
        "  --help               Show this message.\n"
        "  --version            Show version information.\n"
        "\n"
        "Contact: Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>";
}

static void templa_version(void)
{
    puts(templa_get_version());
}

static void templa_help(void)
{
    puts(templa_get_usage());
}

static string_t dirname(const string_t& pathname)
{
    size_t ich = pathname.rfind(L'\\');
    if (ich == pathname.npos)
        return L"";
    return pathname.substr(0, ich + 1);
}

static string_t basename(const string_t& pathname)
{
    size_t ich = pathname.rfind(L'\\');
    if (ich == pathname.npos)
        return pathname;
    return pathname.substr(ich + 1);
}

static string_t binary_to_string(UINT codepage, const binary_t& bin)
{
    string_t ret;
    auto cch = MultiByteToWideChar(codepage, 0, bin.data(), INT(bin.size()), NULL, 0);
    if (cch <= 0)
        return ret;

    ret.resize(cch);
    MultiByteToWideChar(codepage, 0, bin.data(), INT(bin.size()), &ret[0], INT(ret.size()));
    return ret;
}

static binary_t string_to_binary(UINT codepage, const string_t& str)
{
    binary_t ret;
    auto cch = WideCharToMultiByte(codepage, 0, str.data(), INT(str.size()), NULL, 0, NULL, NULL);
    if (cch <= 0)
        return ret;

    ret.resize(cch);
    WideCharToMultiByte(codepage, 0, str.data(), INT(str.size()), &ret[0], INT(ret.size()), NULL, NULL);
    return ret;
}

static void str_replace(string_t& data, const string_t& from, const string_t& to)
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

bool templa_wildcard(const string_t& str, const string_t& pat, size_t istr, size_t ipat)
{
    do
    {
        if (ipat == pat.size())
        {
            return istr == str.size();
        }

        if (pat[ipat] == L'?')
        {
            if (istr == str.size())
                return false;

            return templa_wildcard(str, pat, istr + 1, ipat + 1);
        }

        if (pat[ipat] == L'*')
        {
            if (templa_wildcard(str, pat, istr, ipat + 1))
                return true;

            if (istr < str.size() && templa_wildcard(str, pat, istr + 1, ipat))
                return true;

            return false;
        }

        WCHAR ch1 = pat[ipat], ch2 = str[istr];
        ch1 = (WCHAR)(ULONG_PTR)CharUpperW(MAKEINTRESOURCEW(ch1));
        ch2 = (WCHAR)(ULONG_PTR)CharUpperW(MAKEINTRESOURCEW(ch2));
        if (ch1 != ch2)
            return false;

        ++ipat;
        ++istr;
    } while (1);
}

bool templa_wildcard(const string_t& str, const string_t& pat)
{
    return templa_wildcard(str, pat, 0, 0);
}

bool templa_load_file(const string_t& filename, binary_t& data)
{
    if (FILE *fp = _wfopen(filename.c_str(), L"rb"))
    {
        fseek(fp, 0, SEEK_END);
        size_t file_size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        data.resize(file_size);
        bool ok = fread(&data[0], data.size(), 1, fp);
        fclose(fp);

        return ok;
    }
    return false;
}

bool templa_save_file(const string_t& filename, const void *ptr, size_t data_size)
{
    if (FILE *fp = _wfopen(filename.c_str(), L"wb"))
    {
        bool ok = fwrite(ptr, data_size, 1, fp);
        fclose(fp);
        return ok;
    }
    return false;
}

template <typename T_STR_CONTAINER>
void
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

static void swap_endian(void *ptr, size_t size)
{
    auto pw = reinterpret_cast<uint16_t*>(ptr);
    size /= sizeof(uint16_t);
    while (size-- > 0)
    {
        uint16_t w = *pw;
        w = MAKEWORD(HIBYTE(w), LOBYTE(w));
        *pw = w;
    }
}

void TEMPLA_FILE::detect_newline()
{
    if (m_encoding != TE_BINARY)
    {
        if (m_string.find(L"\r\n") != m_string.npos)
        {
            m_newline = TNL_CRLF;
            return;
        }

        if (m_string.find(L"\n") != m_string.npos)
        {
            m_newline = TNL_LF;
            return;
        }

        if (m_string.find(L"\r") != m_string.npos)
        {
            m_newline = TNL_CR;
            return;
        }
    }

    m_newline = TNL_UNKNOWN;
}

static bool binary_is_ascii(const binary_t& binary)
{
    for (auto byte : binary)
    {
        if (byte == 0)
            return false;
        if (byte & 0x80)
            return false;
    }
    return true;
}

static void check_nulls(const binary_t& binary, bool& bUTF16LE, bool& bUTF16BE)
{
    size_t index = 0;
    for (auto ch : binary)
    {
        if (ch == 0)
        {
            if (index & 1)
            {
                bUTF16LE = true;
                if (bUTF16BE)
                    break;
            }
            else
            {
                bUTF16BE = true;
                if (bUTF16LE)
                    break;
            }
        }

        ++index;
        if (index >= binary.size())
            break;
    }
}

void TEMPLA_FILE::detect_encoding()
{
    if (m_binary.size() >= 3 && memcmp(m_binary.data(), "\xEF\xBB\xBF", 3) == 0)
    {
        m_encoding = TE_UTF8;
        m_bom = true;
        m_string = binary_to_string(CP_UTF8, m_binary.substr(3));
        return;
    }
    else if (m_binary.size() >= 2 && memcmp(m_binary.data(), "\xFF\xFE", 2) == 0)
    {
        m_encoding = TE_UTF16;
        m_bom = true;
        m_string.assign(reinterpret_cast<const wchar_t*>(&m_binary[2]),
                        reinterpret_cast<const wchar_t*>(&m_binary[m_binary.size()]));
        return;
    }
    else if (m_binary.size() >= 2 && memcmp(m_binary.data(), "\xFE\xFF", 2) == 0)
    {
        m_encoding = TE_UTF16BE;
        m_bom = true;
        swap_endian(&m_binary[0], m_binary.size());
        m_string.assign(reinterpret_cast<const wchar_t*>(&m_binary[2]),
                        reinterpret_cast<const wchar_t*>(&m_binary[m_binary.size()]));
        return;
    }
    else if (binary_is_ascii(m_binary))
    {
        m_encoding = TE_ASCII;
        m_bom = false;
        m_string = binary_to_string(CP_ACP, m_binary);
        return;
    }
    else
    {
        bool bUTF16LE = false, bUTF16BE = false;
        check_nulls(m_binary, bUTF16LE, bUTF16BE);

        if (bUTF16LE && bUTF16BE)
        {
            m_encoding = TE_BINARY;
            m_string = binary_to_string(CP_ACP, m_binary);
            return;
        }
        else if ((m_binary.size() & 1) == 0)
        {
            auto begin = m_binary.data();
            auto end = m_binary.data() + m_binary.size();
            if (bUTF16LE)
            {
                m_encoding = TE_UTF16;
                m_string.assign(reinterpret_cast<const wchar_t*>(begin), reinterpret_cast<const wchar_t*>(end));
                return;
            }
            else if (bUTF16BE)
            {
                m_encoding = TE_UTF16BE;
                swap_endian(&m_binary[0], m_binary.size());
                m_string.assign(reinterpret_cast<const wchar_t*>(begin), reinterpret_cast<const wchar_t*>(end));
                return;
            }
        }
    }

    bool is_utf8 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, m_binary.data(), -1, NULL, 0) > 0;
    bool is_ansi = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, m_binary.data(), -1, NULL, 0) > 0;

    if (is_utf8 && !is_ansi)
    {
        m_encoding = TE_UTF8;
        m_string = binary_to_string(CP_UTF8, m_binary);
        return;
    }
    else if (!is_utf8 && is_ansi)
    {
        m_encoding = TE_ANSI;
        m_string = binary_to_string(CP_ACP, m_binary);
        return;
    }

    auto utf8 = binary_to_string(CP_UTF8, m_binary);
    if (string_to_binary(CP_UTF8, utf8) == m_binary)
    {
        m_encoding = TE_UTF8;
        m_string = std::move(utf8);
        return;
    }

    auto ansi = binary_to_string(CP_ACP, m_binary);
    if (string_to_binary(CP_ACP, ansi) == m_binary)
    {
        m_encoding = TE_ANSI;
        m_string = std::move(ansi);
        return;
    }

    m_encoding = TE_BINARY;
    m_string = std::move(utf8);
}

bool TEMPLA_FILE::load(const string_t& filename)
{
    if (!templa_load_file(filename, m_binary))
        return false;

    detect_encoding();
    detect_newline();
    return true;
}

bool TEMPLA_FILE::save(const string_t& filename)
{
    if (m_encoding != TE_BINARY)
    {
        switch (m_newline)
        {
        case TNL_CRLF:
            str_replace(m_string, L"\r\n", L"\n");
            str_replace(m_string, L"\n", L"\r\n");
            break;

        case TNL_LF:
            str_replace(m_string, L"\r\n", L"\n");
            str_replace(m_string, L"\r", L"\n");
            break;

        case TNL_CR:
            str_replace(m_string, L"\r\n", L"\r");
            str_replace(m_string, L"\n", L"\r");
            break;

        case TNL_UNKNOWN:
            break;
        }
    }

    switch (m_encoding)
    {
    case TE_BINARY:
        break;

    case TE_UTF8:
        m_binary = string_to_binary(CP_UTF8, m_string);
        break;

    case TE_UTF16:
        m_binary.assign(reinterpret_cast<const uint8_t*>(m_string.data()), reinterpret_cast<const uint8_t*>(&m_string[m_string.size()]));
        break;

    case TE_UTF16BE:
        m_binary.assign(reinterpret_cast<const uint8_t*>(m_string.data()), reinterpret_cast<const uint8_t*>(&m_string[m_string.size()]));
        swap_endian(&m_binary[0], m_binary.size());
        break;

    case TE_ANSI:
    case TE_ASCII:
        m_binary = string_to_binary(CP_ACP, m_string);
        break;
    }

    if (m_bom)
    {
        switch (m_encoding)
        {
        case TE_UTF8:
            m_binary.insert(0, "\xEF\xBB\xBF", 3);
            break;

        case TE_UTF16:
            m_binary.insert(0, "\xFF\xFE", 2);
            break;

        case TE_UTF16BE:
            m_binary.insert(0, "\xFE\xFF", 2);
            break;

        default:
            break;
        }
    }

    return templa_save_file(filename, m_binary.data(), m_binary.size());
}

static TEMPLA_RET
templa_file(string_t& file1, string_t& file2, const mapping_t& mapping,
            const string_list_t& ignore, templa_canceler_t canceler)
{
    if (canceler && canceler())
        return TEMPLA_RET_CANCELED;

    auto basename1 = basename(file1);
    for (auto& ignore_item : ignore)
    {
        if (templa_wildcard(basename1, ignore_item))
        {
            printf("%ls [ignored]\n", file1.c_str());
            return TEMPLA_RET_OK;
        }
    }

    TEMPLA_FILE file;
    if (!file.load(file1))
    {
        fprintf(stderr, "ERROR: Cannot read file '%ls'\n", file1.c_str());
        return TEMPLA_RET_READERROR;
    }

    if (file.m_encoding != TE_BINARY)
    {
        for (auto& pair : mapping)
        {
            str_replace(file.m_string, pair.first, pair.second);
        }
    }

    if (canceler && canceler())
        return TEMPLA_RET_CANCELED;

    {
        const char *type;
        switch (file.m_encoding)
        {
        case TE_BINARY: type = "binary"; break;
        case TE_UTF8: type = "UTF-8"; break;
        case TE_UTF16: type = "UTF-16"; break;
        case TE_UTF16BE: type = "UTF-16 BE"; break;
        case TE_ANSI: type = "ANSI"; break;
        case TE_ASCII: type = "ASCII"; break;
        }
        printf("%ls --> %ls [%s]\n", file1.c_str(), file2.c_str(), type);
    }

    if (!file.save(file2))
    {
        fprintf(stderr, "ERROR: Cannot write file '%ls'\n", file2.c_str());
        return TEMPLA_RET_WRITEERROR;
    }

    return TEMPLA_RET_OK;
}

static void backslash_to_slash(string_t& string)
{
    for (auto& ch : string)
    {
        if (ch == L'/')
            ch = L'\\';
    }
}

static void add_backslash(string_t& string)
{
    if (string.size() && string[string.size() - 1] != L'\\')
        string += L'\\';
}

static TEMPLA_RET
templa_dir(string_t dir1, string_t dir2, const mapping_t& mapping,
           const string_list_t& ignore, templa_canceler_t canceler)
{
    if (canceler && canceler())
        return TEMPLA_RET_CANCELED;

    add_backslash(dir1);
    add_backslash(dir2);

    printf("%ls --> %ls [DIR]\n", dir1.c_str(), dir2.c_str());

    auto spec = dir1;
    spec += L'*';

    WIN32_FIND_DATAW find;
    HANDLE hFind = FindFirstFileW(spec.c_str(), &find);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "ERROR: '%ls': Not a directory\n", dir1.c_str());
        return TEMPLA_RET_READERROR;
    }

    TEMPLA_RET ret = TEMPLA_RET_OK;
    do
    {
        if (canceler && canceler())
        {
            ret = TEMPLA_RET_CANCELED;
            break;
        }

        auto filename1 = find.cFileName;
        if (filename1[0] == L'.')
        {
            if (filename1[1] == 0)
                continue;
            if (filename1[1] == L'.' && filename1[2] == 0)
                continue;
        }

        auto file1 = dir1 + filename1;
        string_t filename2 = filename1;
        for (auto& pair : mapping)
        {
            str_replace(filename2, pair.first, pair.second);
        }
        auto file2 = dir2 + filename2;

        if (find.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!PathIsDirectoryW(file2.c_str()) && !CreateDirectoryW(file2.c_str(), NULL))
            {
                fprintf(stderr, "ERROR: Cannot create folder '%ls'\n", file2.c_str());
                ret = TEMPLA_RET_WRITEERROR;
                break;
            }
            ret = templa_dir(file1, file2, mapping, ignore, canceler);
            if (ret != TEMPLA_RET_OK)
                break;
        }
        else
        {
            ret = templa_file(file1, file2, mapping, ignore, canceler);
            if (ret != TEMPLA_RET_OK)
                break;
        }
    } while (FindNextFileW(hFind, &find));

    FindClose(hFind);
    return ret;
}

TEMPLA_RET
templa(string_t source, string_t destination, const mapping_t& mapping,
       const string_list_t& ignore, templa_canceler_t canceler)
{
    if (canceler && canceler())
        return TEMPLA_RET_CANCELED;

    backslash_to_slash(source);
    backslash_to_slash(destination);

    if (!PathFileExistsW(source.c_str()))
    {
        fprintf(stderr, "ERROR: File '%ls' not found\n", source.c_str());
        return TEMPLA_RET_READERROR;
    }

    if (!PathIsDirectoryW(destination.c_str()))
    {
        fprintf(stderr, "ERROR: '%ls' is not a directory\n", destination.c_str());
        return TEMPLA_RET_WRITEERROR;
    }

    {
        WCHAR szPath1[MAX_PATH], szPath2[MAX_PATH];
        GetFullPathNameW(source.c_str(), _countof(szPath1), szPath1, NULL);
        GetFullPathNameW(destination.c_str(), _countof(szPath2), szPath2, NULL);
        if (PathIsDirectoryW(szPath1))
            PathAddBackslash(szPath1);
        if (PathIsDirectoryW(szPath2))
            PathAddBackslash(szPath2);

        if (lstrcmpiW(szPath1, szPath2) == 0)
        {
            fprintf(stderr, "ERROR: Destination '%ls' is same as source\n", szPath1);
            return TEMPLA_RET_LOGICALERROR;
        }

        string_t src = szPath1, dest = szPath2;
        if (dest.find(src) == 0)
        {
            fprintf(stderr, "ERROR: Source '%ls' contains destination '%ls'\n",
                    src.c_str(), dest.c_str());
            return TEMPLA_RET_LOGICALERROR;
        }
    }

    add_backslash(destination);

    auto dirname1 = dirname(source);
    auto basename1 = basename(source);

    for (auto& ignore_item : ignore)
    {
        if (templa_wildcard(basename1, ignore_item))
        {
            printf("%ls [ignored]\n", source.c_str());
            return TEMPLA_RET_OK;
        }
    }

    auto dirname2 = destination;
    auto basename2 = basename1;
    for (auto& pair : mapping)
    {
        str_replace(basename2, pair.first, pair.second);
    }
    auto file2 = dirname2 + basename2;

    if (PathIsDirectoryW(source.c_str()))
    {
        if (!PathIsDirectoryW(file2.c_str()) && !CreateDirectoryW(file2.c_str(), NULL))
        {
            fprintf(stderr, "ERROR: Cannot create folder '%ls'\n", file2.c_str());
            return TEMPLA_RET_WRITEERROR;
        }
        return templa_dir(source, file2, mapping, ignore, canceler);
    }

    return templa_file(source, file2, mapping, ignore, canceler);
}

TEMPLA_RET
templa_main(int argc, wchar_t **argv)
{
    if (argc <= 1)
    {
        templa_help();
        return TEMPLA_RET_SYNTAXERROR;
    }

    mapping_t mapping;
    std::vector<string_t> files;
    string_list_t ignore;

    str_split(ignore, string_t(L"q;*.bin;.git;.svg;.vs"), string_t(L";"));

    for (int iarg = 1; iarg < argc; ++iarg)
    {
        string_t arg = argv[iarg];

        if (arg == L"--help")
        {
            templa_help();
            return TEMPLA_RET_OK;
        }

        if (arg == L"--version")
        {
            templa_version();
            return TEMPLA_RET_OK;
        }

        if (arg == L"--replace")
        {
            if (iarg + 2 < argc)
            {
                auto from = argv[iarg + 1], to = argv[iarg + 2];
                mapping[from] = to;
                iarg += 2;
                continue;
            }
            else
            {
                fprintf(stderr, "ERROR: Option '--replace' requires two arguments\n");
                return TEMPLA_RET_SYNTAXERROR;
            }
        }

        if (arg == L"--ignore")
        {
            if (iarg + 1 < argc)
            {
                str_split(ignore, string_t(argv[iarg + 1]), string_t(L";"));
                iarg += 1;
                continue;
            }
            else
            {
                fprintf(stderr, "ERROR: Option '--ignore' requires one argument\n");
                return TEMPLA_RET_SYNTAXERROR;
            }
        }

        if (arg[0] == L'-')
        {
            fprintf(stderr, "ERROR: '%ls' is invalid option\n", arg.c_str());
            return TEMPLA_RET_SYNTAXERROR;
        }

        files.push_back(arg);
    }

    if (files.size() <= 1)
    {
        fprintf(stderr, "ERROR: Specify two or more files\n");
        return TEMPLA_RET_SYNTAXERROR;
    }

    size_t iLast = files.size() - 1;
    auto& destination = files[iLast];
    for (size_t i = 0; i < iLast; ++i)
    {
        TEMPLA_RET ret = templa(files[i], destination, mapping, ignore);
        if (ret != TEMPLA_RET_OK)
            return ret;
    }

    return TEMPLA_RET_OK;
}
