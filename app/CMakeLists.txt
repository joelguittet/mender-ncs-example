# @file      CMakeLists.txt
# @brief     mender-ncs-example project CMakeLists file
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

cmake_minimum_required(VERSION 3.20.0)

# Extra modules
list(APPEND EXTRA_ZEPHYR_MODULES
    "${CMAKE_CURRENT_SOURCE_DIR}/../modules/msgpack-c"
)

# Declare project
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mender-ncs-example)

# Sources
target_sources(app PRIVATE "src/main.c")

# Definitions
target_compile_definitions(app PRIVATE PROJECT_NAME="mender-ncs-example")
target_compile_definitions(app PRIVATE _POSIX_C_SOURCE=200809L) # Required for strdup support

# Generate Root CA include files
generate_inc_file_for_target(app "src/AmazonRootCA1.cer" "${ZEPHYR_BINARY_DIR}/include/generated/AmazonRootCA1.cer.inc")
generate_inc_file_for_target(app "src/GoogleTrustServicesR4.crt" "${ZEPHYR_BINARY_DIR}/include/generated/GoogleTrustServicesR4.crt.inc")
