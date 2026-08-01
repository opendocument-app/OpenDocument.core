#!/usr/bin/env python3
"""Build `OdrCoreObjC.xcframework`: one `OdrCoreObjC.framework` per Apple slice,
assembled into the artifact `Package.swift` points at.

    apple/build_xcframework.py slice --profile apple-ios-armv8
    apple/build_xcframework.py assemble

Set `ODR_GIT_HEAD=v6.2.0` to build a release: the binary then reports the tag
instead of the commit it happened to be built from.

The sibling of `android/build_native.py`, and the same shape: each conan profile
gets its own conan install and cmake build under `apple/build/<profile>`, and
`assemble` merges the results.

A slice is one *platform*, not one architecture — the simulator and macOS slices
are each a `lipo` of two arch builds, because an xcframework may not contain two
entries for the same platform. Device and simulator are different platforms even
at the same arch, and it is the Mach-O `LC_BUILD_VERSION` that says which, not
the SDK path; a simulator binary mistagged as device is the classic
"both ios-arm64 represent two equivalent library definitions" failure, so it is
asserted rather than assumed.
"""

import argparse
import os
import plistlib
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
APPLE_ROOT = REPO_ROOT / "apple"
PROFILE_DIR = REPO_ROOT / ".github" / "config" / "conan" / "profiles"

FRAMEWORK = "OdrCoreObjC"

# xcframework slice -> the conan profiles whose binaries are lipo'd into it, and
# the platform `vtool` must report for each
SLICES = {
    "ios-arm64": {
        "profiles": ["apple-ios-armv8"],
        "platform": "IOS",
    },
    "ios-arm64_x86_64-simulator": {
        "profiles": ["apple-iossim-armv8", "apple-iossim-x86_64"],
        "platform": "IOSSIMULATOR",
    },
    "macos-arm64_x86_64": {
        "profiles": ["apple-macos-armv8", "apple-macos-x86_64"],
        "platform": "MACOS",
    },
}

PROFILES = [profile for slice in SLICES.values() for profile in slice["profiles"]]


