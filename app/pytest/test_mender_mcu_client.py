# @file      test_mender_mcu_client.py
# @brief     Mender MCU client test file
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


import logging
import pytest
import random
import re
import uuid
from datetime import datetime, timezone
from pathlib import Path
from twister_harness import DeviceAdapter
from twister_harness.helpers.utils import find_in_config
from mender_cli import MenderCli
from mender_management import MenderManagement
from helpers import Helpers

logger = logging.getLogger(__name__)

mender_cli = MenderCli()
mender_management = MenderManagement()
helpers = Helpers()

@pytest.mark.order('first')
@pytest.mark.dependency()
def test_device_registration(dut: DeviceAdapter):
    """Get the MAC address of the device."""
    mac_address = helpers.get_device_mac_address(dut)
    """Ensure the device is not authorized to the mender server."""
    helpers.assert_device_not_authenticated(dut)
    device = mender_management.get_device_by_mac(mac_address)
    assert device is not None, f"🔴​​​ Device with MAC '{mac_address}' not found on mender server"
    assert device['status'] == "pending", f"🔴​​​ Device with MAC '{mac_address}' has unexpected status '{device['status']}'"
    print("✅ Device has status 'pending' on the mender server")
    """Accept the device on the mender server."""
    mender_management.accept_device(device)
    """Ensure the device is authorized on the mender server."""
    helpers.assert_device_authenticated(dut)
    device = mender_management.get_device_by_mac(mac_address)
    assert device is not None, f"🔴​​​ Device with MAC '{mac_address}' not found on mender server"
    assert device['status'] == "accepted", f"🔴​​​ Device with MAC '{mac_address}' has unexpected status '{device['status']}'"
    print("✅ Device has status 'accepted' on the mender server")
    print("✅ Device registration done successfully on the mender server")


@pytest.mark.dependency(depends=['test_device_registration'])
def test_device_inventory(dut: DeviceAdapter):
    """Check if inventory add-on is built-in."""
    if not find_in_config(Path(dut.device_config.app_build_dir) / 'zephyr' / '.config', 'CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY'):
        pytest.skip("Inventory add-on is not built-in")
    """Get the MAC address of the device."""
    mac_address = helpers.get_device_mac_address(dut)
    """Ensure the device is authorized on the mender server."""
    helpers.assert_device_authenticated(dut)
    device = mender_management.get_device_by_mac(mac_address)
    assert device is not None, f"🔴​​​ Device with MAC '{mac_address}' not found on mender server"
    assert device['status'] == "accepted", f"🔴​​​ Device with MAC '{mac_address}' has unexpected status '{device['status']}'"
    print("✅ Device has status 'accepted' on the mender server")
    """Wait until the device inventory is updated."""
    def get_device_inventory():
        inventory = mender_management.get_device_inventory(device)
        print(f"🔵 Device inventory at {datetime.now(timezone.utc)} is: {inventory}")
        if inventory is not None and inventory.get('updated_ts'):
            return inventory, inventory.get('updated_ts')
        return None, None
    inventory = helpers.run_until_data_refreshed(get_device_inventory)
    assert inventory is not None, "🔴​​​ Device inventory not found"
    """Ensure of the device inventory content."""
    inventory = {item['name']: item['value'] for item in inventory.get('attributes', [])}
    print(f"Device reported inventory: '{inventory}'")
    expected_inventory = {}
    expected_inventory['artifact_name'] = f"{find_in_config(Path(dut.device_config.app_build_dir) / 'zephyr' / '.config', 'CONFIG_EXAMPLE_ARTIFACT_NAME_PREFIX')}-v0.1.0"
    expected_inventory['rootfs-image.version'] = f"{find_in_config(Path(dut.device_config.app_build_dir) / 'zephyr' / '.config', 'CONFIG_EXAMPLE_ARTIFACT_NAME_PREFIX')}-v0.1.0"
    expected_inventory['device_type'] = find_in_config(Path(dut.device_config.app_build_dir) / 'zephyr' / '.config', 'CONFIG_EXAMPLE_DEVICE_TYPE')
    expected_inventory['latitude'] = "45.8325"
    expected_inventory['longitude'] = "6.864722"
    expected_inventory['mender-mcu-client'] = "[0-9]+.[0-9]+.[0-9]+"
    expected_inventory['zephyr-rtos'] = "[0-9]+.[0-9]+.[0-9]+"
    print(f"Device expected inventory: '{expected_inventory}'")
    for key, val in expected_inventory.items():
        assert inventory.get(key) is not None, "🔴​​​ Device inventory is invalid"
        m = re.search(val, inventory.get(key))
        assert m is not None, "🔴​​​ Device inventory is invalid"
    print("✅ Device inventory contains expected inventory items")


