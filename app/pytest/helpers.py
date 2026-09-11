# @file      helpers.py
# @brief     Helpers
#
# Copyright joelguittet and mender-mcu-client contributors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import os
import re
import subprocess
import tempfile
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path
from twister_harness import DeviceAdapter
from twister_harness.exceptions import TwisterHarnessTimeoutException
from twister_harness.helpers.utils import match_no_lines


class Helpers:
    """
    Provides helper methods for device management.
    """

    def get_device_mac_address(self, dut: DeviceAdapter, timeout=60) -> str:
        try:
            output = dut.readlines_until(regex="MAC address of the device '[0-9A-Za-z:]+'", timeout=timeout)
        except (TwisterHarnessTimeoutException, AssertionError):
            raise AssertionError("🔴​​​ MAC address of the device not found")
        m = re.search("MAC address of the device '(?P<mac>[0-9A-Za-z:]+)'", output[-1])
        if m:
            mac_address = m.group('mac')
        else:
            raise AssertionError("🔴​​​ MAC address of the device not found")
        print(f"🔵​ MAC address of the device is '{mac_address}'")
        return mac_address

    def get_device_firmware_version(self, dut: DeviceAdapter, project_name: str, timeout=60) -> str:
        try:
            output = dut.readlines_until(regex=r"Running project '{project_name}' version '[0-9.+]+'", timeout=timeout)
        except (TwisterHarnessTimeoutException, AssertionError):
            raise AssertionError("🔴​​​ Firmware version of the device not found")
        m = re.search(f"Running project '{project_name}' version '(?P<version>[0-9.+]+)'", output[-1])
        if m:
            fw_version = m.group('version')
        else:
            raise AssertionError("🔴​​​ Firmware version of the device not found")
        print(f"🔵​ Firmware version of the device is '{fw_version}'")
        return fw_version

    def assert_device_not_authenticated(self, dut: DeviceAdapter, timeout=600):
        output = []
        # Try to catch 'Unauthorized' log
        try:
            output = dut.readlines_until(regex=r".*<err>.*\[401\] Unauthorized.*", timeout=timeout)
            print("✅ Device is not authenticated")
        except (TwisterHarnessTimeoutException, AssertionError):
            # Check if device is still authenticated
            try:
                match_no_lines(output, ["Mender client authenticated"])
            except (AssertionError):
                raise AssertionError("🔴​​​ Device is authenticated to the server but it should not")
            raise AssertionError("🔴​​​ Device is not communicating with the server")

    def assert_device_authenticated(self, dut: DeviceAdapter, timeout=600):
        output = []
        # Try to catch 'Mender client authenticated' log
        try:
            output = dut.readlines_until(regex=r".*<inf>.*Mender client authenticated.*", timeout=timeout)
            print("✅ Device is authenticated")
        except (TwisterHarnessTimeoutException, AssertionError):
            # Check if device is unauthorized
            try:
                match_no_lines(output, [r"\[401\] Unauthorized"])
            except (AssertionError):
                raise AssertionError("🔴​​​ Device is not authenticated to the server but it should")
            raise AssertionError("🔴​​​ Device is not communicating with the server")

    def assert_device_troubleshoot_connected(self, dut: DeviceAdapter, timeout=600):
        try:
            dut.readlines_until(regex="Troubleshoot client connected", timeout=timeout)
        except (TwisterHarnessTimeoutException, AssertionError):
            raise AssertionError("🔴​​​ Device troubleshoot doesn't appear to be connected but it should")
        print("✅ Device troubleshoot connected")

    def create_mender_artifact(self, build_dir: Path, key_file: Path, device_type: str, artifact_name_prefix: str, version: str, mcuboot_pad_size: str, mcuboot_primary_size: str) -> Path:
        # zephyr.signed.bin file path
        zephyr_signed_bin = Path(build_dir) / "zephyr" / "zephyr.signed.bin"
        # Work in a temporary directory
        with tempfile.TemporaryDirectory() as tmpdir:
            # Retrieve image information from zephyr.signed.bin file
            cmd=f"imgtool dumpinfo {zephyr_signed_bin}"
            try:
                result = subprocess.run(cmd, cwd=Path(tmpdir), shell=True, capture_output=True, text=True, check=True)
            except subprocess.CalledProcessError as e:
                print(f"🔴​​​ Failed to create artifact, imgtool dumpinfo failed with exit code {e.returncode}, stdout {e.stdout} and stderr {e.stderr}")
                raise
            header = dict(re.findall(r"^\s*(\w+):\s*(0x[0-9a-fA-F]+)", result.stdout, re.MULTILINE))
            # Extract image payload from zephyr.signed.bin file
            cmd=f"dd if={zephyr_signed_bin} of=zephyr.bin bs=1 skip={int(header["hdr_size"], 16)} count={int(header["img_size"], 16)}"
            try:
                subprocess.run(cmd, cwd=Path(tmpdir), shell=True, check=True)
            except subprocess.CalledProcessError as e:
                print(f"🔴​​​ Failed to create artifact, dd failed with exit code {e.returncode}, stdout {e.stdout} and stderr {e.stderr}")
                raise
            # Overwrite zephyr.signed.bin file updating the version (key file is optional)
            cmd=f"imgtool sign --version {version} --align 4 --pad-header --header-size {mcuboot_pad_size} --slot-size {mcuboot_primary_size}"
            if os.path.exists(key_file):
                cmd += f" --key {key_file}"
            cmd += f" zephyr.bin {zephyr_signed_bin}"
            try:
                subprocess.run(cmd, cwd=Path(tmpdir), shell=True, check=True)
            except subprocess.CalledProcessError as e:
                print(f"🔴​​​ Failed to create artifact, imgtool sign failed with exit code {e.returncode}, stdout {e.stdout} and stderr {e.stderr}")
                raise
        # Create artifact using mender-artifact
        artifact_file = Path(build_dir) / "zephyr" / f"{artifact_name_prefix}-v{version}.mender"
        cmd = f"mender-artifact write rootfs-image --compression none --compatible-types {device_type} --artifact-name {artifact_name_prefix}-v{version} --output-path {artifact_file} --file {zephyr_signed_bin}"
        try:
            subprocess.run(cmd, shell=True, check=True)
        except subprocess.CalledProcessError as e:
            print(f"🔴​​​ Failed to create artifact, mender-artifact failed with exit code {e.returncode}, stdout {e.stdout} and stderr {e.stderr}")
            raise
        return artifact_file

    def run_until_data_refreshed(self, f, timeout=600, interval=30):
        start_time = time.time()
        while time.time() - start_time < timeout:
            data, updated_ts = f()
            if data is not None and updated_ts is not None:
                delta = interval * 2
                if datetime.fromisoformat(updated_ts) > datetime.now(timezone.utc) - timedelta(seconds=delta):
                    return data
            time.sleep(interval)
        return None

    def run_until_data_available(self, f, timeout=600, interval=30):
        start_time = time.time()
        while time.time() - start_time < timeout:
            data = f()
            if data is not None:
                return data
            time.sleep(interval)
        return None
