New-Item -ItemType Directory -Force -Path "build" | Out-Null

Remove-Item -Path "build\*" -Force -ErrorAction SilentlyContinue
Remove-Item -Path "webserver.exe" -Force -ErrorAction SilentlyContinue

Write-Host "Compiling assembly..."
nasm -f win64 -DWINDOWS -O3 src/http/parser.asm -o build/parser.obj

Write-Host "Compiling C++..."
g++ -m64 -O3 -march=native -std=c++17 `
    -I"src" `
    src/app.cpp src/socket.cpp src/config/config.cpp `
    build/parser.obj `
    -o webserver.exe `
    -lws2_32 -static -static-libgcc -static-libstdc++

if ($LASTEXITCODE -eq 0) {
    Write-Host "Build successful!"
} else {
    Write-Error "Build failed!"
    exit 1
}