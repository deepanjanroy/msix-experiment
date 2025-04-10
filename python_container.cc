#include <windows.h>
#include <userenv.h>
#include <sddl.h>
#include <aclapi.h>
#include <string>
#include <vector>
#include <iostream>

#pragma comment(lib, "userenv.lib")
#pragma comment(lib, "advapi32.lib")

void PrintLastError() {
  DWORD error = GetLastError();
  if (error != 0) {
    LPVOID lpMsgBuf;
    FormatMessage(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | 
      FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL,
      error,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
      (LPTSTR)&lpMsgBuf,
      0,
      NULL
    );
    
    std::cerr << "Error " << error << ": " << (LPTSTR)lpMsgBuf << std::endl;
    LocalFree(lpMsgBuf);
  }
}

// Function to create or get an existing AppContainer SID
BOOL GetOrCreateAppContainerSid(LPCWSTR containerName, PSID* ppSid) {
    // Try to derive existing AppContainer SID
    HRESULT hr = DeriveAppContainerSidFromAppContainerName(containerName, ppSid);
    
    if (SUCCEEDED(hr)) {
        std::wcout << L"Found existing AppContainer: " << containerName << std::endl;
        return TRUE;
    }
    
    // Create a new AppContainer if it doesn't exist
    hr = CreateAppContainerProfile(
        containerName,                      // AppContainer name
        containerName,                      // Display name (same for simplicity)
        containerName,                      // Description
        NULL,                               // No capabilities
        0,                                  // Zero capabilities
        ppSid);                             // Output SID
    
    if (SUCCEEDED(hr)) {
        std::wcout << L"Created new AppContainer: " << containerName << std::endl;
        return TRUE;
    } else {
        std::cerr << "Failed to create AppContainer profile. HRESULT: 0x" 
                  << std::hex << hr << std::endl;
        return FALSE;
    }
}

// Function to grant AppContainer access to a directory
BOOL GrantAppContainerAccess(PSID appContainerSid, const std::wstring& path) {
    PACL pOldDacl = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    
    // Get the existing DACL
    DWORD result = GetNamedSecurityInfoW(
        path.c_str(),                       // Path to the directory
        SE_FILE_OBJECT,                     // Object type (file/directory)
        DACL_SECURITY_INFORMATION,          // Get DACL
        NULL, NULL,                         // No owner or group
        &pOldDacl,                          // Existing DACL
        NULL,                               // No SACL
        &pSD);                              // Security descriptor
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to get security info. Error: " << result << std::endl;
        return FALSE;
    }
    
    // Prepare the new access entry
    EXPLICIT_ACCESSW ea = {0};
    ea.grfAccessPermissions = FILE_ALL_ACCESS;
    ea.grfAccessMode = SET_ACCESS;
    ea.grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
    ea.Trustee.TrusteeForm = TRUSTEE_IS_SID;
    ea.Trustee.TrusteeType = TRUSTEE_IS_USER;
    ea.Trustee.ptstrName = (LPWSTR)appContainerSid;
    
    // Create a new DACL with our access entry
    PACL pNewDacl = NULL;
    result = SetEntriesInAclW(
        1,                                  // 1 entry
        &ea,                                // Our new entry
        pOldDacl,                           // Existing ACL
        &pNewDacl);                         // Output new ACL
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to create new ACL. Error: " << result << std::endl;
        LocalFree(pSD);
        return FALSE;
    }
    
    // Apply the new DACL to the directory
    result = SetNamedSecurityInfoW(
        (LPWSTR)path.c_str(),               // Path
        SE_FILE_OBJECT,                     // Object type
        DACL_SECURITY_INFORMATION,          // Set DACL
        NULL, NULL,                         // No owner or group
        pNewDacl,                           // New DACL
        NULL);                              // No SACL
    
    if (result != ERROR_SUCCESS) {
        std::cerr << "Failed to set security info. Error: " << result << std::endl;
        LocalFree(pNewDacl);
        LocalFree(pSD);
        return FALSE;
    }
    
    std::wcout << L"Granted access to: " << path << std::endl;
    
    // Clean up
    LocalFree(pNewDacl);
    LocalFree(pSD);
    
    return TRUE;
}

