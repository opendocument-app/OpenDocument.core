import importlib.util
import json
import subprocess
import unittest
from pathlib import Path
from unittest.mock import call, patch

SPEC = importlib.util.spec_from_file_location(
    "apple_test_destination",
    Path(__file__).resolve().parents[1] / "apple_test_destination.py",
)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def devices(*versions):
    return json.dumps(
        {
            "devices": {
                f"com.apple.CoreSimulator.SimRuntime.iOS-{version}": [
                    {
                        "name": "iPhone 17 Pro",
                        "udid": "existing-device",
                        "deviceTypeIdentifier": "com.apple.CoreSimulator.SimDeviceType.iPhone-17-Pro",
                    }
                ]
                for version in versions
            }
        }
    )


class AppleTestDestination(unittest.TestCase):
    def test_explicit_macos_destination_never_uses_simctl(self):
        with patch.object(MODULE, "xcrun") as run:
            self.assertEqual(
                MODULE.destination("platform=macOS,arch=arm64"),
                "platform=macOS,arch=arm64",
            )
        run.assert_not_called()

    def test_newest_compatible_runtime_gets_a_fresh_device(self):
        with patch.object(
            MODULE,
            "xcrun",
            side_effect=[
                "26.0\n",
                devices("18-6", "26-0", "26-1"),
                "new-device\n",
                None,
            ],
        ) as run:
            self.assertEqual(
                MODULE.destination(), "platform=iOS Simulator,id=new-device"
            )
        self.assertEqual(
            run.call_args_list[2],
            call(
                "simctl",
                "create",
                "odr-tests",
                "com.apple.CoreSimulator.SimDeviceType.iPhone-17-Pro",
                "com.apple.CoreSimulator.SimRuntime.iOS-26-0",
            ),
        )
        self.assertEqual(
            run.call_args_list[3],
            call(
                "simctl",
                "bootstatus",
                "new-device",
                "-b",
                timeout=180,
                capture=False,
            ),
        )

    def test_stalled_boot_is_cleaned_up_and_retried_with_a_new_device(self):
        with patch.object(
            MODULE,
            "xcrun",
            side_effect=[
                "26.0",
                devices("26-0"),
                "first",
                subprocess.TimeoutExpired("bootstatus", 180),
                None,
                None,
                "second",
                None,
            ],
        ) as run:
            self.assertEqual(MODULE.destination(), "platform=iOS Simulator,id=second")
        self.assertEqual(
            run.call_args_list[4:6],
            [
                call("simctl", "shutdown", "first", capture=False),
                call("simctl", "delete", "first", capture=False),
            ],
        )

    def test_repeated_failure_stops_even_if_cleanup_stalls(self):
        timeout = subprocess.TimeoutExpired("simctl", 180)
        with patch.object(
            MODULE,
            "xcrun",
            side_effect=[
                "26.0",
                devices("26-0"),
                "first",
                timeout,
                timeout,
                timeout,
                "second",
                subprocess.CalledProcessError(1, "bootstatus"),
                None,
                None,
            ],
        ) as run:
            with self.assertRaisesRegex(RuntimeError, "two bounded attempts"):
                MODULE.destination()
        self.assertEqual(run.call_count, 10)

    def test_missing_compatible_runtime_fails_before_creating_a_device(self):
        with patch.object(
            MODULE, "xcrun", side_effect=["26.0", devices("26-1")]
        ) as run:
            with self.assertRaisesRegex(RuntimeError, "no iPhone simulator"):
                MODULE.destination()
        self.assertEqual(run.call_count, 2)

    def test_slow_simulator_listing_is_retried(self):
        with patch.object(
            MODULE,
            "xcrun",
            side_effect=[
                "26.0",
                subprocess.TimeoutExpired("simctl", 120),
                devices("26-0"),
                "new-device",
                None,
            ],
        ) as run:
            self.assertEqual(
                MODULE.destination(), "platform=iOS Simulator,id=new-device"
            )
        self.assertEqual(run.call_args_list[1].kwargs["timeout"], 120)

    def test_simulator_listing_gives_up_after_two_timeouts(self):
        timeout = subprocess.TimeoutExpired("simctl", 120)
        with patch.object(
            MODULE, "xcrun", side_effect=["26.0", timeout, timeout]
        ) as run:
            with self.assertRaisesRegex(RuntimeError, "timed out twice"):
                MODULE.destination()
        self.assertEqual(run.call_count, 3)

    def test_every_simctl_command_has_a_timeout(self):
        with patch.object(MODULE.subprocess, "run") as run:
            MODULE.xcrun("simctl", "list")
        self.assertEqual(run.call_args.kwargs["timeout"], 30)


if __name__ == "__main__":
    unittest.main()
