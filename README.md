# mender-ncs-example

[![Workflow check-code-format Badge](https://github.com/joelguittet/mender-ncs-example/workflows/check-code-format/badge.svg)](https://github.com/joelguittet/mender-ncs-example/actions)
[![Issues Badge](https://img.shields.io/github/issues/joelguittet/mender-ncs-example)](https://github.com/joelguittet/mender-ncs-example/issues)
[![License Badge](https://img.shields.io/github/license/joelguittet/mender-ncs-example)](https://github.com/joelguittet/mender-ncs-example/blob/master/LICENSE)

[![Bugs](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_mender-ncs-example&metric=bugs)](https://sonarcloud.io/dashboard?id=joelguittet_mender-ncs-example)
[![Code Smells](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_mender-ncs-example&metric=code_smells)](https://sonarcloud.io/dashboard?id=joelguittet_mender-ncs-example)
[![Duplicated Lines (%)](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_mender-ncs-example&metric=duplicated_lines_density)](https://sonarcloud.io/dashboard?id=joelguittet_mender-ncs-example)
[![Lines of Code](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_mender-ncs-example&metric=ncloc)](https://sonarcloud.io/dashboard?id=joelguittet_mender-ncs-example)
[![Vulnerabilities](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_mender-ncs-example&metric=vulnerabilities)](https://sonarcloud.io/dashboard?id=joelguittet_mender-ncs-example)

[![Maintainability Rating](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_mender-ncs-example&metric=sqale_rating)](https://sonarcloud.io/dashboard?id=joelguittet_mender-ncs-example)
[![Reliability Rating](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_mender-ncs-example&metric=reliability_rating)](https://sonarcloud.io/dashboard?id=joelguittet_mender-ncs-example)
[![Security Rating](https://sonarcloud.io/api/project_badges/measure?project=joelguittet_mender-ncs-example&metric=security_rating)](https://sonarcloud.io/dashboard?id=joelguittet_mender-ncs-example)

[Mender MCU client](https://github.com/joelguittet/mender-mcu-client) is an open source over-the-air (OTA) library updater for MCU devices. This demonstration project runs on nRF7002-DK hardware using nRF Connect SDK based on Zephyr RTOS.


## Getting started

This project is used with a [nRF7002-DK](https://www.nordicsemi.com/Products/Development-hardware/nRF7002-DK) evaluation board. No additional wiring is required.

![nRF7002-DK](https://raw.githubusercontent.com/joelguittet/mender-ncs-example/master/.github/docs/nrf7002-dk.png)

The project is built using nRF Connect SDK v3.0.2 and Zephyr SDK v0.16.x. There is no other dependencies.

To start using Mender, we recommend that you begin with the Getting started section in [the Mender documentation](https://docs.mender.io).

To start using Zephyr, we recommend that you begin with the Getting started section in [the Zephyr documentation](https://docs.zephyrproject.org/latest/develop/getting_started/index.html). It is highly recommended to be familiar with Zephyr environment and tools to use this example.

To start using nRF Connect SDK, we recommend that you begin with the nRF Connect SDK [Get started page](https://www.nordicsemi.com/Products/Development-software/nRF-Connect-SDK/GetStarted). It is highly recommended to be familiar with nRF Connect SDK environment and tools to use this example.

### Initialize the project

Initiate a new workspace using the following commands. This will download the dependencies of the example application.

The projects depends on [cJSON](https://github.com/DaveGamble/cJSON) and [msgpack-c](https://github.com/msgpack/msgpack-c) (if troubleshoot add-on support is integrated, see below).

```
mkdir workspace-ncs
cd workspace-ncs
git clone https://github.com/joelguittet/mender-ncs-example
cd mender-ncs-example
git submodule update --init --recursive
west init -l .
west update
```

### Configuration of the application

The example application should first be configured to set at least:
- `CONFIG_MENDER_SERVER_TENANT_TOKEN` to set the Tenant Token of your account on "https://hosted.mender.io" server.
- `CONFIG_EXAMPLE_WIFI_SSID` to set the SSID of the Access Point.
- `CONFIG_EXAMPLE_WIFI_PSK` to set the password of the network.

You may want to customize few interesting settings:
- `CONFIG_MENDER_SERVER_HOST` if using your own Mender server instance. Tenant Token is not required in this case.
- `CONFIG_MENDER_CLIENT_AUTHENTICATION_POLL_INTERVAL` is the interval to retry authentication on the mender server.
- `CONFIG_MENDER_CLIENT_UPDATE_POLL_INTERVAL` is the interval to check for new deployments.
- `CONFIG_MENDER_CLIENT_INVENTORY_REFRESH_INTERVAL` is the interval to publish inventory data.
- `CONFIG_MENDER_CLIENT_CONFIGURE_REFRESH_INTERVAL` is the interval to refresh device configuration.

Other settings are available in the Kconfig. You can also refer to the mender-mcu-client API and configuration keys.

### Building and flashing the application

The nRF7002-DK development kit depends on binary blobs. You can download them using the following commands.

```
west blobs fetch nrf_wifi
```

The application relies on mcuboot and requires to build a signed binary file to be flashed on the evaluation board. The binary file generated by the nRF Connect SDK contains TF-M and the application itself.

Use the following commands to build mcuboot and the application all together:

```
west build -b nrf7002dk/nrf5340/cpuapp/ns app
```

Finaly flash the board using the following command. Both mcuboot and the application will be flashed.

```
west flash
```

### Execution of the application

After flashing the application on the development board and opening a terminal to display the logs, you should be able to see the following:

```
[00:00:16.106,597] <inf> mender_ncs_example: Running project 'mender-ncs-example' version '0.1.0'
[00:00:16.119,628] <inf> mender_ncs_example: Mender client version '0.12.3' initialized
[00:00:16.130,432] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/storage/generic/psa_its/src/mender-storage.c (165): Device configuration not
 available (-140)
[00:00:16.148,895] <inf> mender_ncs_example: Mender configure add-on registered
[00:00:16.158,813] <inf> mender_ncs_example: Mender inventory add-on registered
[00:00:16.168,701] <inf> mender_ncs_example: Device configuration retrieved
[00:00:16.202,758] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/storage/generic/psa_its/src/mender-storage.c (98): Deployment data not avail
able (-140)
[00:00:16.220,611] <inf> mender_ncs_example: Mender client connect network
[00:00:17.719,818] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted.mender.io:4
43' with IPv4 address '54.174.83.129'
[00:00:18.200,866] <err> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/core/src/mender-api.c (514): [401] Unauthorized: dev auth: unauthorized
[00:00:18.217,254] <inf> mender_ncs_example: Mender client authentication failed
[00:00:18.227,203] <inf> mender_ncs_example: Mender client released network
```

Which means you now have generated authentication keys on the device. You now have to accept your device on the mender interface. Once it is accepted on the mender interface the following will be displayed:

```
[00:01:16.178,527] <inf> mender_ncs_example: Mender client connect network
[00:01:17.225,952] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted.mender.io:4
43' with IPv4 address '54.174.83.129'
[00:01:17.807,678] <inf> mender_ncs_example: Mender client authenticated
[00:01:17.817,199] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/core/src/mender-client.c (903): Checking for deployment...
[00:01:18.772,003] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted.mender.io:4
43' with IPv4 address '54.174.83.129'
[00:01:19.229,614] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/core/src/mender-client.c (911): No deployment available
[00:01:19.243,988] <inf> mender_ncs_example: Mender client released network
[00:01:19.253,540] <inf> mender_ncs_example: Mender client connect network
[00:01:20.204,620] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted.mender.io:4
43' with IPv4 address '54.174.83.129'
[00:01:20.559,204] <inf> mender_ncs_example: Mender client released network
[00:01:20.568,817] <inf> mender_ncs_example: Mender client connect network
[00:01:21.555,053] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted.mender.io:4
43' with IPv4 address '98.87.139.180'
[00:01:22.078,308] <inf> mender_ncs_example: Mender client released network
```

Congratulation! Your device is connected to the mender server. Device type is `mender-nrf7002dk-ncs-example` and the current software version is displayed.

### Create a new deployment

First install [mender-artifact](https://docs.mender.io/downloads#mender-artifact) tool.

Change `VERSION` file to `VERSION_MAJOR=0` and `VERSION_MINOR=2`, rebuild the firmware using the same command already used.

```
west build -b nrf7002dk/nrf5340/cpuapp/ns app
```

Then create a new artifact using the following command line:

```
mender-artifact write rootfs-image --compression none --device-type mender-nrf7002dk-ncs-example --artifact-name mender-nrf7002dk-ncs-example-v0.2.0 --output-path build/app/zephyr/mender-nrf7002dk-ncs-example-v0.2.0.mender --file build/app/zephyr/zephyr.signed.bin
```

Upload the artifact `mender-nrf7002dk-ncs-example-v0.2.0.mender` to the mender server and create a new deployment.

The device checks for the new deployment, downloads the artifact and installs it on the update partition. Then it reboots to apply the update:

```
[00:06:17.817,169] <inf> mender_ncs_example: Mender client connect network
[00:06:17.826,629] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/core/src/mender-client.c (903): Checking for deployment...
[00:06:18.834,594] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted.mender.io:4
43' with IPv4 address '98.87.139.180'
[00:06:19.379,089] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/core/src/mender-client.c (930): Downloading deployment artifact with id '88f81b11-04e
3-42ff-9e3f-99e17fe4bcbc', artifact name 'mender-nrf7002dk-ncs-example-v0.2.0' and uri 'https://hosted-mender-artifacts.s3.amazonaws.com/6370b06a7f0deaedb279fb6
a/165c847f-c04e-4280-885c-4f4a555a0da4?X-A
[00:06:20.354,827] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted.mender.io:4
43' with IPv4 address '98.87.139.180'
[00:06:20.901,580] <inf> mender_ncs_example: Deployment status is 'downloading'
[00:06:22.089,477] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted-mender-arti
facts.s3.amazonaws.com:443' with IPv4 address '52.216.36.249'
[00:06:23.887,878] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/core/src/mender-artifact.c (374): Artifact has valid version
[00:06:23.909,454] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/flash/zephyr/dfu_target/src/mender-flash.c (48): Start flashing artifact 'ze
phyr.signed.bin' with size 849308
[00:07:55.832,275] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/core/src/mender-client.c (943): Download done, installing artifact
[00:07:56.811,859] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted.mender.io:4
43' with IPv4 address '54.174.83.129'
[00:07:57.253,906] <inf> mender_ncs_example: Deployment status is 'installing'
[00:07:57.264,923] <inf> dfu_target_mcuboot: MCUBoot image-0 upgrade scheduled. Reset device to apply
[00:07:58.320,129] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted.mender.io:4
43' with IPv4 address '54.174.83.129'
[00:07:58.796,661] <inf> mender_ncs_example: Deployment status is 'rebooting'
[00:07:58.806,640] <inf> mender_ncs_example: Mender client released network
[00:07:58.816,558] <inf> mender_ncs_example: Restarting system
uart:~$ [00:00:00.251,617] <inf> spi_nor: mx25r6435f@0: 8 MiBy flash
*** Booting MCUboot v2.1.0-dev-ae1ee57f3906 ***
*** Using nRF Connect SDK v3.0.2-89ba1294ac9b ***
*** Using Zephyr OS v4.0.99-f791c49f492c ***
[00:00:00.272,430] <inf> mcuboot: Starting bootloader
[00:00:00.279,205] <inf> mcuboot: Primary image: magic=good, swap_type=0x2, copy_done=0x1, image_ok=0x1
[00:00:00.289,215] <inf> mcuboot: Secondary image: magic=good, swap_type=0x2, copy_done=0x3, image_ok=0x3
[00:00:00.299,102] <inf> mcuboot: Boot source: none
[00:00:00.304,748] <inf> mcuboot: Image index: 0, Swap type: test
[00:00:02.576,232] <inf> mcuboot: Starting swap using move algorithm.
[00:01:33.073,333] <inf> mcuboot: Bootloader chainload address offset: 0x10000
[00:01:33.080,902] <inf> mcuboot: Image version: v0.1.0
�00:01:33.086,547] <inf> mcuboot: Jumping to the first image slot
[00:00:00.065,856] <inf> wifi_nrf: Configuring SLEEP CTRL GPIO control register

*** Booting mender-ncs-example v0.2.0-2dfba2ba8e4e ***
*** Using nRF Connect SDK v3.0.2-89ba1294ac9b ***
*** Using Zephyr OS v4.0.99-f791c49f492c ***
[00:00:00.237,976] <inf> mender_ncs_example: Initialization of network interface
[00:00:00.249,267] <inf> wifi_supplicant: wpa_supplicant initialized
Connected
[00:00:18.036,743] <inf> mender_ncs_example: IPv4 address: 192.168.1.123
[00:00:18.045,989] <inf> mender_ncs_example: Lease time: 43200 seconds
[00:00:18.055,114] <inf> mender_ncs_example: Subnet: 255.255.255.0
[00:00:18.063,842] <inf> mender_ncs_example: Router: 192.168.1.1
[00:00:18.073,120] <inf> mender_ncs_example: Initialization of certificate(s)
[00:00:18.082,855] <inf> mender_ncs_example: MAC address of the device 'f4:ce:36:00:1f:dc'
[00:00:18.093,658] <inf> mender_ncs_example: Running project 'mender-ncs-example' version '0.2.0'
[00:00:18.106,689] <inf> mender_ncs_example: Mender client version '0.12.3' initialized
[00:00:18.117,462] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/storage/generic/psa_its/src/mender-storage.c (165): Device configuration not
 available (-140)
[00:00:18.135,894] <inf> mender_ncs_example: Mender configure add-on registered
[00:00:18.145,812] <inf> mender_ncs_example: Mender inventory add-on registered
[00:00:18.155,700] <inf> mender_ncs_example: Device configuration retrieved
[00:00:18.190,002] <inf> mender_ncs_example: Mender client connect network
[00:00:19.218,872] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted.mender.io:4
43' with IPv4 address '54.174.83.129'
[00:00:19.672,698] <inf> mender_ncs_example: Mender client authenticated
[00:00:19.682,189] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/flash/zephyr/dfu_target/src/mender-flash.c (155): Application has been mark 
valid and rollback canceled
[00:00:20.672,760] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted.mender.io:4
43' with IPv4 address '54.174.83.129'
[00:00:21.091,125] <inf> mender_ncs_example: Deployment status is 'success'
[00:00:21.192,779] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/core/src/mender-client.c (903): Checking for deployment...
[00:00:22.194,366] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted.mender.io:4
43' with IPv4 address '98.87.139.180'
[00:00:22.601,379] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/core/src/mender-client.c (911): No deployment available
[00:00:22.615,661] <inf> mender_ncs_example: Mender client released network
[00:00:22.625,213] <inf> mender_ncs_example: Mender client connect network
[00:00:23.605,590] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted.mender.io:4
43' with IPv4 address '54.174.83.129'
[00:00:24.163,055] <inf> mender_ncs_example: Mender client released network
[00:00:24.172,698] <inf> mender_ncs_example: Mender client connect network
[00:00:25.153,472] <inf> mender: WEST_TOPDIR/modules/lib/mender-mcu-client/platform/net/zephyr/src/mender-net.c (156): Connected to the host 'hosted.mender.io:4
43' with IPv4 address '98.87.139.180'
[00:00:25.801,879] <inf> mender_ncs_example: Mender client released network
```

Congratulation! You have updated the device. Mender server displays the success of the deployment.

### Failure or wanted rollback

In case of failure to connect and authenticate to the server the current example application performs a rollback to the previous release.
You can customize the behavior of the example application to add your own checks and perform the rollback in case the tests fail.

### Building vscode nRF Connect SDK environment

As an alternative to using the command line, it is possible to build the application using vscode nRF Connect SDK environment.

Open the app in the vscode nRF Connect SDK environment and create a new build configuration. Select the SDK, the toolchain and the board target `nrf7002dk/nrf5340/cpuapp/ns`. Keep the other options empty (note you may want to create a Kconfig fragment to define your local configuration).

![Build Configuration](https://raw.githubusercontent.com/joelguittet/mender-ncs-example/master/.github/docs/build-configuration.png)

Build and flash mcuboot, TF-M and the application using `Build` and `Flash` buttons of the `Actions` panel in the nRF Connect SDK environment. 

### Using Device Troubleshoot add-on

The Device Troubleshoot add-on permits to display the Zephyr Shell on the Mender interface. Autocompletion and colors are available.

![Troubleshoot console](https://raw.githubusercontent.com/joelguittet/mender-ncs-example/master/.github/docs/troubleshoot.png)

The Device Troubleshoot add-on also permits to upload/download files to/from the Mender server and to perform port forwarding. However, due to limited FLASH memory remaining on the nRF5340, these feature are disabled in the current example.

The Device Troubleshoot add-on feature are integrated using `troubleshoot.conf` Kconfig fragment, for example using the following command.

```
west build -b nrf7002dk/nrf5340/cpuapp/ns app -- -DEXTRA_CONF_FILE="troubleshoot.conf"
```

### Using an other mender instance

The communication with the server is done using HTTPS. To get it working, the Root CA that is providing the server certificate should be integrated and registered in the application (see `tls_credential_add` in the `src/main.c` file). Format of the expected Root CA certificate is DER.

In this example we are using the `https://hosted.mender.io` server with an Enterprise account. While checking the details of the server certificate in your browser, you will see that is is provided by Amazon Root CA 1. Thus the Amazon Root CA 1 certificate `AmazonRootCA1.cer` retrieved at `https://www.amazontrust.com/repository` is integrated in the application.

Using another instance you may need to integrate another Root CA certificate. Also note depending of the certificate properties you may need to activate additional cipher suites in the TF-M configuration. See below considerations when using trial mender instance for example.

### Using a trial mender instance

The trial mender instance which is available for free at `http://hosted.mender.io` to test mender feature requires to integrate the Google Trust Services Root R4 certificate together with Amazon Root CA 1 certificate. This is required because the artifacts are saved on a Cloudflare server instead of the Amazon S3 storage. This is achieved using the secondary root certificate support provided by the mender-mcu-client.

```
CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_PRIMARY=1
CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_SECONDARY=2
```

However, TF-M profile type 'small' has only symmetric cipher to keep the size of the binary small. Particularly, the support of ECDSA with SHA384 which is required to parse the Google Trust Services Root R4 certificate need to be added to the configuration. This is achieved using the following configurations.

```
CONFIG_PSA_WANT_ALG_ECDH=y
CONFIG_PSA_WANT_ALG_ECDSA=y
CONFIG_PSA_WANT_ECC_SECP_R1_384=y
```

Those configurations are also available using the Kconfig fragment `trial.conf` in the build configuration, using the following command.

```
west build -b nrf7002dk/nrf5340/cpuapp/ns app -- -DEXTRA_CONF_FILE="trial.conf"
```

## License

Copyright joelguittet and mender-mcu-client contributors

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
