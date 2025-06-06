trap {
    Write-Error "Error: $($_.Exception.Message)"
    exit 1
}


# directory of this file
$scriptDir = Split-Path -Path $MyInvocation.MyCommand.Path
$outDir = Join-Path -Path $scriptDir -ChildPath "out"
$distDir = Join-Path -Path $outDir -ChildPath "dist" 
$PackageDir = $distDir
$OutputFile = Join-Path -Path $outDir -ChildPath "MSIXPython.msix"
$CertName = "dproy-cert"
$ManifestFile = Join-Path $scriptDir "src\AppxManifest.xml"
$PackageName = "MSIXPython"



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

function Build-MsixPackage {
  param (
      [Parameter(Mandatory=$true)]
      [string]$PackageDir,
      
      [Parameter(Mandatory=$true)]
      [string]$OutputFile,
      
      [Parameter(Mandatory=$true)]
      [string]$CertName
  )

  try {
      # Create the MSIX package
      & MakeAppx.exe pack /d $PackageDir /p $OutputFile /o
      if ($LASTEXITCODE -ne 0) {
          Write-Error "MakeAppx failed with exit code $LASTEXITCODE"
          return $false
      }
      
      Write-Host "Package created successfully." -ForegroundColor Green
      Write-Host "Signing package..." -ForegroundColor Green
      
      # Sign the package
      & SignTool.exe sign /fd SHA256 /n $CertName $OutputFile
      if ($LASTEXITCODE -ne 0) {
          Write-Error "SignTool failed with exit code $LASTEXITCODE"
          return $false
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
          return $false
      }
      Write-Host "Package installed successfully." -ForegroundColor Green
      
      return $true
  }
  catch {
      Write-Error "An error occurred: $_"
      return $false
  }
}

function Update-ManifestVersion {
  param (
      [Parameter(Mandatory=$true)]
      [string]$ManifestFile
  )

  $manifestContent = Get-Content $ManifestFile -Raw

  if ($manifestContent -match '<Identity[^>]*Version="([^"]+)"') {
      $currentVersion = $matches[1]
      $newVersion = Update-Version $currentVersion
      # Use a more precise replacement that preserves XML formatting
      $pattern = "Version=`"$currentVersion`""
      $replacement = "Version=`"$newVersion`""
      $manifestContent = $manifestContent.Replace($pattern, $replacement)
      Set-Content -Path $ManifestFile -Value $manifestContent.TrimEnd()
      Write-Host "Version updated from $currentVersion to $newVersion" -ForegroundColor Green
  } else {
      Write-Error "Could not find version number in manifest file"
      throw "Could not find version number in manifest file"
  }
}

# Uninstall Previously Installed Package if it exists
Write-Host "Uninstalling previously installed package"
Get-AppxPackage -name $PackageName | Remove-AppxPackage -ErrorAction SilentlyContinue

# Clean out directories
Write-Host "Cleaning out directory"
Remove-Item -Recurse -Force $outDir
New-Item -ItemType Directory -Path $outDir
New-Item -ItemType Directory -Path $distDir

# --- C++ Compilation ---
Write-Host "Compiling C++ source files..."
$sourceFilesToCompile = @(
    'launch',
    'launch2',
    'winrt-app' # Add winrt-app to the list
)

foreach ($baseName in $sourceFilesToCompile) {
    $sourcePath = Join-Path $scriptDir "src\$($baseName).cc"
    $outputPath = Join-Path $distDir "$($baseName).exe"
    
    if (-not (Test-Path $sourcePath)) {
        Write-Warning "Source file not found: $sourcePath"
        exit 1
    }

    Write-Host "Compiling $baseName.cc to $baseName.exe..."
    
    # Add WinRT compilation flags for winrt-app
    $compilerFlags = @("-luser32", "-lshell32", "-lwindowsapp.lib", "-lkernel32.lib")
    if ($baseName -eq 'winrt-app') {
        $compilerFlags += "-I $windowsSdkDir"
    }
    
    & cl /std:c++17 /EHsc /LIBPATH:$windowsSdkDir windowsapp.lib $sourcePath -o $outputPath @compilerFlags
    
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to compile $baseName.cc (Exit code: $LASTEXITCODE)"
        exit 1
    } else {
        Write-Host "Successfully compiled $baseName.cc" -ForegroundColor Green
    }
}
Write-Host "Finished C++ compilation."
# --- End C++ Compilation ---


# Copy python to dist directory
Write-Host "Copying Python"
Copy-Item -Path $scriptDir\src\python -Destination $distDir -Recurse

# Copy src\Assets to dist directory
Write-Host "Copying Assets"
Copy-Item -Path $scriptDir\..\Assets -Destination $distDir -Recurse

# Copy app.py to dist directory
Write-Host "Copying app.py"
Copy-Item -Path $scriptDir\src\app.py -Destination $distDir

Write-Host "Activating Windows SDK"
. "..\setup-sdk.ps1"
Update-ManifestVersion -ManifestFile $ManifestFile
Write-Host "Copying AppxManifest.xml"
Copy-Item -Path $scriptDir\src\AppxManifest.xml -Destination $distDir
Build-MsixPackage -PackageDir $PackageDir -OutputFile $OutputFile -CertName $CertName

Write-Host "Launching application..." -ForegroundColor Green
& Start-Process $PackageName
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to launch application"
    exit 1
}
Write-Host "Application launched successfully." -ForegroundColor Green

# & Start-Process "${PackageName}2"





