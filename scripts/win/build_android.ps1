# PowerShell script for building Android libraries on Windows
# This is a Windows adaptation of build_android.sh

# Stop on first error
$ErrorActionPreference = "Stop"

# Get script directory
$SCRIPT_DIR = Split-Path -Parent $MyInvocation.MyCommand.Path
$PROJECT_ROOT = Split-Path -Parent (Split-Path -Parent $SCRIPT_DIR)

# Colors for output
$RED = [System.ConsoleColor]::Red
$GREEN = [System.ConsoleColor]::Green
$YELLOW = [System.ConsoleColor]::Yellow

# Function to check if a command exists
function Test-Command {
    param (
        [string]$Command
    )
    $oldPreference = $ErrorActionPreference
    $ErrorActionPreference = 'stop'
    try {
        if (Get-Command $Command) { return $true }
    }
    catch {
        return $false
    }
    finally {
        $ErrorActionPreference = $oldPreference
    }
}

# Check for required tools
if (-not (Test-Command "cmake")) {
    Write-Host "CMake not found. Please install CMake and add it to your PATH." -ForegroundColor $RED
    Write-Host "You can download it from: https://cmake.org/download/" -ForegroundColor $YELLOW
    Write-Host "After installation, make sure to add CMake to your PATH." -ForegroundColor $YELLOW
    exit 1
}

# Print usage information
function Print-Usage {
    Write-Host "Usage: $($MyInvocation.MyCommand.Name) [options]"
    Write-Host "Options:"
    Write-Host "  --help                 Print this help message"
    Write-Host "  --abi=[all|arm64-v8a|x86_64]  Specify which ABI to build for (default: all)"
    Write-Host "  --no-opencl            Disable OpenCL GPU acceleration"
    Write-Host "  --no-vulkan            Disable Vulkan GPU acceleration"
    Write-Host "  --vulkan               Enable Vulkan GPU acceleration (default)"
    Write-Host "  --debug                Build in debug mode"
    Write-Host "  --clean                Clean previous builds before building"
    Write-Host "  --clean-prebuilt       Clean entire prebuilt directory for a fresh start"
    Write-Host "  --install-deps         Install dependencies (OpenCL, etc.)"
    Write-Host "  --glslc-path=[path]    Specify a custom path to the GLSLC compiler"
    Write-Host "  --ndk-path=[path]      Specify a custom path to the Android NDK"
}

# Default values
$BUILD_ABI = "all"
$BUILD_OPENCL = $true
$BUILD_VULKAN = $true
$BUILD_TYPE = "Release"
$CLEAN_BUILD = $false
$CLEAN_PREBUILT = $false
$INSTALL_DEPS = $false
$CUSTOM_GLSLC_PATH = ""
$CUSTOM_NDK_PATH = ""

# Parse arguments
foreach ($arg in $args) {
    switch -Regex ($arg) {
        '^--help$' {
            Print-Usage
            exit 0
        }
        '^--abi=(.+)$' {
            $BUILD_ABI = $matches[1]
        }
        '^--no-opencl$' {
            $BUILD_OPENCL = $false
        }
        '^--no-vulkan$' {
            $BUILD_VULKAN = $false
        }
        '^--vulkan$' {
            $BUILD_VULKAN = $true
        }
        '^--debug$' {
            $BUILD_TYPE = "Debug"
        }
        '^--clean$' {
            $CLEAN_BUILD = $true
        }
        '^--clean-prebuilt$' {
            $CLEAN_PREBUILT = $true
        }
        '^--install-deps$' {
            $INSTALL_DEPS = $true
        }
        '^--glslc-path=(.+)$' {
            $CUSTOM_GLSLC_PATH = $matches[1]
        }
        '^--ndk-path=(.+)$' {
            $CUSTOM_NDK_PATH = $matches[1]
        }
        default {
            Write-Host "Unknown argument: $arg" -ForegroundColor $RED
            Print-Usage
            exit 1
        }
    }
}

