import win32security
import win32process
import win32con
import sys
import win32api
import os
import ctypes
from ctypes import wintypes

# Define necessary Windows API functions and structures
kernel32 = ctypes.WinDLL('kernel32', use_last_error=True)
userenv = ctypes.WinDLL('userenv', use_last_error=True)
advapi32 = ctypes.WinDLL('advapi32', use_last_error=True)

# Define PSID type
class PSID(ctypes.Structure):
    _fields_ = [
        ("Revision", ctypes.c_byte),
        ("SubAuthorityCount", ctypes.c_byte),
        ("IdentifierAuthority", ctypes.c_byte * 6),
        ("SubAuthority", ctypes.c_ulong * 15)
    ]

# Define HRESULT type
HRESULT = ctypes.c_long

# Define CreateAppContainerProfile function
CreateAppContainerProfile = userenv.CreateAppContainerProfile
CreateAppContainerProfile.argtypes = [
    wintypes.LPWSTR,  # pszAppContainerName
    wintypes.LPWSTR,  # pszDisplayName
    wintypes.LPWSTR,  # pszDescription
    ctypes.POINTER(wintypes.LPWSTR),  # pCapabilities
    wintypes.DWORD,   # dwCapabilityCount
    ctypes.POINTER(PSID)     # ppSidAppContainerSid
]
CreateAppContainerProfile.restype = HRESULT

def enable_privileges():
    # Get the current process token
    current_process = win32api.GetCurrentProcess()
    current_token = win32security.OpenProcessToken(current_process, win32con.TOKEN_ADJUST_PRIVILEGES | win32con.TOKEN_QUERY)
    
    # Define the privileges we need
    privileges = [
        win32security.SE_SECURITY_NAME,
        win32security.SE_TCB_NAME,
        win32security.SE_TAKE_OWNERSHIP_NAME,
        win32security.SE_RESTORE_NAME,
        win32security.SE_CREATE_PERMANENT_NAME,
        win32security.SE_ENABLE_DELEGATION_NAME,
        win32security.SE_CHANGE_NOTIFY_NAME,
        win32security.SE_DEBUG_NAME,
        win32security.SE_PROF_SINGLE_PROCESS_NAME,
        win32security.SE_SYSTEM_PROFILE_NAME,
        win32security.SE_LOCK_MEMORY_NAME
    ]
    
    # Enable each privilege
    for privilege in privileges:
        try:
            priv_id = win32security.LookupPrivilegeValue("", privilege)
            win32security.AdjustTokenPrivileges(
                current_token,
                0,
                [(priv_id, win32con.SE_PRIVILEGE_ENABLED)]
            )
        except Exception as e:
            print(f"Warning: Could not enable privilege {privilege}: {e}")

def is_admin():
    try:
        return ctypes.windll.shell32.IsUserAnAdmin()
    except:
        return False

def create_appcontainer_process(container_name, python_script):
    # Check for admin privileges
    if not is_admin():
        print("Error: This script must be run with administrator privileges to create AppContainer profiles.")
        print("Please run the script as administrator.")
        return None, None

    # Enable required privileges
    enable_privileges()

    # Create AppContainer profile
    display_name = container_name
    description = f"Python AppContainer for {container_name}"
    
    # Define minimal capabilities
    capabilities = [
        "isolatedWin32-userProfileMinimal",  # Access to user profile
        "isolatedWin32-profilesRootMinimal", # Access to app data
    ]
    
    # Convert capabilities to LPWSTR array
    capability_count = len(capabilities)
    capability_array = (wintypes.LPWSTR * capability_count)()
    for i, cap in enumerate(capabilities):
        capability_array[i] = ctypes.cast(ctypes.create_unicode_buffer(cap), wintypes.LPWSTR)
    
    # Create the profile
    sid = PSID()
    hr = CreateAppContainerProfile(
        container_name,
        display_name,
        description,
        capability_array,
        capability_count,
        ctypes.byref(sid)
    )
    
    if hr != 0:  # S_OK is 0
        error_msg = f"Error creating AppContainer profile: {hr}"
        if hr == -2147024891:  # E_ACCESSDENIED
            error_msg += " (Access Denied - Make sure you're running as administrator)"
        elif hr == -2147024713:  # ERROR_ALREADY_EXISTS
            error_msg += " (AppContainer profile already exists)"
        print(error_msg)
        return None, None
    
    # Create a directory for the app's data
    appdata_path = os.path.join(os.environ['LOCALAPPDATA'], container_name)
    os.makedirs(appdata_path, exist_ok=True)
    
    # Get the current process token
    current_process = win32api.GetCurrentProcess()
    current_token = win32security.OpenProcessToken(current_process, win32con.TOKEN_ALL_ACCESS)
    
    # Create restricted token with minimal privileges
    restricted_token = win32security.CreateRestrictedToken(
        current_token,
        win32security.DISABLE_MAX_PRIVILEGE | win32security.SANDBOX_INERT,
        [],  # No privileges to disable
        [],  # No SIDs to restrict
        []   # No SIDs to delete
    )
    
    # Set up the security attributes for the AppContainer
    security_attributes = win32security.SECURITY_ATTRIBUTES()
    security_attributes.bInheritHandle = True
    
    # Create startup info
    startup_info = win32process.STARTUPINFO()
    
    # Command to run
    command = f'python {python_script}'
    
    try:
        # Create the process in AppContainer with restricted token
        process_handle, thread_handle, pid, tid = win32process.CreateProcessAsUser(
            restricted_token,        # Token
            None,                    # Application name
            command,                 # Command line
            None,                    # Process security attributes
            None,                    # Thread security attributes
            False,                   # Inherit handles
            win32con.CREATE_NEW_CONSOLE | 
            win32con.NORMAL_PRIORITY_CLASS |
            0x800000,               # CREATE_APPCONTAINER flag
            None,                   # Environment
            appdata_path,           # Current directory
            startup_info            # Startup info
        )
        
        return process_handle, pid
    except Exception as e:
        print(f"Error creating process: {e}")
        return None, None

# Usage
if __name__ == "__main__":
    # First, try to delete any existing AppContainer profile
    try:
        userenv.DeleteAppContainerProfile("MyPythonContainer")
    except Exception as e:
        print(f"Note: Could not delete existing profile (may not exist): {e}")
    
    process_handle, pid = create_appcontainer_process("MyPythonContainer", "my_script.py")
    if process_handle is None:
        print("Failed to create AppContainer process")
    else:
        print(f"Created AppContainer process with PID: {pid}")