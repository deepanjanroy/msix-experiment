#include <windows.h>
#include <userenv.h>
#include <iostream>

#pragma comment(lib, "userenv.lib")

int main() {
    HRESULT hr;
    PSID sid = nullptr;

    LPCWSTR name = L"MyAppContainer";
    LPCWSTR displayName = L"My App Container";
    LPCWSTR description = L"A test AppContainer profile";
    
    // Needs to run as admin
    hr = CreateAppContainerProfile(name, displayName, description, nullptr, 0, &sid);
    
    if (SUCCEEDED(hr)) {
        std::wcout << L"AppContainer profile created successfully." << std::endl;
    } else if (hr == HRESULT_FROM_WIN32(ERROR_ALREADY_EXISTS)) {
        std::wcout << L"Profile already exists." << std::endl;
    } else {
        std::wcout << L"Failed to create AppContainer. HRESULT: " << std::hex << hr << std::endl;
    }

    if (sid != nullptr) {
        FreeSid(sid);
    }

    return 0;
}
