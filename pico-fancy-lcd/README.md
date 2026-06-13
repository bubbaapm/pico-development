# Pico Fancy LCD

Pico 2 W firmware for ST7735 LCD animation experiments. This project is bootstrapped from:

- `pico-radar`: ST7735 driver, button handling, and LCD animation sources.
- `pico2w-dev`: Pico 2 W CMake/lwIP HTTPD setup.

## Current state

- Targets `pico2_w`.
- Runs the copied LCD animation set on the ST7735 display.
- Uses the Pico 2 W CYW43/lwIP poll stack.
- Hosts a small HTTP UI when `wifi_credentials.h` is present.
- Keeps radar-dependent animations buildable through a local no-op `ld2410` compatibility shim.

## WiFi setup

Copy `wifi_credentials.h.example` to `wifi_credentials.h` and set your SSID/password. Without that file, the firmware builds and runs as an LCD/button-only animation host.

## Controls

- Left button: previous animation.
- Right button: next animation.
- Both buttons: replay current animation.
- Web UI: select animation, set duration, previous/replay/next.

## Build

From this directory:

```powershell
cmake -S . -B build -G Ninja
cmake --build build
```

## Next steps

- Replace the `ld2410` shim with a cleaner animation input abstraction so web controls can simulate distance/motion values.
- Add JSON-style status endpoints if the UI needs live polling without page reloads.
- Decide whether long-running animations should be interruptible immediately from the network; right now web commands are serviced cooperatively through `animation_delay()`.
