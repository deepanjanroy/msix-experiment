# Simple build script for AppContainer test programs
$SourceFiles = @(
    "make_container.cc",
    "launch_container.cc",
    "hello_world.cc",
    "python_container.cc"
)

Write-Host "Compiling AppContainer test programs..." -ForegroundColor Green

# Create output directory if it doesn't exist
New-Item -ItemType Directory -Force -Path "out" | Out-Null

# Compile each source file
foreach ($SourceFile in $SourceFiles) {
    $OutputFile = "out/$([System.IO.Path]::GetFileNameWithoutExtension($SourceFile)).exe"
    Write-Host "Compiling $SourceFile..." -ForegroundColor Yellow
    
    # Compile with clang++
    & clang++ -std=c++17 -g $SourceFile -o $OutputFile -luserenv -ladvapi32

    if ($LASTEXITCODE -ne 0) {
        Write-Error "Compilation of $SourceFile failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }

    Write-Host "Successfully compiled to $OutputFile" -ForegroundColor Green
}

Write-Host "All compilations completed successfully." -ForegroundColor Green 