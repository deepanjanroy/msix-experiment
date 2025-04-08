. ".\setup-sdk.ps1"

# Configuration
$PackageDir = "ignore/ElectronHelloPackage"
$OutputFile = "ElectronHello.msix"
$CertName = "dproy-cert"
$ManifestFile = Join-Path $PackageDir "AppxManifest.xml"
$TopLevelManifest = "EHAppxManifest.xml"
$ElectronAppDir = "electron-hello/out/electron-hello-win32-x64"

# Function to increment version number
function Update-Version {
    param (
        [string]$Version
    )
    
    $parts = $Version.Split('.')
    $build = [int]$parts[3]
    $build++
    $parts[3] = $build.ToString()
    return $parts -join '.'
}

# Ensure we're in the right directory
if (-not (Test-Path $PackageDir)) {
    Write-Host "Creating package directory..." -ForegroundColor Green
    New-Item -ItemType Directory -Path $PackageDir -Force
}

# Read and update version in top level manifest
Write-Host "Updating version number in top level manifest..." -ForegroundColor Green
$manifestContent = Get-Content $TopLevelManifest -Raw

if ($manifestContent -match '<Identity[^>]*Version="([^"]+)"') {
    $currentVersion = $matches[1]
    $newVersion = Update-Version $currentVersion
    # Use a more precise replacement that preserves XML formatting
    $pattern = "Version=`"$currentVersion`""
    $replacement = "Version=`"$newVersion`""
    $manifestContent = $manifestContent.Replace($pattern, $replacement)
    Set-Content -Path $TopLevelManifest -Value $manifestContent.TrimEnd()
    Write-Host "Version updated from $currentVersion to $newVersion" -ForegroundColor Green
} else {
    Write-Error "Could not find version number in manifest file"
    exit 1
}

# Copy manifest from top level to package directory
Write-Host "Copying manifest to package directory..." -ForegroundColor Green
Copy-Item -Path $TopLevelManifest -Destination $ManifestFile -Force
if (-not (Test-Path $ManifestFile)) {
    Write-Error "Failed to copy manifest to package directory"
    exit 1
}
Write-Host "Manifest copied successfully to $ManifestFile" -ForegroundColor Green

# Copy Electron app files to package directory
Write-Host "Copying Electron app files to package directory..." -ForegroundColor Green
Copy-Item -Path "$ElectronAppDir\*" -Destination $PackageDir -Recurse -Force
if (-not (Test-Path (Join-Path $PackageDir "electron-hello.exe"))) {
    Write-Error "Failed to copy Electron app files"
    exit 1
}
Write-Host "Electron app files copied successfully" -ForegroundColor Green

Write-Host "Building MSIX package..." -ForegroundColor Green

try {
    # Create the MSIX package
    & MakeAppx.exe pack /d $PackageDir /p $OutputFile /o
    if ($LASTEXITCODE -ne 0) {
        Write-Error "MakeAppx failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }
    
    Write-Host "Package created successfully." -ForegroundColor Green
    Write-Host "Signing package..." -ForegroundColor Green
    
    # Sign the package
    & SignTool.exe sign /fd SHA256 /n $CertName $OutputFile
    if ($LASTEXITCODE -ne 0) {
        Write-Error "SignTool failed with exit code $LASTEXITCODE"
        exit $LASTEXITCODE
    }
    
    Write-Host "Package signed successfully." -ForegroundColor Green
    Write-Host "Created and signed: $OutputFile" -ForegroundColor Green
    
    # Optional: Display file size
    $fileSize = (Get-Item $OutputFile).Length / 1MB
    Write-Host "Package size: $([math]::Round($fileSize, 2)) MB" -ForegroundColor Gray

    Write-Host "Installing package..." -ForegroundColor Green
    & Add-AppxPackage -Path $OutputFile
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to install package"
        exit $LASTEXITCODE
    }
    Write-Host "Package installed successfully." -ForegroundColor Green

    Write-Host "Launching application..." -ForegroundColor Green
    & Start-Process electron-hello
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to launch application"
        exit $LASTEXITCODE
    }
    Write-Host "Application launched successfully." -ForegroundColor Green
}
catch {
    Write-Error "An error occurred: $_"
    exit 1
} 