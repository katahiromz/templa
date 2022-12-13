/* katahiromz/templa --- Copy files with replacing filenames and contents.
   License: MIT */
#include <windows.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <regex>
#include <cstdint>

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

typedef std::vector<string_t> patterns_t;

void templa_version(void)
{
    printf(
        "katahiromz/templa version 0.1\n"
        "Copyright (C) 2022 Katayama Hirofumi MZ. All Rights Reserved.\n"
        "License: MIT\n");
}

void templa_help(void)
{
    printf(
        "templa -- Copy files with replacing filenames and contents\n"
        "\n"
        "Usage: templa [OPTIONS] \"file1\" \"file2\"\n"
        "\n"
        "Options:\n"
        "  --replace FROM TO    Replace strings in filename and file contents.\n"
        "  --exclude \"PATTERN\"  Exclude the wildcard patterns separated by semicolon.\n"
        "                       (default: \"q;*.bin;.git;.svg;.vs\")\n"
        "  --help               Show this message.\n"
        "  --version            Show version information.\n"
        "\n"
        "Contact: Katayama Hirofumi MZ <katayama.hirofumi.mz@gmail.com>\n");
}

string_t dirname(const string_t& pathname)
{
    size_t ich = pathname.find(L'\\');
    if (ich == pathname.npos)
        return L"";
    return pathname.substr(0, ich + 1);
}

string_t basename(const string_t& pathname)
{
    size_t ich = pathname.find(L'\\');
    if (ich == pathname.npos)
        return pathname;
    return pathname.substr(ich + 1);
}

string_t binary_to_string(UINT codepage, const binary_t& bin)
{
    string_t ret;
    auto cch = MultiByteToWideChar(codepage, 0, bin.data(), INT(bin.size()), NULL, 0);
    if (cch <= 0)
        return ret;

    ret.resize(cch);
    MultiByteToWideChar(codepage, 0, bin.data(), INT(bin.size()), &ret[0], INT(ret.size()));
    return ret;
}

binary_t string_to_binary(UINT codepage, const string_t& str)
{
    binary_t ret;
    auto cch = WideCharToMultiByte(codepage, 0, str.data(), INT(str.size()), NULL, 0, NULL, NULL);
    if (cch <= 0)
        return ret;

    ret.resize(cch);
    WideCharToMultiByte(codepage, 0, str.data(), INT(str.size()), &ret[0], INT(ret.size()), NULL, NULL);
    return ret;
}

void str_replace(string_t& data, const string_t& from, const string_t& to)
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

bool wildcard_match(const string_t& str, const string_t& pattern)
{
    string_t pat = L"^" + pattern + L"$";
    str_replace(pat, L"?", L".");
    str_replace(pat, L"*", L".*");
    std::wregex regex(pat);
    return std::regex_match(str, regex);
}

bool load_file(const string_t& filename, binary_t& data)
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

bool save_file(const string_t& filename, const void *ptr, size_t data_size)
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

void swap_endian(void *ptr, size_t size)
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

bool detect_newline(const string_t& string, ENCODING& encoding)
{
    if (string.find(L"\r\n") != string.npos)
    {
        encoding.newline = EN_CRLF;
        return true;
    }

    if (string.find(L"\n") != string.npos)
    {
        encoding.newline = EN_LF;
        return true;
    }

    if (string.find(L"\r") != string.npos)
    {
        encoding.newline = EN_CR;
        return true;
    }

    encoding.newline = EN_UNKNOWN;
    return true;
}

bool binary_is_ascii(const binary_t& binary)
{
    for (auto& byte : binary)
    {
        if (byte & 0x80)
            return false;
    }
    return true;
}

void check_nulls(const binary_t& binary, bool& bUTF16LE, bool& bUTF16BE)
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