@pytest.mark.dependency(depends=['test_device_registration'])
def test_device_configure(dut: DeviceAdapter):
    """Check if configure add-on is built-in."""
    if not find_in_config(Path(dut.device_config.app_build_dir) / 'zephyr' / '.config', 'CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE'):
        pytest.skip("Configure add-on is not built-in")
    """Get the MAC address of the device."""
    mac_address = helpers.get_device_mac_address(dut)
    """Ensure the device is authorized on the mender server."""
    helpers.assert_device_authenticated(dut)
    device = mender_management.get_device_by_mac(mac_address)
    assert device is not None, f"🔴​​​ Device with MAC '{mac_address}' not found on mender server"
    assert device['status'] == "accepted", f"🔴​​​ Device with MAC '{mac_address}' has unexpected status '{device['status']}'"
    print("✅ Device has status 'accepted' on the mender server")
    """Generate a random configuration content with several uuids."""
    """Generating such configuration ensure very easily the previous one was different."""
    expected_configuration = {f"uuid{index+1}": str(uuid.uuid4()) for index in range(random.randint(4, 8))}
    """Set device configuration."""
    mender_management.set_device_configuration(device, expected_configuration)
    deployment = mender_management.deploy_device_configuration(device)
    assert deployment is not None, "🔴​​​ Deployment not found"
    """"Wait for deployment completes or fails."""
    deployment_status = mender_management.wait_for_deployment(deployment=deployment)
    assert deployment_status is not None, "🔴​​​ Deployment status not found"
    if deployment_status['status'] != 'finished':
        raise AssertionError(f"🔴​​​ Deployment did not complete successfully, it has unexpected status '{deployment_status}'")
    if deployment_status.get('statistics').get('status').get('success') != 1:
        raise AssertionError(f"🔴​​​ Deployment did not complete successfully, it has unexpected status '{deployment_status}'")
    print("✅ Device configuration deployment done successfuly")
    """Wait until the device configuration is updated."""
    def get_device_configuration():
        configuration = mender_management.get_device_configuration(device)
        print(f"🔵 Device configuration at {datetime.now(timezone.utc)} is: {configuration}")
        if configuration is not None and configuration.get('reported_ts') and datetime.fromisoformat(configuration.get('reported_ts')) >= datetime.fromisoformat(deployment_status.get('finished')):
            return configuration, configuration.get('reported_ts')
        return None, None
    configuration = helpers.run_until_data_refreshed(get_device_configuration)
    assert configuration is not None, "🔴​​​ Device configuration not found"
    """Ensure of the device configuration content."""
    print(f"Device reported configuration: '{configuration['reported']}'")
    print(f"Device expected configuration: '{expected_configuration}'")
    if configuration['reported'] != expected_configuration:
        raise AssertionError("🔴​​​ Device configuration is invalid")
    print("✅ Device configuration contains expected configuration items")


@pytest.mark.dependency(depends=['test_device_registration'])
def test_device_troubleshoot_shell(dut: DeviceAdapter):
    """Check if troubleshoot add-on shell is built-in."""
    if not find_in_config(Path(dut.device_config.app_build_dir) / 'zephyr' / '.config', 'CONFIG_MENDER_CLIENT_TROUBLESHOOT_SHELL'):
        pytest.skip("Troubleshoot add-on shell is not built-in")
    """Get the MAC address of the device."""
    mac_address = helpers.get_device_mac_address(dut)
    """Ensure the device is authorized on the mender server."""
    helpers.assert_device_authenticated(dut)
    device = mender_management.get_device_by_mac(mac_address)
    assert device is not None, f"🔴​​​ Device with MAC '{mac_address}' not found on mender server"
    assert device['status'] == "accepted", f"🔴​​​ Device with MAC '{mac_address}' has unexpected status '{device['status']}'"
    print("✅ Device has status 'accepted' on the mender server")
    """Ensure the device troubleshoot is connected to the mender server."""
    helpers.assert_device_troubleshoot_connected(dut)
    """Execute 'help' command on troubleshoot shell."""
    result = mender_cli.shell_exec(device, "help")
    m = re.search(".*Please refer to shell documentation for more details.*", result.stdout)
    assert m is not None, "🔴​​​ Device troubleshoot shell result is invalid"
    print("✅ Device troubleshoot shell working as expected")


