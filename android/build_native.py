#!/usr/bin/env python3
"""Build the native half of the AAR: `libodr_jni.so` per android ABI, laid out
the way `build.gradle.kts` expects.

    android/build_native.py --abi x86_64 --abi armv8

Each ABI gets its own conan install (`android-<arch>` host profile) and cmake
build under `android/native/build/<arch>`, and the results are copied into
`--output` (default `android/native/prebuilt`), which the gradle build reads as
its jniLibs source set:

    prebuilt/jniLibs/<abi>/libodr_jni.so     the bindings, core linked in
    prebuilt/jniLibs/<abi>/libc++_shared.so  from the NDK, see below

`libc++_shared.so` has to be shipped because the android profiles build against
the shared c++ runtime and nothing else in a consuming app pulls it in.

Neither is stripped: the NDK's unconditional `-g` is what a consumer's
`ndk.debugSymbolLevel` turns into symbolicated play crash reports. Costs 60-72 MB
per ABI, and needs the `keepDebugSymbols` rule in `build.gradle.kts` to survive.
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
ANDROID_ROOT = REPO_ROOT / "android"
PROFILE_DIR = REPO_ROOT / ".github" / "config" / "conan" / "profiles"

# conan architecture -> android ABI directory name, and the NDK toolchain triple
# `libc++_shared.so` lives under
ABIS = {
    "armv7": ("armeabi-v7a", "arm-linux-androideabi"),
    "armv8": ("arm64-v8a", "aarch64-linux-android"),
    "x86": ("x86", "i686-linux-android"),
    "x86_64": ("x86_64", "x86_64-linux-android"),
}


def run(command: list[str], **kwargs) -> None:
    print("+ " + " ".join(str(part) for part in command), flush=True)
    subprocess.run([str(part) for part in command], check=True, **kwargs)


def ndk_path(conan: str, host_profile: str, build_profile: str) -> Path:
    """The NDK the host profile points at, straight out of the rendered profile."""
    result = subprocess.run(
        [conan, "profile", "show", "--format=json",
         "--profile:host", host_profile, "--profile:build", build_profile],
        check=True,
        capture_output=True,
        text=True,
    )
    path = json.loads(result.stdout)["host"]["conf"].get("tools.android:ndk_path")
    if not path:
        raise SystemExit(f"'{host_profile}' does not set tools.android:ndk_path")
    return Path(path)


def libcxx_shared(ndk: Path, triple: str) -> Path:
    matches = sorted(ndk.glob(f"toolchains/llvm/prebuilt/*/sysroot/usr/lib/{triple}/libc++_shared.so"))
    if not matches:
        raise SystemExit(f"no libc++_shared.so for {triple} under {ndk}")
    return matches[0]


def report(library: Path) -> None:
    print(f"  {library.name}: {library.stat().st_size // 1024} KiB", flush=True)


def build(architecture: str, conan: str, build_profile: str, output: Path) -> None:
    abi, triple = ABIS[architecture]
    host_profile = str(PROFILE_DIR / f"android-{architecture}")
    build_dir = ANDROID_ROOT / "native" / "build" / architecture
    cmake_dir = build_dir / "cmake"

    run([conan, "install", REPO_ROOT,
         "--options", "&:with_jni=True",
         "--profile:host", host_profile,
         "--profile:build", build_profile,
         "--output-folder", build_dir,
         "--build", "missing"])

    run(["cmake", "-B", cmake_dir, "-S", REPO_ROOT,
         "-DCMAKE_TOOLCHAIN_FILE=" + str(build_dir / "conan_toolchain.cmake"),
         "-DCMAKE_BUILD_TYPE=Release",
         # one self-contained library: the core is linked into the bindings
         # rather than shipped as a second .so the app would have to load
         "-DBUILD_SHARED_LIBS=OFF",
         "-DODR_JNI=ON",
         # the AAR compiles `jni/java/` itself, so the native half is all we
         # want out of cmake — and no JDK is needed to produce it
         "-DODR_JNI_JAR=OFF",
         "-DODR_CLI=OFF",
         "-DODR_TEST=OFF",
         "-DODR_WITH_HTTP_SERVER=ON"])
    run(["cmake", "--build", cmake_dir, "--target", "odr_jni",
         "--parallel", str(os.cpu_count() or 1)])

    ndk = ndk_path(conan, host_profile, build_profile)
    jni_libs = output / "jniLibs" / abi
    jni_libs.mkdir(parents=True, exist_ok=True)
    for source in (cmake_dir / "jni" / "libodr_jni.so", libcxx_shared(ndk, triple)):
        target = jni_libs / source.name
        shutil.copy2(source, target)
        report(target)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--abi", dest="abis", action="append", choices=sorted(ABIS),
                       help="conan architecture to build; repeatable, defaults to all")
    parser.add_argument("--conan", default=os.environ.get("ODR_CONAN", "conan"),
                        help="conan executable (ODR_CONAN)")
    parser.add_argument("--build-profile", default="default",
                        help="conan build profile, i.e. the profile of this machine")
    parser.add_argument("--output", type=Path, default=ANDROID_ROOT / "native" / "prebuilt",
                        help="where to lay out jniLibs/")
    args = parser.parse_args()

    if not os.environ.get("ANDROID_HOME"):
        raise SystemExit("ANDROID_HOME is not set; the android profiles resolve the NDK from it")

    for architecture in args.abis or sorted(ABIS):
        build(architecture, args.conan, args.build_profile, args.output.resolve())
    return 0


if __name__ == "__main__":
    sys.exit(main())
