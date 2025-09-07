# Repository Guidelines

## Project Structure & Modules
- `firmware/`: ESP-IDF (ESP32-C3) application.
  - `main/`: core app (`pin_main.c`), subsystems (WiFi, OTA, webserver), and PWA assets in `main/www/`.
  - `components/`: reusable drivers and libs (e.g., `pin_canvas`, `fpc_a005`).
  - `plugins/`: built-in plugins (e.g., `plugins/clock`).
- `hardware/`: schematics, PCB, BOM.
- `docs/`: architecture and API notes (`docs/architecture/`, `docs/development/`).
- `tools/`: developer utilities, notably `tools/test_system.py`.

## Build, Test, Run
- Setup target: `cd firmware && idf.py set-target esp32c3` (once per env).
- Build + flash all: `make all && idf.py flash` (firmware plus embedded web assets).
- Flash web assets only: `make flash-web` (SPIFFS image for PWA files in `firmware/web`).
- Dev loop: `make dev` (build, flash, then serial monitor).
- Monitor: `idf.py monitor --port <PORT>`.
- Static checks/docs (if configured): `make lint`, `make docs`.

## Coding Style & Naming
- C (C99): spaces (4), no trailing whitespace, one declaration per line.
  - Functions: `pin_module_function_name()`
  - Types: `pin_module_type_t`
  - Constants/macros: `PIN_MODULE_CONSTANT`
  - Public headers in `components/*/include/` with minimal includes.
- JavaScript (PWA): ES6+, prefer const/let, file-local modules in `firmware/main/www/` or `firmware/web/`.

## Testing Guidelines
- System tests: `python3 tools/test_system.py --ip <device_ip>` (checks API, WiFi scan, PWA manifest). Use `--save results.json` to persist.
- Add component-level tests where practical and verify on real hardware for timing/power-sensitive code.
- Target: keep system tests green before PR; include reproduction steps for fixes.

## Commit & PR Guidelines
- Commits: imperative mood, scoped where helpful (e.g., `firmware:`, `web:`, `tools:`). Example: `firmware: implement OTA error handling`.
- PRs: clear description, linked issues, what/why, test evidence (logs from `test_system.py`), and screenshots for UI/PWA changes.
- Touching firmware interfaces or plugins: update docs in `docs/` and headers’ comments; mention migration notes in PR body.

## Security & Config Tips
- Do not commit credentials or secrets (WiFi, API keys). Configure via device UI or local env; avoid embedding in `sdkconfig`.
- Ensure `IDF_PATH` is set and matches ESP-IDF v5.1+.
