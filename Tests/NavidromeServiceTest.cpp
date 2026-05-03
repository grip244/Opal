// Standard C++ tests that compile directly against the source file logic
#include <string>
#include <iostream>
#include <cassert>

namespace Opal {
    class NavidromeService {
    public:
        static std::wstring NormalizeUrlNative(const std::wstring& url);
    };
}

// In the actual Linux sandbox environment, we can't compile the entire NavidromeService.cpp
// due to WinRT dependencies (e.g. ppltasks.h). Instead, the build system (represented here
// by our extraction script) extracts the pure native C++ methods for unit testing.
// The implementation of Opal::NavidromeService::NormalizeUrlNative is linked in from
// the extracted source.

void TestNormalizeUrl() {
    using namespace Opal;

    // Test HTTP default port
    assert(NavidromeService::NormalizeUrlNative(L"http://example.com") == L"http://example.com:4533");

    // Test HTTPS default port
    assert(NavidromeService::NormalizeUrlNative(L"https://example.com") == L"https://example.com:4533");

    // Test missing scheme
    assert(NavidromeService::NormalizeUrlNative(L"example.com") == L"http://example.com:4533");

    // Test existing port
    assert(NavidromeService::NormalizeUrlNative(L"http://example.com:8080") == L"http://example.com:8080");

    // Test trailing slash
    assert(NavidromeService::NormalizeUrlNative(L"http://example.com/") == L"http://example.com:4533/");

    // Test path
    assert(NavidromeService::NormalizeUrlNative(L"example.com/path") == L"http://example.com:4533/path");

    // Test empty
    assert(NavidromeService::NormalizeUrlNative(L"") == L"");

    std::cout << "TestNormalizeUrl passed!" << std::endl;
}

int main() {
    std::cout << "Running NavidromeService tests..." << std::endl;
    TestNormalizeUrl();
    std::cout << "All tests passed!" << std::endl;
    return 0;
}