def run(command: list[str], **kwargs) -> None:
    print("+ " + " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], check=True, **kwargs)


def capture(command: list[str]) -> str:
    return subprocess.run(
        [str(part) for part in command], check=True, capture_output=True, text=True
    ).stdout


def framework_dir(build_dir: Path) -> Path:
    return build_dir / "cmake" / "apple" / f"{FRAMEWORK}.framework"


def binary_in(framework: Path) -> Path:
    """macOS frameworks are versioned, iOS ones are flat."""
    versioned = framework / "Versions" / "A" / FRAMEWORK
    return versioned if versioned.exists() else framework / FRAMEWORK


def build(profile: str, conan: str, build_profile: str) -> None:
    build_dir = APPLE_ROOT / "build" / profile
    cmake_dir = build_dir / "cmake"

    run([conan, "install", REPO_ROOT,
         "--options", "&:with_apple=True",
         "--profile:host", str(PROFILE_DIR / profile),
         "--profile:build", build_profile,
         "--output-folder", build_dir,
         "--build", "missing"])

    # A release identifies itself by its tag rather than by the commit it was
    # built from. That is what stops `Package.swift`'s checksum chasing its own
    # tail: writing the checksum makes a new commit, and a binary that embeds
    # the working-tree sha would change with it.
    release = os.environ.get("ODR_GIT_HEAD")
    version = [
        f"-DGIT_HEAD_SHA1={release}",
        "-DGIT_IS_DIRTY=false",
        f"-DODR_APPLE_BUNDLE_VERSION={release.lstrip('v')}",
    ] if release else []

    run(["cmake", "-B", cmake_dir, "-S", REPO_ROOT,
         "-DCMAKE_TOOLCHAIN_FILE=" + str(build_dir / "conan_toolchain.cmake"),
         "-DCMAKE_BUILD_TYPE=Release",
         *version,
         # one self-contained dylib: odrcore and every dependency are linked
         # into the framework rather than shipped alongside it
         "-DBUILD_SHARED_LIBS=OFF",
         "-DODR_APPLE=ON",
         "-DODR_CLI=OFF",
         "-DODR_TEST=OFF",
         "-DODR_JNI=OFF",
         "-DODR_PYTHON=OFF",
         "-DODR_WITH_HTTP_SERVER=ON",
         "-DODR_BUNDLE_ASSETS=ON"])
    run(["cmake", "--build", cmake_dir, "--target", "odr_apple",
         "--parallel", str(os.cpu_count() or 1)])

    framework = framework_dir(build_dir)
    binary = binary_in(framework)

    # dSYM before stripping, or there is nothing left to symbolicate with
    run(["dsymutil", binary, "-o", build_dir / f"{FRAMEWORK}.framework.dSYM"])
    run(["strip", "-x", binary])


def assert_platform(binary: Path, expected: str) -> None:
    output = capture(["vtool", "-show-build-version", str(binary)])
    for line in output.splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[0] == "platform":
            if parts[1] != expected:
                raise SystemExit(
                    f"{binary} is tagged {parts[1]}, expected {expected}. An "
                    f"xcframework cannot hold two slices of the same platform, "
                    f"and a mistagged one is rejected or silently unusable.")
            return
    raise SystemExit(f"{binary} has no LC_BUILD_VERSION")


def assert_install_name(binary: Path) -> None:
    output = capture(["otool", "-D", str(binary)]).splitlines()
    install_name = output[-1].strip() if len(output) > 1 else ""
    if not install_name.startswith("@rpath/"):
        raise SystemExit(
            f"{binary} has install name '{install_name}', expected an @rpath one")


def assert_contents(framework: Path) -> None:
    """A framework missing its headers, module map or resources builds and
    publishes happily and then fails at the consumer, so make it a build error
    here — the same reason `android/build.gradle.kts` has `checkNative`."""
    root = framework / "Versions" / "A"
    if not root.exists():
        root = framework
    # Resources are flat on iOS and under `Resources/` only in the versioned
    # macOS layout — a bundle that gets that wrong does not load at all.
    resources = root / "Resources" if (root / "Resources").is_dir() else root
    required = [
        root / "Headers" / f"{FRAMEWORK}.h",
        root / "Modules" / "module.modulemap",
        resources / "document.css",
    ]
    missing = [path for path in required if not path.exists()]
    if missing:
        raise SystemExit(
            "framework is incomplete: " + ", ".join(str(p) for p in missing))

    plist = resources / "Info.plist" if (
        resources / "Info.plist").exists() else root / "Info.plist"
    with plist.open("rb") as stream:
        info = plistlib.load(stream)
    for key in ("MinimumOSVersion", "CFBundleSupportedPlatforms"):
        if key not in info:
            raise SystemExit(
                f"{plist} has no {key}; App Store validation rejects an "
                f"embedded framework without it")
    # An empty value is what a placeholder nothing filled in leaves behind, and
    # it is not caught by presence alone. `CFBundleExecutable` empty is fatal:
    # installd refuses the app that embeds the framework.
    for key in ("CFBundleExecutable", "CFBundleName", "CFBundleIdentifier",
                "CFBundleVersion", "CFBundleShortVersionString"):
        if not info.get(key):
            raise SystemExit(f"{plist} has an empty {key}")


def assemble(output: Path, only: list[str] | None = None) -> None:
    """Merges the built slices into the xcframework.

    `only` narrows it to a subset, which is for a consumer that builds this
    itself and needs one platform — a release always ships all of them.
    """
    unknown = set(only or []) - set(SLICES)
    if unknown:
        raise SystemExit(f"no such slice: {', '.join(sorted(unknown))}")

    staging = APPLE_ROOT / "build" / "slices"
    shutil.rmtree(staging, ignore_errors=True)

    arguments: list[str] = []
    for name, slice in SLICES.items():
        if only and name not in only:
            continue
        profiles = slice["profiles"]
        sources = [framework_dir(APPLE_ROOT / "build" / p) for p in profiles]
        for source in sources:
            if not source.exists():
                raise SystemExit(
                    f"{source} is missing — run `slice` for every profile first")

        merged = staging / name / f"{FRAMEWORK}.framework"
        merged.parent.mkdir(parents=True, exist_ok=True)
        shutil.copytree(sources[0], merged, symlinks=True)

        binary = binary_in(merged)
        binary.unlink()
        run(["lipo", "-create", *[binary_in(s) for s in sources],
             "-output", binary])

        assert_platform(binary, slice["platform"])
        assert_install_name(binary)
        assert_contents(merged)

        # the dSYM of the first arch; enough to symbolicate that slice
        symbols = APPLE_ROOT / "build" / profiles[0] / f"{FRAMEWORK}.framework.dSYM"
        arguments += ["-framework", str(merged.resolve())]
        if symbols.exists():
            arguments += ["-debug-symbols", str(symbols.resolve())]

    shutil.rmtree(output, ignore_errors=True)
    run(["xcodebuild", "-create-xcframework", *arguments, "-output", str(output)])

    # `ditto`, not `zip`: the macOS slice is a versioned bundle full of
    # symlinks, and a plain `zip -r` follows them into a tree that checksums
    # fine and then fails to load
    archive = output.with_suffix(".xcframework.zip")
    archive.unlink(missing_ok=True)
    run(["ditto", "-c", "-k", "--keepParent", str(output), str(archive)])
    print(f"\n{archive} ({archive.stat().st_size // 1024 // 1024} MB)")


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--conan", default=os.environ.get("ODR_CONAN", "conan"),
                        help="conan executable (ODR_CONAN)")
    subparsers = parser.add_subparsers(dest="command", required=True)

    slice_parser = subparsers.add_parser("slice", help="build one profile")
    slice_parser.add_argument("--profile", dest="profiles", action="append",
                              choices=PROFILES,
                              help="conan profile to build; repeatable, "
                                   "defaults to all")
    slice_parser.add_argument("--build-profile", default="apple-macos-armv8",
                              help="conan build profile, i.e. this machine")

    assemble_parser = subparsers.add_parser(
        "assemble", help="merge the built slices into an xcframework")
    assemble_parser.add_argument(
        "--output", type=Path, default=REPO_ROOT / f"{FRAMEWORK}.xcframework")
    assemble_parser.add_argument(
        "--slice", action="append", dest="slices", choices=list(SLICES),
        help="only this slice; repeatable, defaults to all of them")

    args = parser.parse_args()

    if sys.platform != "darwin":
        raise SystemExit("this needs an Apple toolchain")

    if args.command == "slice":
        for profile in args.profiles or PROFILES:
            build(profile, args.conan, args.build_profile)
    else:
        assemble(args.output.resolve(), args.slices)
    return 0


if __name__ == "__main__":
    sys.exit(main())
