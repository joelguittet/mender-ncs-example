# @file      mender_cli.py
# @brief     mender-cli wrapper
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
import subprocess


class MenderCli:
    """
    Interface on top of mender-cli.
    Provides methods to accesss remote terminal.
    """

    def __init__(self,
                 server=None,
                 username=None,
                 password=None,
                 ca_cert=None):
        self.server = server or os.getenv("MENDER_SERVER", "https://mender.mycompany.local")
        self.username = username or os.getenv("MENDER_USERNAME", "admin@mycompany.local")
        self.password = password or os.getenv("MENDER_PASSWORD", "MySecretPassword")
        self.ca_cert = ca_cert or os.getenv("MENDER_CA", "/etc/ssl/certs/my-ca.crt")

        self.login()

    # ============================================================
    # AUTH
    # ============================================================
    def login(self, timeout=60):
        """Authenticate to Mender CLI using Basic Auth."""
        cmd = f"mender-cli login --skip-verify --server {self.server} --username {self.username} --password {self.password}"
        subprocess.run(cmd, shell=True, check=True, timeout=timeout)
        print("✅ Mender CLI logged in with Basic Auth")

    # ============================================================
    # SHELL
    # ============================================================
    def shell_exec(self, device, cmd, timeout=60):
        """Execute a shell command using Mender CLI interface."""
        cmd = f"./mender-cli-terminal.sh {self.server} {device['id']} {cmd}"
        result = subprocess.run(cmd, cwd=os.path.dirname(os.path.abspath(__file__)), shell=True, capture_output=True, text=True, check=True, timeout=timeout)
        return result