@pytest.mark.dependency(depends=['test_device_registration'])
def test_device_valid_firmware_update(dut: DeviceAdapter):
    """Get the MAC address of the device."""
    mac_address = helpers.get_device_mac_address(dut)
    """Get the firmware version of the device."""
    fw_version = helpers.get_device_firmware_version(dut)
    assert fw_version == "0.1.0+0", f"🔴​​​ Device has invalid firmware version '{fw_version}'"
    print("✅ Device has expected firmware version")
    """Ensure the device is authorized on the mender server."""
    helpers.assert_device_authenticated(dut)
    device = mender_management.get_device_by_mac(mac_address)
    assert device is not None, f"🔴​​​ Device with MAC '{mac_address}' not found on mender server"
    assert device['status'] == "accepted", f"🔴​​​ Device with MAC '{mac_address}' has unexpected status '{device['status']}'"
    print("✅ Device has status 'accepted' on the mender server")
    """Create a new firmware update."""
    # Key file is optional and will be empty in case signature is not enabled
    # This is the case on esp32s3-devkitc board for which only the Espressif port of mcuboot supports signature verification
    expected_version = "0.2.0+0"
    mcuboot_pad_size = find_in_config(Path(dut.device_config.build_dir) / 'pm.config', 'PM_MCUBOOT_PAD_SIZE')
    mcuboot_primary_size = find_in_config(Path(dut.device_config.build_dir) / 'pm.config', 'PM_MCUBOOT_PRIMARY_SIZE')
    key_file = find_in_config(Path(dut.device_config.build_dir) / 'mcuboot' / 'zephyr' / '.config', 'CONFIG_BOOT_SIGNATURE_KEY_FILE')
    device_type = find_in_config(Path(dut.device_config.app_build_dir) / 'zephyr' / '.config', 'CONFIG_EXAMPLE_DEVICE_TYPE')
    artifact_name_prefix = find_in_config(Path(dut.device_config.app_build_dir) / 'zephyr' / '.config', 'CONFIG_EXAMPLE_ARTIFACT_NAME_PREFIX')
    artifact = helpers.create_mender_artifact(dut.device_config.app_build_dir, key_file, device_type, artifact_name_prefix, expected_version, mcuboot_pad_size, mcuboot_primary_size)
    deployment_name = f"{artifact_name_prefix}-v{expected_version}"
    artifact_name = f"{artifact_name_prefix}-v{expected_version}"
    """Upload artifact to the server."""
    mender_management.upload_artifact(artifact)
    try:
        """Deploy artifact to the device."""
        mender_management.deploy_device_firmware(deployment_name, artifact_name, [device])
        """"Wait for deployment completes or fails."""
        deployment_status = mender_management.wait_for_deployment(name=deployment_name)
        assert deployment_status is not None, "🔴​​​ Deployment status not found"
        if deployment_status['status'] != 'finished':
            raise AssertionError(f"🔴​​​ Deployment did not complete successfully, it has unexpected status '{deployment_status}'")
        if deployment_status.get('statistics').get('status').get('success') != 1:
            raise AssertionError(f"🔴​​​ Deployment did not complete successfully, it has unexpected status '{deployment_status}'")
    finally:
        """Remove artifact from the server (done even in case the above deployment fails)."""
        mender_management.delete_artifact(artifact_name)
    print("✅ Device firmware deployment done successfuly")
    """Get the firmware version of the device."""
    fw_version = helpers.get_device_firmware_version(dut)
    assert fw_version == expected_version, f"🔴​​​ Device has invalid firmware version '{fw_version}'"
    print("✅ Device has expected firmware version")


@pytest.mark.order('last')
@pytest.mark.dependency(depends=['test_device_registration'])
def test_device_decomissioning(dut: DeviceAdapter):
    """Get the MAC address of the device."""
    mac_address = helpers.get_device_mac_address(dut)
    """Ensure the device is authorized on the mender server."""
    helpers.assert_device_authenticated(dut)
    device = mender_management.get_device_by_mac(mac_address)
    assert device is not None, f"🔴​​​ Device with MAC '{mac_address}' not found on mender server"
    assert device['status'] == "accepted", f"🔴​​​ Device with MAC '{mac_address}' has unexpected status '{device['status']}'"
    print("✅ Device has status 'accepted' on the mender server")
    """Decomissioning of the device on the server."""
    mender_management.delete_device(device)
    """Ensure the device is not authorized to the mender server."""
    helpers.assert_device_not_authenticated(dut)
    print("✅ Device decomissioning done successfully on the mender server")
