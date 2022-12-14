/* katahiromz/templa --- Copy files with replacing filenames and contents.
   License: MIT */
#include <windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include "templa.hpp"

extern "C"
int __cdecl wmain(int argc, wchar_t **argv)
{
    return templa_main(argc, argv);
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
