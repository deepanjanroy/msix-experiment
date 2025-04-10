trap {
    Write-Error "Error: $($_.Exception.Message)"
    exit 1
}


# directory of this file
$scriptDir = Split-Path -Path $MyInvocation.MyCommand.Path
$outDir = Join-Path -Path $scriptDir -ChildPath "out"
$distDir = Join-Path -Path $outDir -ChildPath "dist" 
$venvDir = Join-Path -Path $scriptDir -ChildPath "..\venv"
$PackageDir = $distDir
$OutputFile = Join-Path -Path $outDir -ChildPath "Py-Package.msix"
$CertName = "dproy-cert"
$ManifestFile = Join-Path $scriptDir "src\AppxManifest.xml"



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

      Write-Host "Launching application..." -ForegroundColor Green
      & Start-Process py-package
      if ($LASTEXITCODE -ne 0) {
          Write-Error "Failed to launch application"
          return $false
      }
      Write-Host "Application launched successfully." -ForegroundColor Green
      
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


Write-Host "Activating Windows SDK"
. "..\setup-sdk.ps1"

Update-ManifestVersion -ManifestFile $ManifestFile
Build-MsixPackage -PackageDir $PackageDir -OutputFile $OutputFile -CertName $CertName

# Copy src\AppxManifest.xml to dist directory
Write-Host "Copying AppxManifest.xml"
Copy-Item -Path $scriptDir\src\AppxManifest.xml -Destination $distDir

exit 1


# activate venv
Write-Host "Activating venv"
& $venvDir\Scripts\Activate.ps1

# Clean out directories
Write-Host "Cleaning out directory"
Remove-Item -Recurse -Force $outDir
New-Item -ItemType Directory -Path $outDir
New-Item -ItemType Directory -Path $distDir

# Navigate to out directory

# build the exe
Push-Location $scriptDir\out
Write-Host "Building pyinstaller exe"
pyinstaller --onefile --name py-package $scriptDir\src\app.py --distpath $distDir
Pop-Location

# Copy src\Assets to dist directory
Write-Host "Copying Assets"
Copy-Item -Path $scriptDir\src\Assets -Destination $distDir -Recurse







