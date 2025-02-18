nasm -f win32 windows.asm -o windows.obj
golink /console /entry _main windows.obj ws2_32.dll kernel32.dll

if ($LASTEXITCODE -eq 0) {
    .\windows.exe
}
