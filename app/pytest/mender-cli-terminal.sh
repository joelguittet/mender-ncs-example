#!/bin/bash
# @file      mender-cli-terminal.sh
# @brief     mender-cli terminal wrapper
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

server=$1
device_id=$2
command=$3

# Create temporary directory for PTYs
temp_dir=$(mktemp -d /tmp/mender-cli-XXXXXX)

# Start socat with explicit PTY links
socat -d -d pty,raw,echo=0,link=$temp_dir/pty1 pty,raw,echo=0,link=$temp_dir/pty2 > /dev/null 2>&1 &
SOCAT_PID=$!
sleep 3

# Start mender-cli terminal
mender-cli terminal $device_id --server $server --skip-verify < $temp_dir/pty1 > $temp_dir/pty1 2>&1 &
MENDER_CLI_PID=$!
sleep 10

# Check if prompt is received
output=$(timeout 30s dd if=$temp_dir/pty2 iflag=nonblock status=none 2>/dev/null)
if [[ "$output" != *"\$"* ]]; then
    # Disconnect
    printf "\x1d" > $temp_dir/pty2
    sleep 3
    # Stop mender-cli if still executing
    kill -TERM $MENDER_CLI_PID > /dev/null 2>&1
    # Stop socat
    kill -TERM $SOCAT_PID > /dev/null 2>&1
    # Cleaning
    rm -rf $temp_dir
    exit 1
fi

# Print expected command to the terminal
printf "$command\n" > $temp_dir/pty2
sleep 3

# Disconnect
printf "\x1d" > $temp_dir/pty2
sleep 3

# Read result
timeout 30s dd if=$temp_dir/pty2 iflag=nonblock status=none 2>/dev/null | tail -n +2

# Stop mender-cli if still executing
kill -TERM $MENDER_CLI_PID > /dev/null 2>&1

# Stop socat
kill -TERM $SOCAT_PID > /dev/null 2>&1

# Cleaning
rm -rf $temp_dir

exit 0