# Define directories
$PREBUILT_DIR = Join-Path $PROJECT_ROOT "prebuilt"
$PREBUILT_LIBS_DIR = Join-Path $PREBUILT_DIR "libs"
$PREBUILT_EXTERNAL_DIR = Join-Path $PREBUILT_LIBS_DIR "external"
$ANDROID_DIR = Join-Path $PROJECT_ROOT "android"
$ANDROID_JNI_DIR = Join-Path $ANDROID_DIR "src\main\jniLibs"
$ANDROID_CPP_DIR = Join-Path $ANDROID_DIR "src\main\cpp"
$CPP_DIR = Join-Path $PROJECT_ROOT "cpp"
$LLAMA_CPP_DIR = Join-Path $CPP_DIR "llama.cpp"

# Third-party directories
$THIRD_PARTY_DIR = Join-Path $PREBUILT_DIR "third_party"
$OPENCL_HEADERS_DIR = Join-Path $THIRD_PARTY_DIR "OpenCL-Headers"
$OPENCL_INCLUDE_DIR = Join-Path $PREBUILT_EXTERNAL_DIR "opencl\include"
$OPENCL_LIB_DIR = Join-Path $PREBUILT_EXTERNAL_DIR "opencl\lib"
$VULKAN_HEADERS_DIR = Join-Path $THIRD_PARTY_DIR "Vulkan-Headers"
$VULKAN_INCLUDE_DIR = Join-Path $PREBUILT_EXTERNAL_DIR "vulkan\include"

# Create necessary directories
New-Item -ItemType Directory -Force -Path @(
    $PREBUILT_DIR,
    $PREBUILT_LIBS_DIR,
    $PREBUILT_EXTERNAL_DIR,
    (Join-Path $ANDROID_JNI_DIR "arm64-v8a"),
    (Join-Path $ANDROID_JNI_DIR "x86_64"),
    (Join-Path $ANDROID_CPP_DIR "include"),
    $OPENCL_INCLUDE_DIR,
    $OPENCL_LIB_DIR,
    $VULKAN_INCLUDE_DIR
) | Out-Null

# Determine number of cores
$N_CORES = (Get-CimInstance -ClassName Win32_Processor).NumberOfLogicalProcessors
Write-Host "Using $N_CORES cores for building" -ForegroundColor $YELLOW

# Set up and verify NDK path
if (-not $env:ANDROID_HOME) {
    if ($env:ANDROID_SDK_ROOT) {
        $env:ANDROID_HOME = $env:ANDROID_SDK_ROOT
    }
    elseif (Test-Path "$env:USERPROFILE\AppData\Local\Android\Sdk") {
        $env:ANDROID_HOME = "$env:USERPROFILE\AppData\Local\Android\Sdk"
    }
    else {
        Write-Host "Android SDK not found. Please set ANDROID_HOME or ANDROID_SDK_ROOT." -ForegroundColor $RED
        exit 1
    }
}

