# Build script with vcpkg support
param(
    [string]$VcpkgRoot = "",
    [string]$Triplet = "x64-windows"
)

# Read vcpkg config from VS Code settings if available
$settingsFile = ".vscode/settings.json"
if (Test-Path $settingsFile) {
    $settings = Get-Content $settingsFile | ConvertFrom-Json
    if ($settings.'vcpkg.root' -and $settings.'vcpkg.root' -ne "") {
        $VcpkgRoot = $settings.'vcpkg.root'
    }
    if ($settings.'vcpkg.triplet' -and $settings.'vcpkg.triplet' -ne "") {
        $Triplet = $settings.'vcpkg.triplet'
    }
}

# Detect vcpkg root
if (-not $VcpkgRoot) {
    if ($env:VCPKG_ROOT) {
        $VcpkgRoot = $env:VCPKG_ROOT
    } elseif (Test-Path "C:/vcpkg") {
        $VcpkgRoot = "C:/vcpkg"
    } elseif (Test-Path "$env:USERPROFILE/vcpkg") {
        $VcpkgRoot = "$env:USERPROFILE/vcpkg"
    }
}

# Build include and library flags
$includeFlags = "-I."
$libFlags = ""

if ($VcpkgRoot) {
    $vcpkgInclude = "$VcpkgRoot/installed/$Triplet/include"
    $vcpkgLib = "$VcpkgRoot/installed/$Triplet/lib"
    
    if (Test-Path $vcpkgInclude) {
        $includeFlags += " -I`"$vcpkgInclude`""
        Write-Host "Using vcpkg include: $vcpkgInclude" -ForegroundColor Green
        
        # Check for required packages
        $missingPackages = @()
        if (-not (Test-Path "$vcpkgInclude/imgui.h") -and -not (Test-Path "$vcpkgInclude/imgui/imgui.h")) {
            $missingPackages += "imgui"
        }
        if (-not (Test-Path "$vcpkgInclude/implot.h") -and -not (Test-Path "$vcpkgInclude/implot/implot.h")) {
            $missingPackages += "implot"
        }
        if (-not (Test-Path "$vcpkgInclude/GLFW/glfw3.h") -and -not (Test-Path "$vcpkgInclude/glfw3.h")) {
            $missingPackages += "glfw3"
        }
        
        if ($missingPackages.Count -gt 0) {
            Write-Host "Warning: Missing packages: $($missingPackages -join ', ')" -ForegroundColor Yellow
            Write-Host "Install with: vcpkg install $($missingPackages -join ' ')" -ForegroundColor Yellow
        }
    }
    
    if (Test-Path $vcpkgLib) {
        $libFlags = "-L`"$vcpkgLib`""
        Write-Host "Using vcpkg lib: $vcpkgLib" -ForegroundColor Green
    } else {
        Write-Host "Warning: vcpkg lib path not found: $vcpkgLib" -ForegroundColor Yellow
    }
} else {
    Write-Host "Warning: vcpkg not found, building without vcpkg paths" -ForegroundColor Yellow
}

# Build command
$buildCmd = "g++ -std=c++20 -static-libstdc++ $includeFlags main.cpp Backtester/Backtester.cpp Chart/Chart.cpp DataHandler/MMapFile.cpp DataHandler/DateUtils.cpp DataHandler/TapeReader.cpp -o main.exe $libFlags -limgui -limplot -lglfw3 -lopengl32 -lgdi32"

Write-Host "Building..." -ForegroundColor Cyan
Write-Host $buildCmd -ForegroundColor Gray

Invoke-Expression $buildCmd

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful!" -ForegroundColor Green
    Write-Host "Running..." -ForegroundColor Cyan
    .\main.exe
} else {
    Write-Host "Build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}
