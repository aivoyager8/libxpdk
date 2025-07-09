# PowerShell build script for libxpdk

param(
    [string]$BuildType = "Release",
    [string]$BuildDir = "build",
    [string]$InstallPrefix = "C:\Program Files\libxpdk",
    [switch]$Examples = $true,
    [switch]$Tests = $true,
    [switch]$Help
)

if ($Help) {
    Write-Host "libxpdk Build Script"
    Write-Host ""
    Write-Host "Parameters:"
    Write-Host "  -BuildType      Build configuration (Debug/Release) [Default: Release]"
    Write-Host "  -BuildDir       Build directory [Default: build]"
    Write-Host "  -InstallPrefix  Installation prefix [Default: C:\Program Files\libxpdk]"
    Write-Host "  -Examples       Build examples [Default: true]"
    Write-Host "  -Tests          Build tests [Default: true]"
    Write-Host "  -Help           Show this help message"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  .\build.ps1"
    Write-Host "  .\build.ps1 -BuildType Debug -Examples:$false"
    Write-Host "  .\build.ps1 -BuildDir custom_build -InstallPrefix C:\libxpdk"
    exit 0
}

Write-Host "Building libxpdk..." -ForegroundColor Green
Write-Host "Build type: $BuildType"
Write-Host "Build directory: $BuildDir"
Write-Host "Install prefix: $InstallPrefix"
Write-Host "Examples: $Examples"
Write-Host "Tests: $Tests"
Write-Host ""

# Check for required tools
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if (-not $cmake) {
    Write-Error "CMake not found. Please install CMake and add it to PATH."
    exit 1
}

$make = Get-Command make -ErrorAction SilentlyContinue
$ninja = Get-Command ninja -ErrorAction SilentlyContinue
$msbuild = Get-Command msbuild -ErrorAction SilentlyContinue

if (-not ($make -or $ninja -or $msbuild)) {
    Write-Error "No build system found. Please install make, ninja, or Visual Studio."
    exit 1
}

# Create build directory
if (Test-Path $BuildDir) {
    Write-Host "Cleaning existing build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}
New-Item -ItemType Directory -Path $BuildDir | Out-Null
Set-Location $BuildDir

try {
    # Configure with CMake
    Write-Host "Configuring with CMake..." -ForegroundColor Cyan
    
    $cmakeArgs = @(
        ".."
        "-DCMAKE_BUILD_TYPE=$BuildType"
        "-DCMAKE_INSTALL_PREFIX=`"$InstallPrefix`""
        "-DBUILD_EXAMPLES=$($Examples.ToString().ToLower())"
        "-DBUILD_TESTS=$($Tests.ToString().ToLower())"
    )
    
    # Use Ninja if available, otherwise use default generator
    if ($ninja) {
        $cmakeArgs += "-GNinja"
    }
    
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed"
    }
    
    # Build
    Write-Host "Building..." -ForegroundColor Cyan
    & cmake --build . --config $BuildType
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }
    
    Write-Host "Build completed successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "To install, run: cmake --install ." -ForegroundColor Yellow
    Write-Host "To run tests, run: ctest" -ForegroundColor Yellow
    Write-Host ""
    
    if ($Examples) {
        Write-Host "Example binaries:" -ForegroundColor Cyan
        Get-ChildItem -Path "examples" -Filter "*.exe" -ErrorAction SilentlyContinue | ForEach-Object {
            Write-Host "  $($_.FullName)" -ForegroundColor Gray
        }
    }
    
    if ($Tests) {
        Write-Host "Test binaries:" -ForegroundColor Cyan
        Get-ChildItem -Path "tests" -Filter "*.exe" -ErrorAction SilentlyContinue | ForEach-Object {
            Write-Host "  $($_.FullName)" -ForegroundColor Gray
        }
    }
    
} catch {
    Write-Error "Build failed: $_"
    exit 1
} finally {
    # Return to original directory
    Set-Location ..
}
