Import("env")

from pathlib import Path
import re
import shutil
import os


PROJECT_DIR = Path(env.subst("$PROJECT_DIR"))
STAGING_DIR = Path(env.subst("$PROJECT_DATA_DIR"))
RAW_IMAGE_BYTES = 483 * 480 * 2
IMAGES = {
    "src/ui/ui_img_261349413.c": "background-day.rgb565",
    "src/ui/ui_img_1167879829.c": "background-sunset.rgb565",
    "src/ui/ui_img_783909477.c": "background-night.rgb565",
}


def extract_image(source_path):
    source = source_path.read_text(encoding="utf-8")
    match = re.search(r"_data\[\]\s*=\s*\{(.*?)\};", source, re.DOTALL)
    if not match:
        raise RuntimeError(f"image byte array not found in {source_path}")

    image = bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", match.group(1)))
    if len(image) != RAW_IMAGE_BYTES:
        raise RuntimeError(
            f"unexpected RGB565 image size in {source_path}: "
            f"{len(image)} != {RAW_IMAGE_BYTES}"
        )
    return image


if STAGING_DIR.exists():
    shutil.rmtree(STAGING_DIR)
shutil.copytree(PROJECT_DIR / "data", STAGING_DIR)

# A hardware deployment can point this at an extracted private config backup.
# This keeps credentials out of source control while preserving the device's
# existing Wi-Fi and user settings when the regenerated LittleFS is uploaded.
preserved_config = os.environ.get("PRESERVE_CONFIG_FROM")
if preserved_config:
    preserved_path = Path(preserved_config)
    if not preserved_path.is_file():
        raise RuntimeError(f"preserved config not found: {preserved_path}")
    shutil.copy2(preserved_path, STAGING_DIR / "config.json")

asset_dir = STAGING_DIR / "assets"
asset_dir.mkdir(parents=True, exist_ok=True)
for source_name, output_name in IMAGES.items():
    (asset_dir / output_name).write_bytes(extract_image(PROJECT_DIR / source_name))
