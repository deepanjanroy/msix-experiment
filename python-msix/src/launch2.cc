#include <windows.h>
#include <stdio.h>
#include <fstream>
#include <string>
#include <iostream>

void PrintLastError() {
    DWORD error = GetLastError();
    if (error == 0) return;

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        error,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPSTR)&messageBuffer,
        0,
        NULL
    );

    if (size > 0) {
        printf("Error %lu: %s\n", error, messageBuffer);
        LocalFree(messageBuffer);
    } else {
        printf("Error %lu: Unknown error\n", error);
    }
}

int main() {
  STARTUPINFO si = { sizeof(si) };
  PROCESS_INFORMATION pi;
  
  
  const char* cmd = "C:\\ProgramData\\MSIXPython_d90b81feyebxc\\eomfy-env\\Scripts\\python.exe";
  
  if (!CreateProcess(NULL,   // No module name (use command line)
                    (LPSTR)cmd,  // Command line
                    NULL,    // Process handle not inheritable
                    NULL,    // Thread handle not inheritable
                    FALSE,   // Set handle inheritance to FALSE
                    0,       // No creation flags
                    NULL,    // Use parent's environment block
                    NULL,    // Use parent's starting directory 
                    &si,     // Pointer to STARTUPINFO structure
                    &pi)     // Pointer to PROCESS_INFORMATION structure
  ) {
      printf("CreateProcess failed:\n");
      PrintLastError();
      printf("\nPress Enter to exit...");
      std::cin.get();
      return 1;
  }

  // Close process and thread handles
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  
  return 0;
}