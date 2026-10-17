Import("env")

import filecmp
import re
import shutil
from pathlib import Path


def archive_immutable(source_path, archive_path, label):
    if archive_path.exists():
        if filecmp.cmp(source_path, archive_path, shallow=False):
            print(f"Identisches {label}-Archiv bereits vorhanden: {archive_path}")
            return
        raise RuntimeError(
            f"{label}-Version existiert bereits: {archive_path.name}. "
            "Bitte FIRMWARE_VERSION erhöhen; alte Firmware wird nicht überschrieben."
        )

    shutil.copy2(source_path, archive_path)
    print(f"Versioniertes {label}-Update erstellt: {archive_path}")


def create_factory_image(firmware_path, env):
    build_dir = Path(env.subst("$BUILD_DIR"))
    framework_dir = Path(env.PioPlatform().get_package_dir("framework-arduinoespressif32"))
    segments = [
        (0x1000, build_dir / "bootloader.bin"),
        (0x8000, build_dir / "partitions.bin"),
        (0xE000, framework_dir / "tools" / "partitions" / "boot_app0.bin"),
        (0x10000, firmware_path),
    ]
    for _, segment_path in segments:
        if not segment_path.is_file():
            raise RuntimeError(f"Factory-Bestandteil fehlt: {segment_path}")

    factory_path = build_dir / "firmware.factory.bin"
    with factory_path.open("wb") as output:
        for offset, segment_path in segments:
            if output.tell() > offset:
                raise RuntimeError(f"Factory-Segmente überschneiden sich bei 0x{offset:X}")
            output.write(b"\xFF" * (offset - output.tell()))
            with segment_path.open("rb") as segment:
                shutil.copyfileobj(segment, output)
    return factory_path


def archive_firmware(source, target, env):
    project_dir = Path(env.subst("$PROJECT_DIR"))
    build_info = (project_dir / "include" / "BuildInfo.h").read_text(encoding="utf-8")
    match = re.search(r'FIRMWARE_VERSION\s*=\s*"([^"]+)"', build_info)
    if not match:
        raise RuntimeError("FIRMWARE_VERSION fehlt in include/BuildInfo.h")

    version = match.group(1)
    firmware_path = Path(str(target[0]))
    archive_dir = project_dir / "firmware" / "esp32-wroom-32d"
    archive_dir.mkdir(parents=True, exist_ok=True)
    ota_archive = archive_dir / f"eq3-gateway-wroom-v{version}-ota.bin"
    factory_archive = archive_dir / f"eq3-gateway-wroom-v{version}-factory.bin"

    archive_immutable(firmware_path, ota_archive, "OTA")
    factory_path = create_factory_image(firmware_path, env)
    archive_immutable(factory_path, factory_archive, "Factory")


env.AddPostAction("$BUILD_DIR/${PROGNAME}.bin", archive_firmware)

