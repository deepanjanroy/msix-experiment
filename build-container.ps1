# Simple build script for AppContainer test program
$SourceFile = "make_container.cc"
$OutputFile = "out/make_container.exe"

Write-Host "Compiling AppContainer test program..." -ForegroundColor Green

# Create output directory if it doesn't exist
New-Item -ItemType Directory -Force -Path "out" | Out-Null
# Compile with clang++
& clang++ -std=c++17 -g $SourceFile -o $OutputFile -luserenv -ladvapi32

if ($LASTEXITCODE -ne 0) {
    Write-Error "Compilation failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

Write-Host "Compilation successful. Output: $OutputFile" -ForegroundColor Green 