#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <userenv.h>
#include <sddl.h>
#include <iostream>

// Function to get the SID for a specific AppContainer
BOOL GetSpecificAppContainerSid(LPCWSTR containerName, PSID* ppsid) {
  HRESULT hr;
  LPCWSTR displayName = L"My App Container";
  
  hr = DeriveAppContainerSidFromAppContainerName(containerName, ppsid);
  
  if (SUCCEEDED(hr)) {
    return TRUE;
  } else {
    printf("Failed to get AppContainer SID. HRESULT: %lx\n", hr);
    return FALSE;
  }
}

void PrintTokenInformation(HANDLE tokenHandle) {
  DWORD tokenInfoLength = 0;
  GetTokenInformation(tokenHandle, TokenUser, NULL, 0, &tokenInfoLength);

  if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
    std::cerr << "Failed to get token information size." << std::endl;
    return;
  }

  BYTE* tokenInfo = new BYTE[tokenInfoLength];
  if (!GetTokenInformation(tokenHandle, TokenUser, tokenInfo, tokenInfoLength, &tokenInfoLength)) {
    std::cerr << "Failed to get token information." << std::endl;
    delete[] tokenInfo;
    return;
  }

  TOKEN_USER* tokenUser = reinterpret_cast<TOKEN_USER*>(tokenInfo);
  SID_NAME_USE sidType;
  char name[256], domain[256];
  DWORD nameSize = sizeof(name), domainSize = sizeof(domain);

  if (LookupAccountSid(NULL, tokenUser->User.Sid, name, &nameSize, domain, &domainSize, &sidType)) {
    std::cout << "User: " << domain << "\\" << name << std::endl;
  } else {
    std::cerr << "Failed to lookup account SID." << std::endl;
  }

  delete[] tokenInfo;
}

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

HANDLE GetAccessToken() {
  HANDLE hToken = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE |TOKEN_QUERY, &hToken)) {
    printf("Failed to open process token.\n");
    exit(1);
  }

  printf("Token: %p\n", hToken);
  PrintTokenInformation(hToken);
  return hToken;
}

HANDLE CreateAppContainerToken(HANDLE primaryToken, PSID pAppContainerSid) {
  HANDLE hToken = nullptr;
  // SecurityImpersonation: I want to create a new process with this token. 
  // https://learn.microsoft.com/en-us/windows/win32/api/winnt/ne-winnt-security_impersonation_level
  // TokenPrimary: The new token will be a primary token for the new process.
  // TOKEN_ALL_ACCESS: Probably can have less access but we're going to make a bunch of
  // changes to this token so let's just grab everything for this POC.
  if (!DuplicateTokenEx(
          /*hExistingToken HANDLE*/ primaryToken,
          /*dwDesiredAccess DWORD*/ TOKEN_ALL_ACCESS,
          /*lpTokenAttributes LPSECURITY_ATTRIBUTES*/ nullptr,
          /*ImpersonationLevel SECURITY_IMPERSONATION_LEVEL*/ SecurityImpersonation,
          /*TokenType TOKEN_TYPE*/ TokenPrimary,
          /*phNewToken PHANDLE*/ &hToken)) {
    printf("Failed to duplicate token.\n");
    PrintLastError();
    exit(1);
  }

  return hToken;
} 

