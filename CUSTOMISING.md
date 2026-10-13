# Local X4 Pro customization

This checkout starts from upstream v1.0.15, commit
`a9b8acedb6eb23eb58d782c1b02f954c79060e9c`, on local branch
`custom/x4-pro`. No changes have been pushed to upstream.

Read `AGENTS.md` before editing. Start with the stock display driver:
the neighboring app shell's custom-LUT experiment caused severe ghosting
on our UC8179 unit. Historical timing notes are not verified improvements.

## Build and flash

From the neighboring flasher project:

```bash
cd ../xteink-x4
tools/build_draftling.sh
```

The script builds with the official ESP-IDF 6.0.2 container and the
`xteink_x4_pro` preset, then stages all four required images into our
existing flasher. It does not access USB or flash a device. Refresh the
flasher page, choose Draftling, Connect, then Flash. Leave full erase
unchecked unless intentionally clearing settings and keyboard pairings.
Our original app-shell image is a separate dropdown choice.

The Docker build creates files in `firmware/build/xteink_x4_pro` and
downloads managed dependencies. Images are validated and staged together;
never combine our Arduino bootloader with a Draftling app image.
Published-release vs. local-build provenance is recorded in the flasher's
`draftling-manifest.json`. Bump `PROJECT_VER` in `firmware/CMakeLists.txt`
to distinguish future customized builds on the device and in the flasher.

## Starting points

The diary implementation lives in `firmware/components/editor/private_include/diary_model.h`,
`diary_ui.inc`, and `diary_touch.inc`, included by the existing editor UI.
The model owns Gregorian dates and 100 original quotes; the UI owns calendar,
day preview, clock and sync pages; the touch layer owns navigation and tools.
See `docs/diary-usability.md` for the review, storage format and verification.

Current local build: `1.0.15-diary6`. Home and everyday Settings use
touch-only icon grids with a brief outlined press effect and no selection highlight.
New documents start in Markdown automatically. Home has no instruction footer.
The touch1 crash (ELF SHA prefix `6fff798f6`) was caused by assigning a
NULL LVGL transition descriptor to the pressed tile state. LVGL dereferences
that descriptor on pointer press. touch2 removes inherited tile styles
before applying explicit styles. The host preview now exercises the real
`LV_STATE_PRESSED` path as well as the custom feedback state.
Home and reading work without a keyboard; Write requests pairing and Home
leaves the waiting page without cancelling background pairing/reconnection.
Touch browsing uses the normal inactivity timeout. The X4 Pro starts with
an 8-pixel left margin and 6 pixels on the other edges. Smaller existing
margins are raised
once, larger ones are kept, and later manual Settings changes survive.
Touch coordinates use the same physical crop as the display canvas.
Fonts and display waveforms remain unchanged. After pairing, Home offers
Continue writing, Today's entry, Calendar, My documents, Sync & backup and
Settings. New notes remain available through Tools and My documents. Everyday
settings are separated from Advanced; F1 / Ctrl+M opens Home.

- UI, menus, bars, editor layout: `firmware/components/editor/editor_ui.cpp`.
- Font assets: `firmware/components/fonts/` (generation instructions in AGENTS).
- Hardware options: `firmware/main/boards/xteink_x4_pro.h` and
  `firmware/sdkconfig.defaults.xteink_x4_pro`.
- Document editing: `firmware/components/editor/editor.cpp`.
- BLE keyboard: `firmware/components/ble_keyboard/`.
- Sleep: `firmware/components/standby/` and `firmware/main/main.cpp`.
- Display: `firmware/components/display/display_xteink_epd.cpp`.

User-facing firmware changes need a CHANGELOG entry under Unreleased.
The flasher stages the customized firmware without changing its behavior.
