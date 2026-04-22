if (-not (Get-Command usbipd -ErrorAction SilentlyContinue)) {
    Write-Host "usbipd-win is not installed or not on PATH."
    Write-Host "Install it with: winget install --id dorssel.usbipd-win -e"
    exit 1
}

usbipd list