BOOL CreateAppContainerProcess(HANDLE hToken, PSID pAppContainerSid, LPSTR lpCommandLine) {
  // Set up the security capabilities
  SECURITY_CAPABILITIES secCaps = {};
  secCaps.AppContainerSid = pAppContainerSid;
  secCaps.Capabilities = nullptr;
  secCaps.CapabilityCount = 0;

  // Set it on StartupInfo
  STARTUPINFOEX siEx{};
  siEx.StartupInfo.cb = sizeof(siEx);

  SIZE_T attributeListSize;
  InitializeProcThreadAttributeList(
      /*lpAttributeList LPPROC_THREAD_ATTRIBUTE_LIST*/ nullptr,
      /*dwAttributeCount DWORD*/ 1,
      /*dwFlags DWORD*/ 0,
      /*lpSize PSIZE_T*/ &attributeListSize
  );

  siEx.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)HeapAlloc(
      /*hHeap HANDLE*/ GetProcessHeap(),
      /*dwFlags DWORD*/ 0,
      /*dwBytes SIZE_T*/ attributeListSize
  );

  if (!siEx.lpAttributeList) {
    printf("Failed to allocate attribute list.\n");
    exit(1);
  }

  if (!InitializeProcThreadAttributeList(
      /*lpAttributeList LPPROC_THREAD_ATTRIBUTE_LIST*/ siEx.lpAttributeList,
      /*dwAttributeCount DWORD*/ 1,
      /*dwFlags DWORD*/ 0,
      /*lpSize PSIZE_T*/ &attributeListSize
  )) {
    printf("Failed to initialize attribute list.\n");
    exit(1);
  }

  if (!UpdateProcThreadAttribute(
      /*lpAttributeList LPPROC_THREAD_ATTRIBUTE_LIST*/ siEx.lpAttributeList,
      /*dwFlags DWORD*/ 0,
      /*Attribute DWORD*/ PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES,
      /*lpValue PVOID*/ &secCaps,
      /*cbSize SIZE_T*/ sizeof(secCaps),
      nullptr, nullptr)) {
    printf("Failed to update attribute list.\n");
    exit(1);
  } 

  PROCESS_INFORMATION pi;
  // Print the command line
  printf("Command line: %s\n", lpCommandLine);
  if (!CreateProcessAsUser(
      /*hToken HANDLE*/ hToken,
      /*lpApplicationName LPCTSTR*/ nullptr,
      /*lpCommandLine LPSTR*/ lpCommandLine,
      /*lpProcessAttributes LPSECURITY_ATTRIBUTES*/ nullptr,
      /*lpThreadAttributes LPSECURITY_ATTRIBUTES*/ nullptr,
      /*bInheritHandles BOOL*/ FALSE,
      /*dwCreationFlags DWORD*/ EXTENDED_STARTUPINFO_PRESENT,
      /*lpEnvironment LPVOID*/ nullptr,
      /*lpCurrentDirectory LPCTSTR*/ nullptr,
      /*lpStartupInfo LPSTARTUPINFO*/ &siEx.StartupInfo,
      /*lpProcessInformation LPPROCESS_INFORMATION*/ &pi)) {
    printf("Failed to create process.\n");
    PrintLastError();
    return FALSE;
  }
  printf("AppContainer Token: %p\n", hToken);
  PrintTokenInformation(hToken);
  
  return TRUE;
}


int _tmain( int argc, TCHAR *argv[] )
{
  PSID pAppContainerSid = nullptr;
  if (!GetSpecificAppContainerSid(L"My App Container", &pAppContainerSid)) {
    printf("Failed to get AppContainer SID.\n");
    return 1;
  }

  // Allocate a LPSTR to hold the AppContainer SID string
  LPSTR pszAppContainerSid = nullptr;
  if (!ConvertSidToStringSid(pAppContainerSid, &pszAppContainerSid)) {
    printf("Failed to convert AppContainer SID to string.\n");
    return 1;
  }
  printf("AppContainer SID: %s\n", pszAppContainerSid);

  HANDLE hToken = GetAccessToken();    
  HANDLE hAppContainerToken = CreateAppContainerToken(hToken, pAppContainerSid);

  STARTUPINFO si;
  PROCESS_INFORMATION pi;

  ZeroMemory( &si, sizeof(si) );
  si.cb = sizeof(si);
  ZeroMemory( &pi, sizeof(pi) );

  if( argc != 2 )
  {
    printf("Usage: %s [cmdline]\n", argv[0]);
    return 1;
  }

  if (!CreateAppContainerProcess(hAppContainerToken, pAppContainerSid, argv[1])) {
    printf("Failed to create app container process.\n");
    // return 1;
  } else {
    printf("AppContainer process created successfully.\n");
  }

  // Start the child process.
  // Print the command line
  printf("Command line: %s\n", argv[1]);
  if( !CreateProcess( NULL,   // No module name (use command line)
    argv[1],        // Command line
    NULL,           // Process handle not inheritable
    NULL,           // Thread handle not inheritable
    FALSE,          // Set handle inheritance to FALSE
    0,              // No creation flags
    NULL,           // Use parent's environment block
    NULL,           // Use parent's starting directory 
    &si,            // Pointer to STARTUPINFO structure
    &pi )           // Pointer to PROCESS_INFORMATION structure
  ) 
  {
    printf( "CreateProcess failed (%lu).\n", GetLastError() );
    return 1;
  } else {
    printf("Non-AppContainer process created successfully.\n");
  }

  // Wait until child process exits.
  WaitForSingleObject( pi.hProcess, INFINITE );

  // Close process and thread handles. 
  CloseHandle( pi.hProcess );
  CloseHandle( pi.hThread );

  return 0;
}