param(
    [Parameter(Mandatory = $true)]
    [string[]] $BusIds,

    [string] $Distribution = "Ubuntu"
)

if (-not (Get-Command usbipd -ErrorAction SilentlyContinue)) {
    Write-Host "usbipd-win is not installed or not on PATH."
    Write-Host "Install it with: winget install --id dorssel.usbipd-win -e"
    exit 1
}

foreach ($busId in $BusIds) {
    Write-Host "Binding USB device $busId for WSL sharing..."
    usbipd bind --busid $busId

    Write-Host "Attaching USB device $busId to WSL distribution $Distribution..."
    usbipd attach --wsl --distribution $Distribution --busid $busId
}

Write-Host "Done. Verify inside WSL with: lsusb"

