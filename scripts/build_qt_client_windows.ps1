param(
    [string]$QtPrefixPath = "E:\QT\6.11.0\mingw_64",
    [string]$BuildDir = ""
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $BuildDir) {
    $BuildDir = Join-Path $projectRoot "build-qt"
}
$env:PATH = "$QtPrefixPath\bin;$env:PATH"

cmake -S $projectRoot -B $BuildDir -G Ninja `
    -DBUILD_QT_CLIENT=ON `
    -DBUILD_TESTS=ON `
    -DENABLE_PROTOBUF=OFF `
    -DENABLE_MYSQL=OFF `
    -DCMAKE_PREFIX_PATH=$QtPrefixPath

cmake --build $BuildDir --target QtUserClient test_qt_proto_messages test_client_session test_dashboard_widget test_login_widget test_register_widget
if ($LASTEXITCODE -ne 0) {
    throw "cmake build failed with exit code $LASTEXITCODE"
}

$clientExe = Join-Path $BuildDir "client\QtUserClient\QtUserClient.exe"
$windeployqt = Join-Path $QtPrefixPath "bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    throw "windeployqt was not found: $windeployqt"
}

& $windeployqt --release --no-translations $clientExe
if ($LASTEXITCODE -ne 0) {
    throw "windeployqt failed with exit code $LASTEXITCODE"
}

$networkDll = Join-Path (Split-Path $clientExe) "Qt6Network.dll"
if (-not (Test-Path $networkDll)) {
    throw "Qt deployment is incomplete; missing $networkDll"
}

ctest --test-dir (Join-Path $BuildDir "client\QtUserClient") --output-on-failure

Write-Host "Qt client: $BuildDir\client\QtUserClient\QtUserClient.exe"
