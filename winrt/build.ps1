# Source the SDK setup script
. ..\setup-sdk.ps1

cmd /c '"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 && set' | ForEach-Object {
    if ($_ -match '^(.*?)=(.*)$') {
        [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], 'Process')
    }
}
Write-Host "Added Vistual Studio Environment Variables"

# Compiler and linker flags
$clFlags = @(
    "/std:c++17",
    "/await",
    "/EHsc",
    "/W4",
    "/I$windowsSdkDir\..\..\Include\$sdkVersion\cppwinrt",
    "/I$windowsSdkDir\..\..\Include\$sdkVersion\shared",
    "/I$windowsSdkDir\..\..\Include\$sdkVersion\um",
    "/I$windowsSdkDir\..\..\Include\$sdkVersion\winrt"
)

# Clean the build directory
Remove-Item -Recurse -Force ".\build\*"

# Create output directory
if (-not (Test-Path ".\build")) {
    New-Item -ItemType Directory -Path ".\build" | Out-Null
}

# Compile the source file
Write-Host "Compiling hello_world.cpp..."
& "cl.exe" $clFlags WindowsApp.lib hello_world.cpp /Fe:build\hello_world.exe /Fo:build\hello_world.obj

Write-Host "Build complete! Executable is in build\hello_world.exe" 