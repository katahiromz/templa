#include <windows.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdio>
#include <cassert>
#include "../templa.hpp"

int main(void)
{
    assert(templa_wildcard(L"", L""));
    assert(!templa_wildcard(L"", L"?"));
    assert(templa_wildcard(L"", L"*"));

    assert(!templa_wildcard(L"a", L""));
    assert(templa_wildcard(L"a", L"?"));
    assert(templa_wildcard(L"a", L"*"));

    assert(templa_wildcard(L"same", L"same"));
    assert(!templa_wildcard(L"not", L"same"));

    assert(templa_wildcard(L"abc", L"abc"));
    assert(templa_wildcard(L"abc", L"ABC"));
    assert(templa_wildcard(L"ABC", L"abc"));
    assert(templa_wildcard(L"ABC", L"ABC"));

    assert(templa_wildcard(L"abc", L"a*"));
    assert(templa_wildcard(L"abc", L"A*"));
    assert(templa_wildcard(L"ABC", L"a*"));
    assert(templa_wildcard(L"ABC", L"A*"));

    assert(templa_wildcard(L"abc", L"ab?"));
    assert(templa_wildcard(L"abc", L"AB?"));
    assert(templa_wildcard(L"ABC", L"ab?"));
    assert(templa_wildcard(L"ABC", L"AB?"));

    assert(templa_wildcard(L"abc", L"a??"));
    assert(templa_wildcard(L"abc", L"A??"));
    assert(templa_wildcard(L"ABC", L"a??"));
    assert(templa_wildcard(L"ABC", L"A??"));

    assert(templa_wildcard(L"abc", L"*c"));
    assert(templa_wildcard(L"abc", L"*C"));
    assert(templa_wildcard(L"ABC", L"*c"));
    assert(templa_wildcard(L"ABC", L"*C"));

    assert(!templa_wildcard(L"abc", L"abc?"));
    assert(!templa_wildcard(L"abc", L"ABC?"));
    assert(!templa_wildcard(L"ABC", L"abc?"));
    assert(!templa_wildcard(L"ABC", L"ABC?"));

    assert(!templa_wildcard(L"abc", L"?abc"));
    assert(!templa_wildcard(L"abc", L"?ABC"));
    assert(!templa_wildcard(L"ABC", L"?abc"));
    assert(!templa_wildcard(L"ABC", L"?ABC"));

    assert(templa_wildcard(L"abc", L"a*c"));
    assert(templa_wildcard(L"abc", L"A*C"));
    assert(templa_wildcard(L"ABC", L"a*c"));
    assert(templa_wildcard(L"ABC", L"A*C"));

    puts("OK");
    return 0;
}