# Try to use the user-provided NDK path first
if ($CUSTOM_NDK_PATH) {
    $NDK_PATH = $CUSTOM_NDK_PATH
    Write-Host "Using custom NDK path: $NDK_PATH" -ForegroundColor $GREEN
    
    if (-not (Test-Path $NDK_PATH)) {
        Write-Host "Custom NDK path not found at $NDK_PATH" -ForegroundColor $RED
        exit 1
    }
}
else {
    # First try to find any available NDK
    $NDK_DIR = Join-Path $env:ANDROID_HOME "ndk"
    if (Test-Path $NDK_DIR) {
        # Get list of NDK versions sorted by version number (newest first)
        $NEWEST_NDK_VERSION = Get-ChildItem $NDK_DIR | Sort-Object -Descending | Select-Object -First 1
        
        if ($NEWEST_NDK_VERSION) {
            $NDK_PATH = Join-Path $NDK_DIR $NEWEST_NDK_VERSION.Name
            Write-Host "Found NDK version $($NEWEST_NDK_VERSION.Name), using this version" -ForegroundColor $GREEN
        }
        else {
            # If no NDK is found, fall back to the version from environment
            $NDK_PATH = Join-Path $NDK_DIR $env:NDK_VERSION
            Write-Host "No NDK versions found in $NDK_DIR, trying to use version $($env:NDK_VERSION) from environment" -ForegroundColor $YELLOW
            
            if (-not (Test-Path $NDK_PATH)) {
                Write-Host "NDK version $($env:NDK_VERSION) not found at $NDK_PATH" -ForegroundColor $RED
                Write-Host "Please install Android NDK using Android SDK Manager:" -ForegroundColor $YELLOW
                Write-Host "`$ANDROID_HOME\cmdline-tools\latest\bin\sdkmanager.bat `"ndk;latest`"" -ForegroundColor $YELLOW
                exit 1
            }
        }
    }
    else {
        # Try to find the NDK version specified in environment as last resort
        $NDK_PATH = Join-Path $NDK_DIR $env:NDK_VERSION
        Write-Host "No NDK directory found, trying to use version $($env:NDK_VERSION) from environment" -ForegroundColor $YELLOW
        
        if (-not (Test-Path $NDK_PATH)) {
            Write-Host "NDK directory not found at $NDK_DIR" -ForegroundColor $RED
            Write-Host "NDK version $($env:NDK_VERSION) from environment not found either" -ForegroundColor $RED
            Write-Host "Please install Android NDK using Android SDK Manager:" -ForegroundColor $YELLOW
            Write-Host "`$ANDROID_HOME\cmdline-tools\latest\bin\sdkmanager.bat `"ndk;latest`"" -ForegroundColor $YELLOW
            exit 1
        }
    }
}

# Extract the Android platform version from the NDK path
$PLATFORMS_DIR = Join-Path $NDK_PATH "platforms"
if (Test-Path $PLATFORMS_DIR) {
    # Get the highest API level available in the NDK
    $ANDROID_PLATFORM = Get-ChildItem $PLATFORMS_DIR | Sort-Object | Select-Object -Last 1
    if ($ANDROID_PLATFORM) {
        $ANDROID_MIN_SDK = $ANDROID_PLATFORM.Name -replace "android-", ""
        Write-Host "Using Android platform: $($ANDROID_PLATFORM.Name) (API level $ANDROID_MIN_SDK)" -ForegroundColor $GREEN
    }
    else {
        Write-Host "No Android platforms found in NDK. Using default API level 21." -ForegroundColor $YELLOW
        $ANDROID_PLATFORM = "android-21"
        $ANDROID_MIN_SDK = "21"
    }
}
else {
    Write-Host "No platforms directory found in NDK. Using default API level 21." -ForegroundColor $YELLOW
    $ANDROID_PLATFORM = "android-21"
    $ANDROID_MIN_SDK = "21"
}

# Setup build environment
$CMAKE_TOOLCHAIN_FILE = Join-Path $NDK_PATH "build\cmake\android.toolchain.cmake"
Write-Host "Using NDK at: $NDK_PATH" -ForegroundColor $GREEN
Write-Host "Using toolchain file: $CMAKE_TOOLCHAIN_FILE" -ForegroundColor $GREEN

# Determine NDK host platform
$NDK_HOST_TAG = "windows-x86_64"
Write-Host "Using NDK host platform: $NDK_HOST_TAG" -ForegroundColor $GREEN

# Function to build for a specific ABI
function Build-ForABI {
    param (
        [string]$ABI
    )
    
    Write-Host "Building for $ABI..." -ForegroundColor $YELLOW
    
    $BUILD_DIR = Join-Path $PREBUILT_DIR "build-android-$ABI"
    
    # Clean build directory if requested
    if ($CLEAN_BUILD -and (Test-Path $BUILD_DIR)) {
        Write-Host "Cleaning previous build for $ABI" -ForegroundColor $YELLOW
        Remove-Item -Recurse -Force $BUILD_DIR
    }
    
    New-Item -ItemType Directory -Force -Path $BUILD_DIR | Out-Null
    
    # Setup CMake flags
    $CMAKE_FLAGS = @(
        "-DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE",
        "-DANDROID_ABI=$ABI",
        "-DANDROID_PLATFORM=$ANDROID_PLATFORM",
        "-DCMAKE_BUILD_TYPE=$BUILD_TYPE",
        "-DBUILD_SHARED_LIBS=ON",
        "-DLLAMA_BUILD_TESTS=OFF",
        "-DLLAMA_BUILD_EXAMPLES=OFF",
        "-DLLAMA_CURL=OFF"
    )
    
    # Configure OpenCL
    if ($BUILD_OPENCL) {
        # Add OpenCL configuration
        $OPENCL_ABI_LIB_DIR = Join-Path $OPENCL_LIB_DIR $ABI
        New-Item -ItemType Directory -Force -Path $OPENCL_ABI_LIB_DIR | Out-Null
        
        if (-not (Test-Path (Join-Path $OPENCL_ABI_LIB_DIR "libOpenCL.so"))) {
            Write-Host "Creating stub libOpenCL.so for $ABI..." -ForegroundColor $YELLOW
            New-Item -ItemType File -Force -Path (Join-Path $OPENCL_ABI_LIB_DIR "libOpenCL.so") | Out-Null
        }
        
        $CMAKE_FLAGS += @(
            "-DLLAMA_OPENCL=ON",
            "-DOPENCL_INCLUDE_DIR=$OPENCL_INCLUDE_DIR",
            "-DOPENCL_LIB_DIR=$OPENCL_ABI_LIB_DIR"
        )
    }
    
    # Configure Vulkan
    if ($BUILD_VULKAN) {
        $CMAKE_FLAGS += @(
            "-DLLAMA_VULKAN=ON",
            "-DVULKAN_INCLUDE_DIR=$VULKAN_INCLUDE_DIR"
        )
    }
    
    # Run CMake configuration
    Write-Host "Configuring CMake for $ABI..." -ForegroundColor $YELLOW
    Push-Location $BUILD_DIR
    try {
        cmake $LLAMA_CPP_DIR $CMAKE_FLAGS
        if ($LASTEXITCODE -ne 0) {
            Write-Host "CMake configuration failed for $ABI" -ForegroundColor $RED
            Pop-Location
            return $false
        }
        
        # Build
        Write-Host "Building for $ABI..." -ForegroundColor $YELLOW
        cmake --build . --config $BUILD_TYPE -j $N_CORES
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Build failed for $ABI" -ForegroundColor $RED
            Pop-Location
            return $false
        }
        
        # Copy the built library
        $LIB_DIR = Join-Path $ANDROID_JNI_DIR $ABI
        New-Item -ItemType Directory -Force -Path $LIB_DIR | Out-Null
        
        $LIB_PATH = Join-Path $BUILD_DIR "libllama.so"
        if (Test-Path $LIB_PATH) {
            Copy-Item -Force $LIB_PATH $LIB_DIR
            Write-Host "Copied libllama.so to $LIB_DIR" -ForegroundColor $GREEN
        }
        else {
            Write-Host "Error: libllama.so not found at $LIB_PATH" -ForegroundColor $RED
            Pop-Location
            return $false
        }
    }
    catch {
        Write-Host "Error during build process for $ABI: $_" -ForegroundColor $RED
        Pop-Location
        return $false
    }
    finally {
        Pop-Location
    }
    
    return $true
}

# Main build process
Write-Host "Starting Android build process..." -ForegroundColor $YELLOW

# Clean prebuilt directory if requested
if ($CLEAN_PREBUILT) {
    Write-Host "Cleaning prebuilt directory..." -ForegroundColor $YELLOW
    if (Test-Path $PREBUILT_DIR) {
        Remove-Item -Recurse -Force $PREBUILT_DIR
    }
}

# Install dependencies if requested
if ($INSTALL_DEPS) {
    Write-Host "Installing dependencies..." -ForegroundColor $YELLOW
    & (Join-Path $SCRIPT_DIR "build_android_external.ps1")
}

# Build for each ABI
$ABIS = @()
if ($BUILD_ABI -eq "all") {
    $ABIS = @("arm64-v8a", "x86_64")
}
else {
    $ABIS = @($BUILD_ABI)
}

$SUCCESS = $true
foreach ($ABI in $ABIS) {
    if (-not (Build-ForABI $ABI)) {
        $SUCCESS = $false
        break
    }
}

if ($SUCCESS) {
    Write-Host "Android build completed successfully" -ForegroundColor $GREEN
    exit 0
}
else {
    Write-Host "Android build failed" -ForegroundColor $RED
    exit 1
} 