bool detect_encoding(string_t& string, binary_t& binary, ENCODING& encoding)
{
    if (binary.size() >= 3 && memcmp(binary.data(), "\xEF\xBB\xBF", 3) == 0)
    {
        encoding.type = ET_UTF8;
        encoding.bom = true;
        string = binary_to_string(CP_UTF8, binary.substr(3));
        return detect_newline(string, encoding);
    }
    else if (binary.size() >= 2 && memcmp(binary.data(), "\xFF\xFE", 2) == 0)
    {
        encoding.type = ET_UTF16;
        encoding.bom = true;
        string.assign(reinterpret_cast<const wchar_t*>(&binary[2]), reinterpret_cast<const wchar_t*>(&binary[binary.size()]));
        return detect_newline(string, encoding);
    }
    else if (binary.size() >= 2 && memcmp(binary.data(), "\xFE\xFF", 2) == 0)
    {
        encoding.type = ET_UTF16BE;
        encoding.bom = true;
        swap_endian(&binary[0], binary.size());
        string.assign(reinterpret_cast<const wchar_t*>(&binary[2]), reinterpret_cast<const wchar_t*>(&binary[binary.size()]));
        return detect_newline(string, encoding);
    }
    else if (binary_is_ascii(binary))
    {
        encoding.type = ET_ASCII;
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
            encoding.type = ET_BINARY;
            string = binary_to_string(CP_ACP, binary);
            return detect_newline(string, encoding);
        }
        else if (bUTF16LE)
        {
            encoding.type = ET_UTF16;
            string.assign(reinterpret_cast<const wchar_t*>(binary.data()), reinterpret_cast<const wchar_t*>(binary.data() + binary.size()));
            return detect_newline(string, encoding);
        }
        else if (bUTF16BE)
        {
            encoding.type = ET_UTF16BE;
            swap_endian(&binary[0], binary.size());
            string.assign(reinterpret_cast<const wchar_t*>(binary.data()), reinterpret_cast<const wchar_t*>(binary.data() + binary.size()));
            return detect_newline(string, encoding);
        }
    }

    bool is_utf8 = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, binary.data(), -1, NULL, 0) > 0;
    bool is_ansi = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, binary.data(), -1, NULL, 0) > 0;

    if (is_utf8 && !is_ansi)
    {
        encoding.type = ET_UTF8;
        string = binary_to_string(CP_UTF8, binary);
        return detect_newline(string, encoding);
    }
    else if (!is_utf8 && is_ansi)
    {
        encoding.type = ET_ANSI;
        string = binary_to_string(CP_ACP, binary);
        return detect_newline(string, encoding);
    }

    auto utf8 = binary_to_string(CP_UTF8, binary);
    if (string_to_binary(CP_UTF8, utf8) == binary)
    {
        encoding.type = ET_UTF8;
        string = std::move(utf8);
        return detect_newline(string, encoding);
    }

    auto ansi = binary_to_string(CP_ACP, binary);
    if (string_to_binary(CP_ACP, ansi) == binary)
    {
        encoding.type = ET_ANSI;
        string = std::move(ansi);
        return detect_newline(string, encoding);
    }

    encoding.type = ET_BINARY;
    string = binary_to_string(CP_ACP, binary);
    return detect_newline(string, encoding);
}

bool load_file_with_encoding(const string_t& filename, string_t& string, ENCODING& encoding)
{
    binary_t binary;
    if (!load_file(filename, binary))
        return false;

    detect_encoding(string, binary, encoding);
    return true;
}

bool save_file_with_encoding(const string_t& filename, string_t& string, ENCODING& encoding)
{
    switch (encoding.newline)
    {
    case EN_CRLF:
    case EN_UNKNOWN:
        str_replace(string, L"\r\n", L"\n");
        str_replace(string, L"\n", L"\r\n");
        break;
    case EN_LF:
        str_replace(string, L"\r\n", L"\n");
        str_replace(string, L"\r", L"\n");
        break;
    case EN_CR:
        str_replace(string, L"\r\n", L"\r");
        str_replace(string, L"\n", L"\r");
        break;
    }

    binary_t binary;
    switch (encoding.type)
    {
    case ET_BINARY:
    case ET_UTF8:
        binary = std::move(string_to_binary(CP_UTF8, string));
        break;
    case ET_UTF16:
        binary.assign(reinterpret_cast<const uint8_t*>(string.data()), reinterpret_cast<const uint8_t*>(&string[string.size()]));
        break;
    case ET_UTF16BE:
        binary.assign(reinterpret_cast<const uint8_t*>(string.data()), reinterpret_cast<const uint8_t*>(&string[string.size()]));
        swap_endian(&binary[0], binary.size());
        break;
    case ET_ANSI:
    case ET_ASCII:
        binary = std::move(string_to_binary(CP_ACP, string));
        break;
    }

    if (encoding.bom)
    {
        switch (encoding.type)
        {
        case ET_UTF8:
            binary.insert(0, "\xEF\xBB\xBF", 3);
            break;
        case ET_UTF16:
            binary.insert(0, "\xFF\xFE", 2);
            break;
        case ET_UTF16BE:
            binary.insert(0, "\xFE\xFF", 2);
            break;
        default:
            break;
        }
    }

    return save_file(filename, binary.data(), binary.size());
}

