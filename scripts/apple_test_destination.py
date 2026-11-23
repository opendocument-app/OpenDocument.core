#!/usr/bin/env python3
"""Select a test destination and boot a fresh simulator with bounded recovery."""

import json
import os
import subprocess
from pathlib import Path


def xcrun(*args, timeout=30, capture=True):
    return subprocess.run(
        ["xcrun", *args],
        check=True,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        timeout=timeout,
    ).stdout


def version(value):
    parts = tuple(int(part) for part in value.split("."))
    return parts + (0,) * (3 - len(parts))


def destination(explicit=""):
    if explicit:
        return explicit
    sdk = xcrun("--sdk", "iphonesimulator", "--show-sdk-version").strip()
    devices = json.loads(xcrun("simctl", "list", "devices", "available", "--json"))
    candidates = []
    for runtime, phones in devices["devices"].items():
        name = runtime.rpartition(".")[2]
        if not name.startswith("iOS-"):
            continue
        runtime_version = version(name[4:].replace("-", "."))
        for phone in phones:
            if phone["name"].startswith("iPhone") and runtime_version <= version(sdk):
                candidates.append(
                    (runtime_version, runtime, phone["deviceTypeIdentifier"])
                )
                break
    if not candidates:
        raise RuntimeError(f"no iPhone simulator for the iOS {sdk} simulator SDK")
    _, runtime, device_type = max(candidates)
    for attempt in range(1, 3):
        device = xcrun("simctl", "create", "odr-tests", device_type, runtime).strip()
        print(f"Booting test simulator {device}, attempt {attempt}/2", flush=True)
        try:
            xcrun("simctl", "bootstatus", device, "-b", timeout=180, capture=False)
            return f"platform=iOS Simulator,id={device}"
        except (subprocess.TimeoutExpired, subprocess.CalledProcessError) as error:
            print(f"Simulator boot failed: {error}", flush=True)
            for command in ("shutdown", "delete"):
                try:
                    xcrun("simctl", command, device, capture=False)
                except (
                    subprocess.TimeoutExpired,
                    subprocess.CalledProcessError,
                ) as cleanup:
                    print(f"Simulator cleanup failed: {cleanup}", flush=True)
    raise RuntimeError("iOS simulator failed to boot after two bounded attempts")


def main():
    selected = destination(os.environ.get("MATRIX_DESTINATION", ""))
    print(f"destination: {selected}", flush=True)
    with Path(os.environ["GITHUB_ENV"]).open("a") as env:
        env.write(f"DESTINATION={selected}\n")


if __name__ == "__main__":
    main()
