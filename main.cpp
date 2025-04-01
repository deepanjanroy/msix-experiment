#include <iostream>
#include <filesystem>
#include <string>
#include <thread>
#include <chrono>
#include <windows.h>
#include <appmodel.h>
#include <shlobj.h>

namespace fs = std::filesystem;

bool IsRunningAsUwp() {
    // Method 1: Check if GetCurrentPackageFullName is available and returns a package name
    UINT32 length = 0;
    LONG result = GetCurrentPackageFullName(&length, nullptr);
    
    if (result == APPMODEL_ERROR_NO_PACKAGE) {
        return false;  // Not running as UWP/Desktop Bridge
    }
    
    // Method 2: Check for UWP-specific environment variable
    char* uwpPath = nullptr;
    size_t pathSize;
    _dupenv_s(&uwpPath, &pathSize, "PACKAGE_FAMILY_NAME");
    if (uwpPath != nullptr) {
        free(uwpPath);
        return true;
    }

    // Method 3: Check for Windows Apps directory
    wchar_t programFilesPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES | CSIDL_FLAG_CREATE,
                                  nullptr, 0, programFilesPath))) {
        std::wstring windowsAppsPath = std::wstring(programFilesPath) + L"\\WindowsApps";
        if (GetFileAttributesW(windowsAppsPath.c_str()) != INVALID_FILE_ATTRIBUTES) {
            return true;
        }
    }

    return false;
}

bool IsRunningAsAppContainer() {
    HANDLE tokenHandle{};
    DWORD isAppContainer{};
    DWORD tokenInformationLength{ sizeof(DWORD) };
    if (!::OpenProcessToken(
        GetCurrentProcess(),
        TOKEN_QUERY,
        &tokenHandle)) {
        return false;
    }

    if (!::GetTokenInformation(
        tokenHandle,
        TOKEN_INFORMATION_CLASS::TokenIsAppContainer,
        &isAppContainer,
        tokenInformationLength,
        &tokenInformationLength
    )) {
        return false;
    }

    return isAppContainer != 0;
}


int main() {    
    if (IsRunningAsUwp()) {
        std::cout << "Application is running as Desktop Bridge/UWP" << std::endl;
    } else {
        std::cout << "Application is running as classic Win32" << std::endl;
    }

    if (IsRunningAsAppContainer()) {
        std::cout << "Application is running as AppContainer" << std::endl;
    } else {
        std::cout << "Application is running as classic Win32" << std::endl;
    }
    
    std::string directory;
    std::cout << "Enter directory path: ";
    std::getline(std::cin, directory);
    std::cout << "Attempting to access: " << directory << std::endl;

    // Print out the absolute path we're trying to access
    try {
        std::cout << "Absolute path: " << fs::absolute(directory) << std::endl;
    } catch (const fs::filesystem_error& e) {
        std::cout << "Error getting absolute path: " << e.what() << std::endl;
    }

    // // Check parent directory access
    // fs::path parentPath = fs::path(directory).parent_path();
    // std::error_code parentEc;
    // if (!fs::exists(parentPath, parentEc)) {
    //     std::cout << "Parent directory access failed. Error: " << parentEc.message() << std::endl;
    // }

    try {
        if (!fs::exists(directory)) {
            std::cout << "Directory does not exist." << std::endl;
            std::cout << "Attempted path: " << fs::absolute(directory) << std::endl;
            std::cin.get();
            return 1;
        }
    } catch (const fs::filesystem_error& e) {
        std::cout << "Error checking directory: " << e.what() << std::endl;
        std::cout << "Attempted path: " << fs::absolute(directory) << std::endl;
        std::cin.get();
        return 1;
    }

    std::cout << "Directory exists." << std::endl;

    // List all files in the specified directory
    std::cout << "\nListing contents of " << directory << ":\n";

    for (const auto& entry : fs::directory_iterator(directory)) {
        std::cout << entry.path().filename().string();
        if (entry.is_directory()) {
            std::cout << "/";
        }
        std::cout << std::endl;
    }

    std::cout << "\nWaiting for 3 seconds..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    return 0;
} 