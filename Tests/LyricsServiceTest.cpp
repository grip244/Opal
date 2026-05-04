#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <algorithm>
#include <sstream>
#include <regex>
#include <cstdint>
#include "Services/LyricsServiceInternal.h"

using namespace Opal::Services;

void TestParseLrc_Timed() {
    std::wstring lrc = L"[00:01.00]First line\n[00:02.50]Second line";
    NativeLyricsResult result = LyricsParser::ParseLrc(lrc);

    assert(result.IsTimed == true);
    assert(result.Lines.size() == 2);
    assert(result.Lines[0].TimestampMs == 1000);
    assert(result.Lines[0].Text == L"First line");
    assert(result.Lines[1].TimestampMs == 2500);
    assert(result.Lines[1].Text == L"Second line");
    std::cout << "TestParseLrc_Timed passed." << std::endl;
}

void TestParseLrc_Untimed() {
    std::wstring lrc = L"First line\nSecond line\n\nThird line";
    NativeLyricsResult result = LyricsParser::ParseLrc(lrc);

    assert(result.IsTimed == false);
    assert(result.Lines.size() == 3);
    assert(result.Lines[0].TimestampMs == 0);
    assert(result.Lines[0].Text == L"First line");
    assert(result.Lines[1].TimestampMs == 0);
    assert(result.Lines[1].Text == L"Second line");
    assert(result.Lines[2].TimestampMs == 0);
    assert(result.Lines[2].Text == L"Third line");
    std::cout << "TestParseLrc_Untimed passed." << std::endl;
}

void TestParseLrc_Empty() {
    std::wstring lrc = L"";
    NativeLyricsResult result = LyricsParser::ParseLrc(lrc);

    assert(result.IsTimed == false);
    assert(result.Lines.size() == 0);
    std::cout << "TestParseLrc_Empty passed." << std::endl;
}

void TestParseLrc_Mixed() {
    std::wstring lrc = L"[00:01.00]Timed line\nUntimed line";
    NativeLyricsResult result = LyricsParser::ParseLrc(lrc);

    assert(result.IsTimed == true);
    assert(result.Lines.size() == 1);
    assert(result.Lines[0].Text == L"Timed line");
    std::cout << "TestParseLrc_Mixed passed." << std::endl;
}

int main() {
    std::cout << "Running LyricsService tests..." << std::endl;
    TestParseLrc_Timed();
    TestParseLrc_Untimed();
    TestParseLrc_Empty();
    TestParseLrc_Mixed();
    std::cout << "All LyricsService tests passed!" << std::endl;
    return 0;
}
