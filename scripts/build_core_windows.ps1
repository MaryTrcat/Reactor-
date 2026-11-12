$ErrorActionPreference = "Stop"

$scriptPath = "C:\Users\Dell\AppData\Roaming\Python\Python312\Scripts"
if (Test-Path $scriptPath) {
    $env:PATH = "$scriptPath;$env:PATH"
}

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$buildDir = Join-Path $projectRoot "build-core"
$cacheFile = Join-Path $buildDir "CMakeCache.txt"

if (Test-Path $cacheFile) {
    $cachedSource = Select-String -LiteralPath $cacheFile -Pattern "^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$" |
        Select-Object -First 1 |
        ForEach-Object { $_.Matches[0].Groups[1].Value }

    if ($cachedSource -and (([System.IO.Path]::GetFullPath($cachedSource).TrimEnd('\')) -ne $projectRoot.TrimEnd('\'))) {
        Remove-Item -LiteralPath $buildDir -Recurse -Force
    }
}

cmake -S $projectRoot -B $buildDir -G Ninja -DENABLE_PROTOBUF=OFF -DENABLE_MYSQL=OFF
cmake --build $buildDir
ctest --test-dir $buildDir --output-on-failure
