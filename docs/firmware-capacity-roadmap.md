# Firmware Capacity and Three-Feature Roadmap

## Flash contract

- The installed ESP32-S3 has been physically detected as **16 MB internal flash**.
- The live partition table was read directly from the device: it uses the 8 MB-compatible factory layout, with a `0x5C0000` app partition and LittleFS beginning at `0x5D0000`. The data subtype is explicitly `littlefs`, as required by Arduino-ESP32 3.3.9.
- Every production `firmware.bin` must be no larger than 5,963,776 bytes, leaving at least 64 KiB inside the app partition.
- PlatformIO checks the app-partition limit, while `scripts/check_firmware_size.py` checks the generated binary's exact size after every build.
- Large static images belong in internal LittleFS rather than the application image. The physical 16 MB capacity must not be used to hide an oversized app.

## Feature 1: KMB touch favourites

- Keep at most eight KMB favourites and show four per ETA page.
- Store only resolved route, direction, service type and stop records; do not bundle the full KMB catalogue.
- Run catalogue and ETA HTTP work on core 0 and keep LVGL callbacks network-free.

## Feature 2: Hong Kong Observatory forecast

- Replace the one-day Open-Meteo presentation with the Hong Kong Observatory `fnd` local weather forecast feed.
- Read the official JSON endpoint using `dataType=fnd`, `rformat=json` and `lang=tc`.
- Retain only the first seven forecast entries needed by the seven coloured cards.
- The source contract is the [Hong Kong Observatory Open Data API documentation](https://data.weather.gov.hk/weatherAPI/doc/HKO_Open_Data_API_Documentation.pdf).

## Feature 3: Google Tasks agenda

- Request the narrow read-only Tasks scope unless editing is explicitly added later: `https://www.googleapis.com/auth/tasks.readonly`.
- Do not compile client secrets, access tokens or refresh tokens into firmware.
- Google currently documents its limited-input device flow as supporting only a limited scope list, and that list does **not** include Google Tasks scopes. Therefore the ESP32 must not assume that the TV/device flow can authorize Tasks directly.
- Use a companion browser authorization or a private intermediary such as Google Apps Script/Home Assistant. The ESP32 receives only the minimum agenda payload over HTTPS; long-lived OAuth credentials remain off-device where possible.
- If an on-device token is eventually required, provision it privately, store it separately from the firmware image, and support revocation/rotation.
- References: [Google Tasks scopes](https://developers.google.com/workspace/tasks/auth) and [OAuth for limited-input devices](https://developers.google.com/identity/protocols/oauth2/limited-input-device).

## Completion gate

The three features are not complete until native tests pass, the ESP32 build passes the exact binary-size gate, and hardware smoke tests confirm KMB ETA, seven-day HKO forecast rendering and Google Tasks agenda refresh/error handling.
