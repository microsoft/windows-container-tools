
# files to copy from Windows host.
# this are mainly legacy binaries that don't have any
# extra dependencies.
$filePaths = @(
    "C:\Windows\System32\fc.exe",
    "C:\Windows\System32\whoami.exe"
)

$dest = "./bin"

if (-not (Test-Path -Path $dest)) {
    New-Item -ItemType Directory -Path $dest
}

foreach ($filePath in $filePaths) {
    Copy-Item -Path $filePath -Destination $dest
}
