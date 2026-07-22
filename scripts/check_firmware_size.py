Import("env")

from pathlib import Path


APP_PARTITION_BYTES = 0x5C0000
# Product constraint: all three features must fit within this app-image budget,
# regardless of the larger physical flash and 7 MiB OTA slot.
MAX_FIRMWARE_BYTES = 0x5B0000


def check_firmware_size(target, source, env):
    firmware_path = Path(str(target[0]))
    firmware_size = firmware_path.stat().st_size
    free_bytes = APP_PARTITION_BYTES - firmware_size

    print(
        "Firmware size gate: "
        f"{firmware_size} / {MAX_FIRMWARE_BYTES} bytes "
        f"({MAX_FIRMWARE_BYTES - firmware_size} bytes below product budget; "
        f"{free_bytes} bytes free in app partition)"
    )
    if firmware_size > MAX_FIRMWARE_BYTES:
        raise RuntimeError(
            "firmware.bin exceeds the 5,963,776-byte product budget: "
            f"{firmware_size} > {MAX_FIRMWARE_BYTES} bytes"
        )


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", check_firmware_size)
