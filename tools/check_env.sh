#!/usr/bin/env bash
set -e

echo "== Pin firmware environment check =="

ok=true

echo "- Checking IDF_PATH..."
if [[ -z "${IDF_PATH}" ]]; then
  echo "  ERROR: IDF_PATH is not set."
  ok=false
else
  echo "  OK: IDF_PATH=${IDF_PATH}"
fi

echo "- Checking idf.py..."
if ! command -v idf.py >/dev/null 2>&1; then
  echo "  ERROR: idf.py not found in PATH. Did you 'source \"${IDF_PATH}/export.sh\"'?"
  ok=false
else
  echo -n "  OK: "; idf.py --version || true
fi

echo "- Checking CMake..."
if ! command -v cmake >/dev/null 2>&1; then
  echo "  ERROR: cmake not found. Install CMake 3.16+."
  ok=false
else
  echo -n "  OK: "; cmake --version | head -n1
fi

echo "- Checking toolchain (riscv32-esp-elf-gcc)..."
if ! command -v riscv32-esp-elf-gcc >/dev/null 2>&1; then
  echo "  WARN: riscv32-esp-elf-gcc not found in PATH. ESP-IDF export usually provides this."
else
  echo -n "  OK: "; riscv32-esp-elf-gcc --version | head -n1
fi

echo "- Python packages (ESP-IDF managed)..."
python3 -c "import sys; print(f'  OK: Python {sys.version.split()[0]}')" || { echo "  WARN: Python3 not found"; ok=false; }

echo "- Verifying project structure..."
for d in firmware/main firmware/components firmware/plugins; do
  if [[ ! -d "$d" ]]; then echo "  ERROR: Missing $d"; ok=false; else echo "  OK: $d"; fi
done

if [[ "$ok" != true ]]; then
  echo "\nEnvironment not ready. See errors above."
  exit 1
fi

echo "\nEnvironment looks good. You can run:"
echo "  cd firmware && idf.py set-target esp32c3 && idf.py build"
