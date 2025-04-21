# Set your SDK version here
$sdkVersion = "10.0.26100.0"

# Add the specific SDK version's bin/x64 path
$path = "C:\Program Files (x86)\Windows Kits\10\bin\$sdkVersion\x64"
if (Test-Path $path) {
    if ($env:Path -notlike "*$path*") {
        $env:Path += ";$path"
        Write-Host "Added to PATH: $path"
    } else {
        Write-Host "Path already exists in PATH: $path"
    }
} else {
    Write-Error "Path not found: $path"
    exit 1
} 

$windowsSdkDir = $path