// Launch Python in an AppContainer with restricted access
BOOL LaunchPythonInAppContainer(PSID appContainerSid, 
                               const std::wstring& pythonPath, 
                               const std::wstring& scriptPath,
                               const std::wstring& args) {
    // Set up the security capabilities
    SECURITY_CAPABILITIES secCaps = {0};
    secCaps.AppContainerSid = appContainerSid;
    secCaps.CapabilityCount = 0;
    secCaps.Capabilities = NULL;
    
    // Set up the attribute list for the process
    SIZE_T attrListSize = 0;
    InitializeProcThreadAttributeList(NULL, 1, 0, &attrListSize);
    
    LPPROC_THREAD_ATTRIBUTE_LIST pAttrList = 
        (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(
            GetProcessHeap(), 0, attrListSize);
    
    if (!pAttrList) {
        std::cerr << "Failed to allocate attribute list memory." << std::endl;
        return FALSE;
    }
    
    if (!InitializeProcThreadAttributeList(pAttrList, 1, 0, &attrListSize)) {
        std::cerr << "Failed to initialize attribute list. Error: " 
                  << GetLastError() << std::endl;
        HeapFree(GetProcessHeap(), 0, pAttrList);
        return FALSE;
    }
    
    if (!UpdateProcThreadAttribute(
        pAttrList,
        0,
        PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
        &secCaps,
        sizeof(secCaps),
        NULL,
        NULL)) {
        std::cerr << "Failed to update attribute list. Error: " 
                  << GetLastError() << std::endl;
        DeleteProcThreadAttributeList(pAttrList);
        HeapFree(GetProcessHeap(), 0, pAttrList);
        return FALSE;
    }
    
    // Create the command line: python.exe scriptPath args
    std::wstring cmdLine = L"\"" + pythonPath + L"\" \"" + scriptPath + L"\" " + args;
    std::wcout << L"Command: " << cmdLine << std::endl;
    
    // Buffer for the modifiable command line
    wchar_t* pCmdLine = new wchar_t[cmdLine.length() + 1];
    wcscpy_s(pCmdLine, cmdLine.length() + 1, cmdLine.c_str());
    
    // Initialize the startup info with the attribute list
    STARTUPINFOEXW siEx = {0};
    siEx.StartupInfo.cb = sizeof(siEx);
    siEx.lpAttributeList = pAttrList;
    
    // Initialize the process info
    PROCESS_INFORMATION pi = {0};
    
    // Create the process in the AppContainer
    BOOL result = CreateProcessW(
        NULL,                               // No specific application name
        pCmdLine,                           // Command line
        NULL, NULL,                         // Default process/thread attributes
        FALSE,                              // Don't inherit handles
        EXTENDED_STARTUPINFO_PRESENT |      // Use extended startup info
        CREATE_NO_WINDOW,                   // Don't create a window
        NULL,                               // Use parent's environment
        NULL,                               // Use parent's current directory
        &siEx.StartupInfo,                  // Startup info
        &pi);                               // Process info
    
    if (!result) {
        std::cerr << "Failed to create process. Error: " << GetLastError() << std::endl;
        PrintLastError();
        DeleteProcThreadAttributeList(pAttrList);
        HeapFree(GetProcessHeap(), 0, pAttrList);
        delete[] pCmdLine;
        return FALSE;
    }
    
    std::wcout << L"Successfully launched Python in AppContainer." << std::endl;
    
    // Wait for the process to finish
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    // Get the exit code
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    std::wcout << L"Python process exited with code: " << exitCode << std::endl;
    
    // Clean up
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    DeleteProcThreadAttributeList(pAttrList);
    HeapFree(GetProcessHeap(), 0, pAttrList);
    delete[] pCmdLine;
    
    return TRUE;
}

BOOL TestCreateProcess(const std::wstring& pythonPath, const std::wstring& scriptPath) {
    std::wstring cmdLine = L"\"" + pythonPath + L"\" \"" + scriptPath + L"\"";
    wchar_t* pCmdLine = new wchar_t[cmdLine.length() + 1];
    wcscpy_s(pCmdLine, cmdLine.length() + 1, cmdLine.c_str());
    
    STARTUPINFOW si = {0};  // Change to STARTUPINFOW for wide char
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    
    BOOL result = CreateProcessW(NULL, pCmdLine, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    
    if (!result) {
        std::cerr << "Test CreateProcess failed. Error: " << GetLastError() << std::endl;
    } else {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    
    delete[] pCmdLine;
    return result;
}

int wmain(int argc, wchar_t* argv[]) {
    if (argc < 4) {
        std::wcout << L"Usage: " << argv[0] 
                   << L" <python_path> <script_path> <allowed_dir1> [<allowed_dir2> ...]" 
                   << std::endl;
        return 1;
    }
    
    // Get the Python and script paths
    std::wstring pythonPath = argv[1];
    std::wstring scriptPath = argv[2];
    
    // Get the AppContainer SID
    PSID appContainerSid = NULL;
    if (!GetOrCreateAppContainerSid(L"PythonSandbox", &appContainerSid)) {
        std::cerr << "Failed to get or create AppContainer SID." << std::endl;
        return 1;
    }
    
    // Grant access to the Python directory (so Python can run)
    std::wstring pythonDir = pythonPath.substr(0, pythonPath.find_last_of(L'\\'));
    GrantAppContainerAccess(appContainerSid, pythonDir);
    
    // Grant access to the script path (so Python can read the script)
    std::wstring scriptDir = scriptPath.substr(0, scriptPath.find_last_of(L'\\'));
    GrantAppContainerAccess(appContainerSid, scriptDir);
    
    // Grant access to each allowed directory
    for (int i = 3; i < argc; i++) {
        std::wstring allowedDir = argv[i];
        GrantAppContainerAccess(appContainerSid, allowedDir);
    }
    
    // Grant access to Python directory and parent directories
    GrantAppContainerAccess(appContainerSid, L"C:\\Users");
    GrantAppContainerAccess(appContainerSid, L"C:\\Users\\deepa");
    GrantAppContainerAccess(appContainerSid, L"C:\\Users\\deepa\\AppData");
    GrantAppContainerAccess(appContainerSid, L"C:\\Users\\deepa\\AppData\\Local");
    GrantAppContainerAccess(appContainerSid, L"C:\\Users\\deepa\\AppData\\Local\\Programs");
    GrantAppContainerAccess(appContainerSid, L"C:\\Users\\deepa\\AppData\\Local\\Programs\\Python");
    GrantAppContainerAccess(appContainerSid, L"C:\\Users\\deepa\\AppData\\Local\\Programs\\Python\\Python312");
    
    // Grant access to system directories Python might need
    GrantAppContainerAccess(appContainerSid, L"C:\\Windows\\System32");
    
    // Build the script arguments (all args beyond the allowed directories)
    std::wstring scriptArgs = L"";
    
    if (!TestCreateProcess(pythonPath, scriptPath)) {
        std::cerr << "Failed to create non-AppContainer process." << std::endl;
        FreeSid(appContainerSid);
        return 1;
    }
    
    // Launch Python in the AppContainer
    if (!LaunchPythonInAppContainer(appContainerSid, pythonPath, scriptPath, scriptArgs)) {
        std::cerr << "Failed to launch Python in AppContainer." << std::endl;
        FreeSid(appContainerSid);
        return 1;
    }
    
    // Clean up
    FreeSid(appContainerSid);
    
    return 0;
}