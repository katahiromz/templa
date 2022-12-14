/* katahiromz/templa --- Copy files with replacing filenames and contents.
   License: MIT */
#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <regex>
#include <cstdint>
#include <unordered_map>
#include "templa.hpp"

const char *templa_get_version(void)
{
    return
        "katahiromz/templa version 0.7\n"
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
        "  --exclude \"PATTERN\"  Exclude the wildcard patterns separated by semicolon.\n"
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

static bool wildcard_match(const string_t& str, const string_t& pattern)
{
    string_t pat = L"^" + pattern + L"$";
    str_replace(pat, L"?", L".");
    str_replace(pat, L"*", L".*");
    std::wregex regex(pat);
    return std::regex_match(str, regex);
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

static bool detect_newline(const string_t& string, TEMPLA_ENCODING& encoding)
{
    if (string.find(L"\r\n") != string.npos)
    {
        encoding.newline = TNL_CRLF;
        return true;
    }

    if (string.find(L"\n") != string.npos)
    {
        encoding.newline = TNL_LF;
        return true;
    }

    if (string.find(L"\r") != string.npos)
    {
        encoding.newline = TNL_CR;
        return true;
    }

    encoding.newline = TNL_UNKNOWN;
    return true;
}

static bool binary_is_ascii(const binary_t& binary)
{
    for (auto& byte : binary)
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

static bool detect_encoding(string_t& string, binary_t& binary, TEMPLA_ENCODING& encoding)
{
    if (binary.size() >= 3 && memcmp(binary.data(), "\xEF\xBB\xBF", 3) == 0)
    {
        encoding.type = TET_UTF8;
        encoding.bom = true;
        string = binary_to_string(CP_UTF8, binary.substr(3));
        return detect_newline(string, encoding);
    }
    else if (binary.size() >= 2 && memcmp(binary.data(), "\xFF\xFE", 2) == 0)
    {
        encoding.type = TET_UTF16;
        encoding.bom = true;
        string.assign(reinterpret_cast<const wchar_t*>(&binary[2]), reinterpret_cast<const wchar_t*>(&binary[binary.size()]));
        return detect_newline(string, encoding);
    }
    else if (binary.size() >= 2 && memcmp(binary.data(), "\xFE\xFF", 2) == 0)
    {
        encoding.type = TET_UTF16BE;
        encoding.bom = true;
        swap_endian(&binary[0], binary.size());
        string.assign(reinterpret_cast<const wchar_t*>(&binary[2]), reinterpret_cast<const wchar_t*>(&binary[binary.size()]));
        return detect_newline(string, encoding);
    }
    else if (binary_is_ascii(binary))
    {
        encoding.type = TET_ASCII;
        encoding.bom = false;
        string = binary_to_string(CP_ACP, binary);
        return detect_newline(string, encoding);
    }
    else
    {
        bool bUTF16LE = false, bUTF16BE = false;
        check_nulls(binary, bUTF16LE, bUTF16BE);

        if (bUTF16LE && bUTF16BE)
        {
            encoding.type = TET_BINARY;
            string = binary_to_string(CP_ACP, binary);
            return detect_newline(string, encoding);
        }
        else if (bUTF16LE)
        {
            encoding.type = TET_UTF16;
            string.assign(reinterpret_cast<const wchar_t*>(binary.data()), reinterpret_cast<const wchar_t*>(binary.data() + binary.size()));
            return detect_newline(string, encoding);
        }
        else if (bUTF16BE)
        {
            encoding.type = TET_UTF16BE;
            swap_endian(&binary[0], binary.size());
            string.assign(reinterpret_cast<const wchar_t*>(binary.data()), reinterpret_cast<const wchar_t*>(binary.data() + binary.size()));
            return detect_newline(string, encoding);
        }
    }

    bool is_utf8 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, binary.data(), -1, NULL, 0) > 0;
    bool is_ansi = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, binary.data(), -1, NULL, 0) > 0;

    if (is_utf8 && !is_ansi)
    {
        encoding.type = TET_UTF8;
        string = binary_to_string(CP_UTF8, binary);
        return detect_newline(string, encoding);
    }
    else if (!is_utf8 && is_ansi)
    {
        encoding.type = TET_ANSI;
        string = binary_to_string(CP_ACP, binary);
        return detect_newline(string, encoding);
    }

    auto utf8 = binary_to_string(CP_UTF8, binary);
    if (string_to_binary(CP_UTF8, utf8) == binary)
    {
        encoding.type = TET_UTF8;
        string = std::move(utf8);
        return detect_newline(string, encoding);
    }

    auto ansi = binary_to_string(CP_ACP, binary);
    if (string_to_binary(CP_ACP, ansi) == binary)
    {
        encoding.type = TET_ANSI;
        string = std::move(ansi);
        return detect_newline(string, encoding);
    }

    encoding.type = TET_BINARY;
    string = std::move(utf8);
    return detect_newline(string, encoding);
}

bool templa_load_file_ex(const string_t& filename, binary_t& binary, string_t& string, TEMPLA_ENCODING& encoding)
{
    if (!templa_load_file(filename, binary))
        return false;

    detect_encoding(string, binary, encoding);
    return true;
}

bool templa_save_file_ex(const string_t& filename, binary_t& binary, string_t& string, const TEMPLA_ENCODING& encoding)
{
    if (encoding.type != TET_BINARY)
    {
        switch (encoding.newline)
        {
        case TNL_CRLF:
        case TNL_UNKNOWN:
            str_replace(string, L"\r\n", L"\n");
            str_replace(string, L"\n", L"\r\n");
            break;

        case TNL_LF:
            str_replace(string, L"\r\n", L"\n");
            str_replace(string, L"\r", L"\n");
            break;

        case TNL_CR:
            str_replace(string, L"\r\n", L"\r");
            str_replace(string, L"\n", L"\r");
            break;
        }
    }

    switch (encoding.type)
    {
    case TET_BINARY:
        break;

    case TET_UTF8:
        binary = std::move(string_to_binary(CP_UTF8, string));
        break;

    case TET_UTF16:
        binary.assign(reinterpret_cast<const uint8_t*>(string.data()), reinterpret_cast<const uint8_t*>(&string[string.size()]));
        break;

    case TET_UTF16BE:
        binary.assign(reinterpret_cast<const uint8_t*>(string.data()), reinterpret_cast<const uint8_t*>(&string[string.size()]));
        swap_endian(&binary[0], binary.size());
        break;

    case TET_ANSI:
    case TET_ASCII:
        binary = std::move(string_to_binary(CP_ACP, string));
        break;
    }

    if (encoding.type != TET_BINARY && encoding.bom)
    {
        switch (encoding.type)
        {
        case TET_UTF8:
            binary.insert(0, "\xEF\xBB\xBF", 3);
            break;

        case TET_UTF16:
            binary.insert(0, "\xFF\xFE", 2);
            break;

        case TET_UTF16BE:
            binary.insert(0, "\xFE\xFF", 2);
            break;

        default:
            break;
        }
    }

    return templa_save_file(filename, binary.data(), binary.size());
}

static TEMPLA_RET
templa_file(string_t& file1, string_t& file2, const mapping_t& mapping,
            const string_list_t& exclude, templa_canceler_t canceler)
{
    string_t string;
    TEMPLA_ENCODING encoding;

    if (canceler && canceler())
        return TEMPLA_RET_CANCELED;

    auto basename1 = basename(file1);
    for (auto& exclude_item : exclude)
    {
        if (wildcard_match(basename1, exclude_item))
            return TEMPLA_RET_OK;
    }

    binary_t binary;
    if (!templa_load_file_ex(file1, binary, string, encoding))
    {
        fprintf(stderr, "ERROR: Cannot read file '%ls'\n", file1.c_str());
        return TEMPLA_RET_READERROR;
    }

    if (encoding.type != TET_BINARY)
    {
        for (auto& pair : mapping)
        {
            str_replace(string, pair.first, pair.second);
        }
    }

    if (canceler && canceler())
        return TEMPLA_RET_CANCELED;

    {
        const char *type;
        switch (encoding.type)
        {
        case TET_BINARY: type = "binary"; break;
        case TET_UTF8: type = "UTF-8"; break;
        case TET_UTF16: type = "UTF-16"; break;
        case TET_UTF16BE: type = "UTF-16 BE"; break;
        case TET_ANSI: type = "ANSI"; break;
        case TET_ASCII: type = "ASCII"; break;
        }
        printf("%ls --> %ls [%s]\n", file1.c_str(), file2.c_str(), type);
    }

    if (!templa_save_file_ex(file2, binary, string, encoding))
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
           const string_list_t& exclude, templa_canceler_t canceler)
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
            ret = templa_dir(file1, file2, mapping, exclude, canceler);
            if (ret != TEMPLA_RET_OK)
                break;
        }
        else
        {
            ret = templa_file(file1, file2, mapping, exclude, canceler);
            if (ret != TEMPLA_RET_OK)
                break;
        }
    } while (FindNextFileW(hFind, &find));

    FindClose(hFind);
    return ret;
}

TEMPLA_RET
templa(string_t source, string_t destination, const mapping_t& mapping,
       const string_list_t& exclude, templa_canceler_t canceler)
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

    for (auto& exclude_item : exclude)
    {
        if (wildcard_match(basename1, exclude_item))
            return TEMPLA_RET_OK;
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
        return templa_dir(source, file2, mapping, exclude, canceler);
    }

    return templa_file(source, file2, mapping, exclude, canceler);
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
    string_list_t exclude;

    str_split(exclude, string_t(L"q;*.bin;.git;.svg;.vs"), string_t(L";"));

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

        if (arg == L"--exclude")
        {
            if (iarg + 1 < argc)
            {
                str_split(exclude, string_t(argv[iarg + 1]), string_t(L";"));
                iarg += 1;
                continue;
            }
            else
            {
                fprintf(stderr, "ERROR: Option '--exclude' requires one argument\n");
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
        TEMPLA_RET ret = templa(files[i], destination, mapping, exclude);
        if (ret != TEMPLA_RET_OK)
            return ret;
    }

    return TEMPLA_RET_OK;
}
