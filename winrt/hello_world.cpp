#include <winrt/Windows.Foundation.h>
#include <iostream>

using namespace winrt;
using namespace Windows::Foundation;

int main()
{
    // Initialize the Windows Runtime
    init_apartment();

    // Print Hello World
    std::cout << "Hello, World from WinRT!" << std::endl;

    // Demonstrate a simple WinRT API call
    Uri uri(L"https://microsoft.com");
    std::wcout << L"Created URI: " << uri.ToString().c_str() << std::endl;

    return 0;
} 