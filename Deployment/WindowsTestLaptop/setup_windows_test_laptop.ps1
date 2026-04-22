param(
    [string] $WslDistribution = "Ubuntu",
    [switch] $InstallUsbipd,
    [switch] $RunWslSetup
)

$ErrorActionPreference = "Stop"
$RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")

function Write-Section {
    param([string] $Title)
    Write-Host ""
    Write-Host "== $Title =="
}

function Test-CommandExists {
    param([string] $Command)
    return [bool](Get-Command $Command -ErrorAction SilentlyContinue)
}

function ConvertTo-WslPath {
    param([string] $WindowsPath)
    $converted = & wsl -d $WslDistribution -- wslpath -a "$WindowsPath"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to convert Windows path to WSL path: $WindowsPath"
    }
    return $converted.Trim()
}

Write-Section "Assistive Robotic Arm Windows Test Laptop Setup"
Write-Host "Repo root: $RepoRoot"
Write-Host "WSL distribution: $WslDistribution"

Write-Section "Checking WSL"
if (-not (Test-CommandExists "wsl")) {
    Write-Host "WSL is not installed."
    Write-Host "Run this from an elevated PowerShell window, then reboot if requested:"
    Write-Host "  wsl --install -d Ubuntu"
    exit 1
}

$distros = (& wsl --list --quiet) | ForEach-Object { $_.Trim([char]0xFEFF).Trim() } | Where-Object { $_ }
if ($distros -notcontains $WslDistribution) {
    Write-Host "WSL is installed, but distribution '$WslDistribution' was not found."
    Write-Host "Installed distributions:"
    $distros | ForEach-Object { Write-Host "  $_" }
    Write-Host ""
    Write-Host "Install Ubuntu with:"
    Write-Host "  wsl --install -d Ubuntu"
    exit 1
}

Write-Host "WSL distribution found."
& wsl --list --verbose

Write-Section "Checking usbipd-win"
if (-not (Test-CommandExists "usbipd")) {
    if ($InstallUsbipd) {
        if (-not (Test-CommandExists "winget")) {
            Write-Host "winget is not available. Install usbipd-win manually from:"
            Write-Host "  https://github.com/dorssel/usbipd-win/releases"
            exit 1
        }
        Write-Host "Installing usbipd-win with winget..."
        winget install --id dorssel.usbipd-win -e
    } else {
        Write-Host "usbipd-win is not installed or not on PATH."
        Write-Host "Install it with:"
        Write-Host "  winget install --id dorssel.usbipd-win -e"
        Write-Host ""
        Write-Host "Or rerun this script with:"
        Write-Host "  .\Deployment\WindowsTestLaptop\setup_windows_test_laptop.ps1 -InstallUsbipd"
    }
} else {
    Write-Host "usbipd-win found."
    usbipd list
}

Write-Section "Checking WSL systemd"
$systemdOk = $false
try {
    & wsl -d $WslDistribution -- bash -lc "systemctl --version >/dev/null 2>&1"
    $systemdOk = ($LASTEXITCODE -eq 0)
} catch {
    $systemdOk = $false
}

if ($systemdOk) {
    Write-Host "systemd is available in WSL."
} else {
    Write-Host "systemd is not available yet in WSL."
    Write-Host "Inside Ubuntu, edit /etc/wsl.conf:"
    Write-Host "  sudo nano /etc/wsl.conf"
    Write-Host ""
    Write-Host "Add these lines:"
    Write-Host "  [boot]"
    Write-Host "  systemd=true"
    Write-Host ""
    Write-Host "Then restart WSL from PowerShell:"
    Write-Host "  wsl --shutdown"
}

if ($RunWslSetup) {
    Write-Section "Running WSL dependency setup"
    $repoRootWsl = ConvertTo-WslPath "$RepoRoot"
    $escapedRepoRootWsl = $repoRootWsl.Replace("'", "'\''")
    & wsl -d $WslDistribution -- bash -lc "cd '$escapedRepoRootWsl' && bash Deployment/WindowsTestLaptop/scripts/setup_wsl.sh"
    if ($LASTEXITCODE -ne 0) {
        throw "WSL setup script failed."
    }
} else {
    Write-Section "WSL dependency setup"
    Write-Host "After systemd is enabled, run this inside Ubuntu from the repo root:"
    Write-Host "  bash Deployment/WindowsTestLaptop/scripts/setup_wsl.sh"
    Write-Host ""
    Write-Host "Or rerun this PowerShell script with:"
    Write-Host "  .\Deployment\WindowsTestLaptop\setup_windows_test_laptop.ps1 -RunWslSetup"
}

Write-Section "Next required manual steps"
Write-Host "1. Copy the local hardware config inside WSL:"
Write-Host "     cp Deployment/WindowsTestLaptop/config/hwci.example.yaml Deployment/WindowsTestLaptop/config/hwci.yaml"
Write-Host ""
Write-Host "2. Edit hwci.yaml with the real STM32, ESP32, and Jetson USB paths and commands."
Write-Host ""
Write-Host "3. Plug the boards into a powered USB hub connected to the Windows laptop."
Write-Host ""
Write-Host "4. List USB devices from PowerShell:"
Write-Host "     .\Deployment\WindowsTestLaptop\scripts\list_usb_devices.ps1"
Write-Host ""
Write-Host "5. Attach the selected USB bus IDs to WSL:"
Write-Host '     .\Deployment\WindowsTestLaptop\scripts\attach_usb_devices.ps1 -BusIds "1-4","1-7","1-9"'
Write-Host ""
Write-Host "6. Verify devices inside WSL:"
Write-Host "     lsusb"
Write-Host "     ls -l /dev/ttyUSB* /dev/ttyACM* 2>/dev/null"
Write-Host ""
Write-Host "7. Dry-run the controller inside WSL:"
Write-Host "     bash Deployment/WindowsTestLaptop/scripts/run_once.sh --dry-run --force"
Write-Host ""
Write-Host "8. Install the always-running WSL service after config is correct:"
Write-Host "     bash Deployment/WindowsTestLaptop/scripts/install_controller_service.sh"