int templa_file(string_t& file1, string_t& file2, const mapping_t& mapping, const patterns_t& exclude)
{
    string_t string;
    ENCODING encoding;

    auto basename1 = basename(file1);
    for (auto& exclude_item : exclude)
    {
        if (wildcard_match(basename1, exclude_item))
            return 0;
    }

    if (!load_file_with_encoding(file1, string, encoding))
    {
        fprintf(stderr, "ERROR: Cannot read file '%s'\n", file1.c_str());
        return 1;
    }

    if (encoding.type != ET_BINARY)
    {
        for (auto& pair : mapping)
        {
            str_replace(string, pair.first, pair.second);
        }
    }

    {
        const char *type;
        switch (encoding.type)
        {
        case ET_BINARY: type = "binary"; break;
        case ET_UTF8: type = "UTF-8"; break;
        case ET_UTF16: type = "UTF-16"; break;
        case ET_UTF16BE: type = "UTF-16 BE"; break;
        case ET_ANSI: type = "ANSI"; break;
        case ET_ASCII: type = "ASCII"; break;
        }
        printf("%ls --> %ls [%s]\n", file1.c_str(), file2.c_str(), type);
    }

    if (!save_file_with_encoding(file2, string, encoding))
    {
        fprintf(stderr, "ERROR: Cannot write file '%s'\n", file2.c_str());
        return 1;
    }

    return 0;
}

void backslash_to_slash(string_t& string)
{
    for (auto& ch : string)
    {
        if (ch == L'/')
            ch = L'\\';
    }
}

void add_backslash(string_t& string)
{
    if (string.size() && string[string.size() - 1] != L'\\')
        string += L'\\';
}

int templa_dir(string_t dir1, string_t dir2, const mapping_t& mapping, const patterns_t& exclude)
{
    add_backslash(dir1);
    add_backslash(dir2);

    printf("%ls --> %ls [DIR]\n", dir1.c_str(), dir2.c_str());

    auto spec = dir1;
    spec += L'*';

    WIN32_FIND_DATAW find;
    HANDLE hFind = FindFirstFileW(spec.c_str(), &find);
    if (hFind == INVALID_HANDLE_VALUE)
        return 0;

    int ret = 0;
    do
    {
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
            if (!CreateDirectoryW(file2.c_str(), NULL))
            {
                fprintf(stderr, "ERROR: Cannot create folder '%s'\n", file2.c_str());
                ret = 1;
                break;
            }
            if (templa_dir(file1, file2, mapping, exclude) != 0)
            {
                ret = 1;
                break;
            }
        }
        else
        {
            if (templa_file(file1, file2, mapping, exclude) != 0)
            {
                ret = 1;
                break;
            }
        }
    } while (FindNextFileW(hFind, &find));

    FindClose(hFind);
    return ret;
}

int templa(string_t file1, string_t file2, const mapping_t& mapping, const patterns_t& exclude)
{
    backslash_to_slash(file1);
    backslash_to_slash(file2);

    if (!PathFileExistsW(file1.c_str()))
    {
        fprintf(stderr, "ERROR: File '%s' not found\n", file1.c_str());
        return 1;
    }

    auto dirname1 = dirname(file1);
    auto basename1 = basename(file1);

    for (auto& exclude_item : exclude)
    {
        if (wildcard_match(basename1, exclude_item))
            return 0;
    }

    auto dirname2 = dirname(file2);
    auto basename2 = basename(file2);

    for (auto& pair : mapping)
    {
        str_replace(basename2, pair.first, pair.second);
    }

    file2 = dirname2 + basename2;

    if (PathIsDirectoryW(file1.c_str()))
    {
        if (!CreateDirectoryW(file2.c_str(), NULL))
        {
            fprintf(stderr, "ERROR: Cannot create folder '%s'\n", file2.c_str());
            return 1;
        }
        return templa_dir(file1, file2, mapping, exclude);
    }

    return templa_file(file1, file2, mapping, exclude);
}

extern "C"
int __cdecl wmain(int argc, wchar_t **argv)
{
    if (argc <= 1)
    {
        templa_help();
        return 1;
    }

    mapping_t mapping;
    string_t file1, file2;
    patterns_t exclude;

    str_split(exclude, string_t(L"q;*.bin;.git;.svg;.vs"), string_t(L";"));

    for (int iarg = 1; iarg < argc; ++iarg)
    {
        string_t arg = argv[iarg];

        if (arg == L"--help")
        {
            templa_help();
            return 1;
        }

        if (arg == L"--version")
        {
            templa_version();
            return 1;
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
                return 1;
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
                return 1;
            }
        }

        if (file1.empty())
        {
            file1 = arg;
        }
        else if (file2.empty())
        {
            file2 = arg;
        }
        else
        {
            fprintf(stderr, "ERROR: Too many files\n");
            return 1;
        }
    }

    if (file1.empty())
    {
        fprintf(stderr, "ERROR: Specify two files\n");
        return 1;
    }

    if (file2.empty())
    {
        fprintf(stderr, "ERROR: Specify one more file\n");
        return 1;
    }

    return templa(file1, file2, mapping, exclude);
}

#if defined(__MINGW32__) || defined(__clang__)
int main(void)
{
    INT argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    INT ret = wmain(argc, argv);
    LocalFree(argv);
    return ret;
}
#endif
