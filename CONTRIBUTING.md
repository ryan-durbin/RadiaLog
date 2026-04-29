# Contributing to RadiaLog

Thank you for your interest in contributing to RadiaLog! This document covers how to set up a development environment, coding standards, and the process for submitting changes.

## Development Environment

### Prerequisites

- [PlatformIO CLI](https://docs.platformio.org/page/installation.html) (`pip install platformio`)
- Python 3.10+ (for running tests)
- ESP-IDF toolchain (installed automatically by PlatformIO)
- A RadiaCode detector (model 102/103) for hardware testing

### Building

```bash
cd firmware

# Build for default board (XIAO ESP32S3 Plus)
pio run

# Build for a specific board
pio run -e lilygo_tdisplay_s3_amoled
pio run -e waveshare_amoled_175

# Upload to device
pio run -e seeed_xiao_esp32s3_plus --target upload

# Monitor serial output
pio device monitor --baud 115200
```

### Running Tests

All tests are Python scripts in `firmware/test/`. They use the standard library `unittest` framework — no external dependencies required.

```bash
cd firmware/test

# Run all tests
python3 -m unittest discover -v

# Run a single test file
python3 -m unittest test_uploader_core.py
```

## Code Style

### C++ (firmware source)

- **Indentation:** 4 spaces (no tabs)
- **Line length:** 120 characters max
- **Naming:**
  - Classes: `PascalCase` (`StatusPortal`, `WifiMgr`)
  - Functions: `camelCase` (`begin()`, `updateBattery()`)
  - Private members: `_snake_case` (`_batteryPercent`, `_server`)
  - Constants/macros: `UPPER_SNAKE_CASE` (`FW_VERSION`, `GPS_TX_PIN`)
- **Headers:** Use `#pragma once` + traditional `#ifndef` guard
- **No `delay()`** in non-blocking modules (use `millis()` timing)
- **Forward declarations** to avoid circular includes

### Python (tests)

- Follow [PEP 8](https://peps.python.org/pep-0008/) conventions
- Use `unittest.TestCase` for all test classes
- Group related assertions in methods named `test_<feature>`
- Each test file should end with `sys.exit(0)` on success, `sys.exit(1)` on failure

### Embedded HTML/CSS/JS (web portal)

- No external linting currently enforced — keep inline JS minimal and well-structured
- CSS uses CSS custom properties (`var(--primary)`, etc.) defined in the shared stylesheet
- Template strings use template literals, not string concatenation where possible

## Architecture Guidelines

### Adding a New Board

1. Add a `#elif defined(BOARD_XXX)` block in `src/config.h` with pin mappings
2. Define `HAS_DISPLAY`, `DISPLAY_WIDTH`, `DISPLAY_HEIGHT` if applicable
3. For circular displays, also define `DISPLAY_CIRCULAR 1`
4. Add an `[env:...]` section in `firmware/platformio.ini` with matching build flags
5. Add board-specific library dependencies under `lib_deps` or `board_build.partitions`

### Adding a New GPS Driver

1. Implement the `GPS` abstract interface (`src/gps/gps.h`) — only 6 virtual methods required
2. Create `gps_<name>.h` and `gps_<name>.cpp` in `src/gps/`
3. Add conditional include in `main.cpp` under the existing GPS section

### Adding a New Web Portal Page

1. Embed the HTML/CSS/JS as a PROGMEM array in `src/portal/html/<page>_html.h`
2. Register a route in `_registerRoutes()` in `src/portal/portal.cpp`
3. If the page needs data, add an API endpoint following the existing pattern (`/api/...`)

## Submitting Changes

1. Create a feature branch from `main`: `git checkout -b feat/<short-description>`
2. Make your changes and test locally (build + run Python tests)
3. Write or update tests as needed
4. Commit with a descriptive message following [Conventional Commits](https://www.conventionalcommits.org/):
   - `feat:` new feature
   - `fix:` bug fix
   - `docs:` documentation changes
   - `chore:` tooling, config, maintenance
   - `refactor:` code change that neither fixes a bug nor adds a feature
5. Push and open a Pull Request

## Testing Philosophy

The test suite uses **source-code introspection** — tests read `.h`/`.cpp` files with regex to verify structural properties (class declarations, method signatures, constant values, absence of `delay()` calls). This catches regressions in architecture without requiring an ESP32 simulator.

When adding new functionality, consider whether a functional test with mocked dependencies would be more valuable than another structural check. See the `test/` directory for examples of both approaches.

## Questions?

Open an issue or PR — we're happy to help get you started.
