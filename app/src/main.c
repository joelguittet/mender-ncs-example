/**
 * @file      main.c
 * @brief     Main entry point
 *
 * Copyright joelguittet and mender-mcu-client contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <assert.h>

#include <app_version.h>
#include <ncs_version.h>
#include <version.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(mender_ncs_example, LOG_LEVEL_INF);

#include <zephyr/kernel.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_mgmt.h>
#ifdef CONFIG_WIFI
#include <zephyr/net/wifi_mgmt.h>
#ifdef CONFIG_WIFI_NM_WPA_SUPPLICANT
#include <supp_events.h>
#endif /* CONFIG_WIFI_NM_WPA_SUPPLICANT */
#endif /* CONFIG_WIFI */
#include <zephyr/sys/reboot.h>

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER
#include <zephyr/fs/fs.h>
#include <zephyr/fs/littlefs.h>
#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER */
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT */

#ifdef CONFIG_NET_SOCKETS_SOCKOPT_TLS
#include <zephyr/net/tls_credentials.h>

/*
 * Amazon Root CA 1 certificate, retrieved from https://www.amazontrust.com/repository in DER format.
 * It is converted to include file in application CMakeLists.txt.
 */
#ifdef CONFIG_TLS_CREDENTIAL_FILENAMES
static const unsigned char ca_certificate_primary[] = "AmazonRootCA1.cer";
#else
static const unsigned char ca_certificate_primary[] = {
#include "AmazonRootCA1.cer.inc"
};
#endif /* CONFIG_TLS_CREDENTIAL_FILENAMES */

/*
 * Google Trust Services Root R4 certificate, retrieved from https://pki.goog/repository in DER format (original name is r4.crt).
 * It is converted to include file in application CMakeLists.txt.
 * This secondary Root CA certificate is to be used if the device is connected to a free hosted Mender account (for which artifacts are saved on a Cloudflare server instead of the Amazon S3 storage)
 */
#if (0 != CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_SECONDARY)
#ifdef CONFIG_TLS_CREDENTIAL_FILENAMES
static const unsigned char ca_certificate_secondary[] = "GoogleTrustServicesR4.crt";
#else
static const unsigned char ca_certificate_secondary[] = {
#include "GoogleTrustServicesR4.crt.inc"
};
#endif /* CONFIG_TLS_CREDENTIAL_FILENAMES */
#endif /* (0 != CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_SECONDARY) */

#endif /* CONFIG_NET_SOCKETS_SOCKOPT_TLS */

#include "mender-client.h"
#include "mender-configure.h"
#include "mender-flash.h"
#include "mender-inventory.h"
#include "mender-shell.h"
#include "mender-troubleshoot.h"

/**
 * @brief Mender client events
 */
static K_EVENT_DEFINE(mender_client_events);
#define MENDER_CLIENT_EVENT_CONNECT      (1 << 0)
#define MENDER_CLIENT_EVENT_CONNECTED    (1 << 1)
#define MENDER_CLIENT_EVENT_DISCONNECT   (1 << 2)
#define MENDER_CLIENT_EVENT_DISCONNECTED (1 << 3)
#define MENDER_CLIENT_EVENT_RESTART      (1 << 4)

/**
 * @brief Network management callback
 */
static struct net_mgmt_event_callback mgmt_cb;

/**
 * @brief Network events
 */
static K_EVENT_DEFINE(network_events);
#define NETWORK_EVENT_CONNECTED    (1 << 0)
#define NETWORK_EVENT_DISCONNECTED (1 << 1)

/**
 * @brief print DHCPv4 address information
 * @param iface Interface
 * @param if_addr Interface address
 * @param user_data user data (not used)
 */
static void
print_dhcpv4_addr(struct net_if *iface, struct net_if_addr *if_addr, void *user_data) {

    char           hr_addr[NET_IPV4_ADDR_LEN];
    struct in_addr netmask;

    /* Check address type */
    if (NET_ADDR_DHCP != if_addr->addr_type) {
        return;
    }

    LOG_INF("IPv4 address: %s", net_addr_ntop(AF_INET, &if_addr->address.in_addr, hr_addr, NET_IPV4_ADDR_LEN));
    LOG_INF("Lease time: %u seconds", iface->config.dhcpv4.lease_time);
    netmask = net_if_ipv4_get_netmask_by_addr(iface, &if_addr->address.in_addr);
    LOG_INF("Subnet: %s", net_addr_ntop(AF_INET, &netmask, hr_addr, NET_IPV4_ADDR_LEN));
    LOG_INF("Router: %s", net_addr_ntop(AF_INET, &iface->config.ip.ipv4->gw, hr_addr, NET_IPV4_ADDR_LEN));
}

/**
 * @brief Network event handler
 * @param cb Network management callback
 * @param mgmt_event Event
 * @param iface Interface
 */
#if (ZEPHYR_VERSION_CODE >= ZEPHYR_VERSION(4, 2, 0))
static void
net_event_handler(struct net_mgmt_event_callback *cb, uint64_t mgmt_event, struct net_if *iface) {
#else
static void
net_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event, struct net_if *iface) {
#endif

    switch (mgmt_event) {
        case NET_EVENT_IPV4_ADDR_ADD:
            /* Connected to network */
            LOG_INF("Got IPv4 event: 'NET_EVENT_IPV4_ADDR_ADD'");
            /* Print interface information */
            net_if_ipv4_addr_foreach(iface, print_dhcpv4_addr, NULL);
            /* Indicate the network is available */
            k_event_post(&network_events, NETWORK_EVENT_CONNECTED);
            break;
        case NET_EVENT_IPV4_ADDR_DEL:
            /* Disconnected from network */
            LOG_INF("Got IPv4 event: 'NET_EVENT_IPV4_ADDR_DEL'");
            /* Indicate the network is not available */
            k_event_post(&network_events, NETWORK_EVENT_DISCONNECTED);
            break;
    }
}

/**
 * @brief Network connnect callback
 * @return MENDER_OK if network is connected following the request, error code otherwise
 */
static mender_err_t
network_connect_cb(void) {

    LOG_INF("Mender client connect network");

    /* This callback can be used to configure network connection */
    /* Note that the application can connect the network before if required */
    /* This callback only indicates the mender-client requests network access now */
    k_event_post(&mender_client_events, MENDER_CLIENT_EVENT_CONNECT);
    uint32_t events = k_event_wait(&mender_client_events,
                                   MENDER_CLIENT_EVENT_CONNECTED | MENDER_CLIENT_EVENT_DISCONNECTED,
                                   false,
                                   K_SECONDS(2 * CONFIG_EXAMPLE_NETWORK_CONNECT_FAILS_MAX_TRIES * CONFIG_EXAMPLE_NETWORK_CONNECT_FAILS_TIMEOUT));
    k_event_clear(&mender_client_events, events);
    if (MENDER_CLIENT_EVENT_CONNECTED != (events & MENDER_CLIENT_EVENT_CONNECTED)) {
        return MENDER_FAIL;
    }

    return MENDER_OK;
}

/**
 * @brief Network release callback
 * @return MENDER_OK if network is released following the request, error code otherwise
 */
static mender_err_t
network_release_cb(void) {

    LOG_INF("Mender client released network");

    /* This callback can be used to release network connection */
    /* Note that the application can keep network activated if required */
    /* This callback only indicates the mender-client doesn't request network access now */
    k_event_post(&mender_client_events, MENDER_CLIENT_EVENT_DISCONNECT);

    return MENDER_OK;
}

int
network_connect(struct net_if *iface) {

    int err;

    /* Clear network events */
    k_event_clear(&network_events, NETWORK_EVENT_CONNECTED | NETWORK_EVENT_DISCONNECTED);

#ifdef CONFIG_WIFI

    /* Set connection request parameters */
    struct wifi_connect_req_params params = { 0 };
    params.band                           = WIFI_FREQ_BAND_UNKNOWN;
    params.channel                        = WIFI_CHANNEL_ANY;
    params.mfp                            = WIFI_MFP_OPTIONAL;
    params.ssid                           = CONFIG_EXAMPLE_WIFI_SSID;
    params.ssid_length                    = sizeof(CONFIG_EXAMPLE_WIFI_SSID) - 1;
#if defined(CONFIG_EXAMPLE_WIFI_AUTHENTICATION_MODE_WPA2_PSK)
    params.security   = WIFI_SECURITY_TYPE_PSK;
    params.psk        = CONFIG_EXAMPLE_WIFI_PSK;
    params.psk_length = sizeof(CONFIG_EXAMPLE_WIFI_PSK) - 1;
#elif defined(CONFIG_EXAMPLE_WIFI_AUTHENTICATION_MODE_WPA3_SAE)
    params.security            = WIFI_SECURITY_TYPE_SAE;
    params.sae_password        = CONFIG_EXAMPLE_WIFI_PSK;
    params.sae_password_length = sizeof(CONFIG_EXAMPLE_WIFI_PSK) - 1;
#else
    params.security = WIFI_SECURITY_TYPE_NONE;
#endif

    /* Request connection to the network */
    if (0 != (err = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(struct wifi_connect_req_params)))) {
        if (-EALREADY == err) {
            LOG_INF("Network is already connected");
            return 0;
        }
        LOG_WRN("An error occured requesting connection to the network (err=%d)", err);
    }

    /* Waiting interface to connect to the network */
    int      tries  = 0;
    uint32_t events = 0;
    while (NETWORK_EVENT_CONNECTED != (events & NETWORK_EVENT_CONNECTED)) {

        /* Wait until the network event */
        events = k_event_wait(
            &network_events, NETWORK_EVENT_CONNECTED | NETWORK_EVENT_DISCONNECTED, false, K_SECONDS(CONFIG_EXAMPLE_NETWORK_CONNECT_FAILS_TIMEOUT));
        k_event_clear(&network_events, events);
        if (NETWORK_EVENT_CONNECTED != (events & NETWORK_EVENT_CONNECTED)) {
            tries++;
            if (tries < CONFIG_EXAMPLE_NETWORK_CONNECT_FAILS_MAX_TRIES) {
                LOG_WRN("Not connected to the network (%d/%d), waiting for interface to connect", tries, CONFIG_EXAMPLE_NETWORK_CONNECT_FAILS_MAX_TRIES);
                /* Request connection to the network */
                if (0 != (err = net_mgmt(NET_REQUEST_WIFI_CONNECT, iface, &params, sizeof(struct wifi_connect_req_params)))) {
                    if (-EALREADY == err) {
                        LOG_INF("Network is already connected");
                        return 0;
                    }
                    LOG_WRN("An error occured requesting connection to the network (err=%d)", err);
                }
            } else {
                LOG_ERR("Not connected to the network (%d/%d)", tries, CONFIG_EXAMPLE_NETWORK_CONNECT_FAILS_MAX_TRIES);
                return -ENODEV;
            }
        }
    }

#else

    /* Up interface */
    if (0 != (err = net_if_up(iface))) {
        if (-EALREADY == err) {
            LOG_INF("Network interface is already up");
            return 0;
        }
        LOG_WRN("An error occured requesting interface to up (err=%d)", err);
    }

    /* Waiting interface to connect to the network */
    int      tries  = 0;
    uint32_t events = 0;
    while (NETWORK_EVENT_CONNECTED != (events & NETWORK_EVENT_CONNECTED)) {

        /* Wait until the network event */
        events = k_event_wait(
            &network_events, NETWORK_EVENT_CONNECTED | NETWORK_EVENT_DISCONNECTED, false, K_SECONDS(CONFIG_EXAMPLE_NETWORK_CONNECT_FAILS_TIMEOUT));
        k_event_clear(&network_events, events);
        if (NETWORK_EVENT_CONNECTED != (events & NETWORK_EVENT_CONNECTED)) {
            tries++;
            if (tries < CONFIG_EXAMPLE_NETWORK_CONNECT_FAILS_MAX_TRIES) {
                LOG_WRN("Not connected to the network (%d/%d), waiting for interface to connect", tries, CONFIG_EXAMPLE_NETWORK_CONNECT_FAILS_MAX_TRIES);
            } else {
                LOG_ERR("Not connected to the network (%d/%d)", tries, CONFIG_EXAMPLE_NETWORK_CONNECT_FAILS_MAX_TRIES);
                return -ENODEV;
            }
        }
    }

#endif /* CONFIG_WIFI */

    return 0;
}

int
network_disconnect(struct net_if *iface) {

#ifdef CONFIG_WIFI

    /* Request disconnection of the network */
    net_mgmt(NET_REQUEST_WIFI_DISCONNECT, iface, NULL, 0);

#else

    /* Down interface */
    net_if_down(iface);

#endif /* CONFIG_WIFI */

    return 0;
}

/**
 * @brief Authentication success callback
 * @return MENDER_OK if application is marked valid and success deployment status should be reported to the server, error code otherwise
 */
static mender_err_t
authentication_success_cb(void) {

    mender_err_t ret;

    LOG_INF("Mender client authenticated");

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT
    /* Activate troubleshoot add-on (deactivated by default) */
    if (MENDER_OK != (ret = mender_troubleshoot_activate())) {
        LOG_ERR("Unable to activate troubleshoot add-on");
        return ret;
    }
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT */

    /* Validate the image if it is still pending */
    /* Note it is possible to do multiple diagnosic tests before validating the image */
    /* In this example, authentication success with the mender-server is enough */
    if (MENDER_OK != (ret = mender_flash_confirm_image())) {
        LOG_ERR("Unable to validate the image");
        return ret;
    }

    return ret;
}

/**
 * @brief Authentication failure callback
 * @return MENDER_OK if nothing to do, error code if the mender client should restart the application
 */
static mender_err_t
authentication_failure_cb(void) {

    static int tries = 0;

    /* Check if confirmation of the image is still pending */
    if (true == mender_flash_is_image_confirmed()) {
        LOG_INF("Mender client authentication failed");
        return MENDER_OK;
    }

    /* Increment number of failures */
    tries++;
    LOG_ERR("Mender client authentication failed (%d/%d)", tries, CONFIG_EXAMPLE_AUTHENTICATION_FAILS_MAX_TRIES);

    /* Restart the application after several authentication failures with the mender-server */
    /* The image has not been confirmed and the bootloader will now rollback to the previous working image */
    /* Note it is possible to customize this depending of the wanted behavior */
    return (tries >= CONFIG_EXAMPLE_AUTHENTICATION_FAILS_MAX_TRIES) ? MENDER_FAIL : MENDER_OK;
}

/**
 * @brief Deployment status callback
 * @param status Deployment status value
 * @param desc Deployment status description as string
 * @return MENDER_OK if the function succeeds, error code otherwise
 */
static mender_err_t
deployment_status_cb(mender_deployment_status_t status, char *desc) {

    /* We can do something else if required */
    LOG_INF("Deployment status is '%s'", desc);

    return MENDER_OK;
}

/**
 * @brief Restart callback
 * @return MENDER_OK if the function succeeds, error code otherwise
 */
static mender_err_t
restart_cb(void) {

    /* Application is responsible to shutdown and restart the system now */
    k_event_post(&mender_client_events, MENDER_CLIENT_EVENT_RESTART);

    return MENDER_OK;
}

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE
#ifndef CONFIG_MENDER_CLIENT_CONFIGURE_STORAGE

/**
 * @brief Device configuration updated
 * @param configuration Device configuration
 * @return MENDER_OK if the function succeeds, error code otherwise
 */
static mender_err_t
config_updated_cb(mender_keystore_t *configuration) {

    /* Application can use the new device configuration now */
    /* In this example, we just print the content of the configuration received from the Mender server */
    if (NULL != configuration) {
        size_t index = 0;
        LOG_INF("Device configuration received from the server");
        while ((NULL != configuration[index].name) && (NULL != configuration[index].value)) {
            LOG_INF("Key=%s, value=%s", configuration[index].name, configuration[index].value);
            index++;
        }
    }

    return MENDER_OK;
}

#endif /* CONFIG_MENDER_CLIENT_CONFIGURE_STORAGE */
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE */

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER

static mender_err_t
file_transfer_stat_cb(char *path, size_t **size, uint32_t **uid, uint32_t **gid, uint32_t **mode, time_t **time) {

    assert(NULL != path);
    struct fs_dirent stats;
    mender_err_t     ret = MENDER_OK;

    /* Get statistics of file */
    if (0 != fs_stat(path, &stats)) {
        LOG_ERR("Unable to get statistics of file '%s'", path);
        ret = MENDER_FAIL;
        goto FAIL;
    }
    /* Size is optional */
    if (NULL != size) {
        if (NULL == (*size = (size_t *)malloc(sizeof(size_t)))) {
            LOG_ERR("Unable to allocate memory");
            ret = MENDER_FAIL;
            goto FAIL;
        }
        **size = stats.size;
    }
    /* Mode is not optional and file must be a regular file to be downloaded by the server */
    if (NULL != mode) {
        if (NULL == (*mode = (uint32_t *)malloc(sizeof(uint32_t)))) {
            LOG_ERR("Unable to allocate memory");
            ret = MENDER_FAIL;
            goto FAIL;
        }
        **mode = (FS_DIR_ENTRY_FILE == stats.type) ? 0100000 : 0040000;
    }

FAIL:

    return ret;
}

static mender_err_t
file_transfer_open_cb(char *path, char *mode, void **handle) {

    assert(NULL != path);
    assert(NULL != mode);
    int err;

    /* Allocate file handle */
    struct fs_file_t *file;
    if (NULL == (file = malloc(sizeof(struct fs_file_t)))) {
        LOG_ERR("Unable to allocate memory");
        return MENDER_FAIL;
    }
    fs_file_t_init(file);

    /* Open file */
    LOG_INF("Opening file '%s' with mode '%s'", path, mode);
    if (!strcmp(mode, "rb")) {
        if ((err = fs_open(file, path, FS_O_READ)) < 0) {
            LOG_ERR("Unable to open file '%s' (err=%d)", path, err);
            free(file);
            return MENDER_FAIL;
        }
    } else {
        if ((err = fs_open(file, path, FS_O_CREATE | FS_O_WRITE)) < 0) {
            LOG_ERR("Unable to open file '%s' (err=%d)", path, err);
            free(file);
            return MENDER_FAIL;
        }
    }
    *handle = file;

    return MENDER_OK;
}

static mender_err_t
file_transfer_read_cb(void *handle, void *data, size_t *length) {

    assert(NULL != handle);
    assert(NULL != data);
    assert(NULL != length);
    int err;

    /* Read file */
    if ((err = fs_read((struct fs_file_t *)handle, data, *length)) < 0) {
        LOG_ERR("Unable to read data from the file (err=%d)", err);
        return MENDER_FAIL;
    }
    *length = err;

    return MENDER_OK;
}

static mender_err_t
file_transfer_write_cb(void *handle, void *data, size_t length) {

    assert(NULL != handle);
    assert(NULL != data);
    int err;

    /* Write file */
    if ((err = fs_write((struct fs_file_t *)handle, data, length)) < 0) {
        LOG_ERR("Unable to write data to the file (err=%d)", err);
        return MENDER_FAIL;
    }

    return MENDER_OK;
}

static mender_err_t
file_transfer_close_cb(void *handle) {

    assert(NULL != handle);
    int err;

    /* Close file */
    LOG_INF("Closing file");
    if ((err = fs_close((struct fs_file_t *)handle)) < 0) {
        LOG_ERR("Unable to close file (err=%d)", err);
    }

    /* Release memory */
    free(handle);

    return MENDER_OK;
}

#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER */
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT */

/**
 * @brief Main function
 * @return Always returns 0
 */
int
main(void) {

    /* Initialize network */
    LOG_INF("Initialization of network interface");
    struct net_if *iface = net_if_get_default();
    assert(NULL != iface);
#ifdef CONFIG_WIFI
#ifdef CONFIG_WIFI_NM_WPA_SUPPLICANT
    /* Wait for wpa-supplicant initialization */
    net_mgmt_event_wait_on_iface(iface, NET_EVENT_SUPPLICANT_IFACE_ADDED, NULL, NULL, NULL, K_SECONDS(30));
#endif /* CONFIG_WIFI_NM_WPA_SUPPLICANT */
#endif /* CONFIG_WIFI */
    net_mgmt_init_event_callback(&mgmt_cb, net_event_handler, NET_EVENT_IPV4_ADDR_ADD | NET_EVENT_IPV4_ADDR_DEL);
    net_mgmt_add_event_callback(&mgmt_cb);
    net_dhcpv4_start(iface);

#ifdef CONFIG_NET_SOCKETS_SOCKOPT_TLS
    /* Initialize certificate */
    LOG_INF("Initialization of certificate(s)");
    tls_credential_add(CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_PRIMARY, TLS_CREDENTIAL_CA_CERTIFICATE, ca_certificate_primary, sizeof(ca_certificate_primary));
#if (0 != CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_SECONDARY)
    tls_credential_add(
        CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_SECONDARY, TLS_CREDENTIAL_CA_CERTIFICATE, ca_certificate_secondary, sizeof(ca_certificate_secondary));
#endif /* (0 != CONFIG_MENDER_NET_CA_CERTIFICATE_TAG_SECONDARY) */
#endif /* CONFIG_NET_SOCKETS_SOCKOPT_TLS */

    /* Read base MAC address of the device */
    char                 mac_address[18];
    struct net_linkaddr *linkaddr = net_if_get_link_addr(iface);
    assert(NULL != linkaddr);
    snprintf(mac_address,
             sizeof(mac_address),
             "%02x:%02x:%02x:%02x:%02x:%02x",
             linkaddr->addr[0],
             linkaddr->addr[1],
             linkaddr->addr[2],
             linkaddr->addr[3],
             linkaddr->addr[4],
             linkaddr->addr[5]);
    LOG_INF("MAC address of the device '%s'", mac_address);

    /* Retrieve running version of the device */
    LOG_INF("Running project '%s' version '%s'", PROJECT_NAME, APP_VERSION_STRING);

    /* Compute artifact name */
    char artifact_name[128];
    snprintf(artifact_name, sizeof(artifact_name), "%s-v%s", CONFIG_EXAMPLE_ARTIFACT_NAME_PREFIX, APP_VERSION_STRING);

    /* Retrieve device type */
    char *device_type = CONFIG_EXAMPLE_DEVICE_TYPE;

    /* Initialize mender-client */
    mender_keystore_t         identity[]              = { { .name = "mac", .value = mac_address }, { .name = NULL, .value = NULL } };
    mender_client_config_t    mender_client_config    = { .identity                     = identity,
                                                          .artifact_name                = artifact_name,
                                                          .device_type                  = device_type,
                                                          .host                         = NULL,
                                                          .tenant_token                 = NULL,
                                                          .authentication_poll_interval = 0,
                                                          .update_poll_interval         = 0,
                                                          .recommissioning              = false };
    mender_client_callbacks_t mender_client_callbacks = { .network_connect        = network_connect_cb,
                                                          .network_release        = network_release_cb,
                                                          .authentication_success = authentication_success_cb,
                                                          .authentication_failure = authentication_failure_cb,
                                                          .deployment_status      = deployment_status_cb,
                                                          .restart                = restart_cb };
    if (MENDER_OK != mender_client_init(&mender_client_config, &mender_client_callbacks)) {
        LOG_ERR("Unable to initialize mender client");
        goto RESTART;
    }
    LOG_INF("Mender client version '%s' initialized", mender_client_version());

    /* Initialize mender add-ons */
#ifdef CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE
    mender_configure_config_t    mender_configure_config    = { .refresh_interval = 0 };
    mender_configure_callbacks_t mender_configure_callbacks = {
#ifndef CONFIG_MENDER_CLIENT_CONFIGURE_STORAGE
        .config_updated = config_updated_cb,
#endif /* CONFIG_MENDER_CLIENT_CONFIGURE_STORAGE */
    };
    if (MENDER_OK
        != mender_client_register_addon(
            (mender_addon_instance_t *)&mender_configure_addon_instance, (void *)&mender_configure_config, (void *)&mender_configure_callbacks)) {
        LOG_ERR("Unable to register configure add-on");
        goto RELEASE;
    }
    LOG_INF("Mender configure add-on registered");
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE */
#ifdef CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY
    mender_inventory_config_t mender_inventory_config = { .refresh_interval = 0 };
    if (MENDER_OK != mender_client_register_addon((mender_addon_instance_t *)&mender_inventory_addon_instance, (void *)&mender_inventory_config, NULL)) {
        LOG_ERR("Unable to register inventory add-on");
        goto RELEASE;
    }
    LOG_INF("Mender inventory add-on registered");
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY */
#ifdef CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT
    mender_troubleshoot_config_t    mender_troubleshoot_config    = { .host = NULL, .healthcheck_interval = 0 };
    mender_troubleshoot_callbacks_t mender_troubleshoot_callbacks = {
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER
        .file_transfer = { .stat  = file_transfer_stat_cb,
                           .open  = file_transfer_open_cb,
                           .read  = file_transfer_read_cb,
                           .write = file_transfer_write_cb,
                           .close = file_transfer_close_cb },
#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_FILE_TRANSFER */
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_PORT_FORWARDING
        .port_forwarding = { .connect = NULL, .send = NULL, .close = NULL },
#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_PORT_FORWARDING */
#ifdef CONFIG_MENDER_CLIENT_TROUBLESHOOT_SHELL
        .shell = { .open = mender_shell_open, .resize = mender_shell_resize, .write = mender_shell_write, .close = mender_shell_close }
#endif /* CONFIG_MENDER_CLIENT_TROUBLESHOOT_SHELL */
    };
    if (MENDER_OK
        != mender_client_register_addon(
            (mender_addon_instance_t *)&mender_troubleshoot_addon_instance, (void *)&mender_troubleshoot_config, (void *)&mender_troubleshoot_callbacks)) {
        LOG_ERR("Unable to register troubleshoot add-on");
        goto RELEASE;
    }
    LOG_INF("Mender troubleshoot add-on registered");
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_TROUBLESHOOT */

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE
    /* Get mender configuration (this is just an example to illustrate the API) */
    mender_keystore_t *configuration;
    if (MENDER_OK != mender_configure_get(&configuration)) {
        LOG_ERR("Unable to get mender device configuration");
        goto RELEASE;
    } else if (NULL != configuration) {
        size_t index = 0;
        LOG_INF("Device configuration retrieved");
        while ((NULL != configuration[index].name) && (NULL != configuration[index].value)) {
            LOG_INF("Key=%s, value=%s", configuration[index].name, configuration[index].value);
            index++;
        }
        mender_utils_keystore_delete(configuration);
    } else {
        LOG_INF("Mender device configuration is empty");
    }
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_CONFIGURE */

#ifdef CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY
    /* Set mender inventory (this is just an example to illustrate the API) */
    mender_keystore_t inventory[] = { { .name = "zephyr-rtos", .value = KERNEL_VERSION_STRING },
                                      { .name = "nrf-connect-sdk", .value = NCS_VERSION_STRING },
                                      { .name = "mender-mcu-client", .value = mender_client_version() },
                                      { .name = "latitude", .value = "45.8325" },
                                      { .name = "longitude", .value = "6.864722" },
                                      { .name = NULL, .value = NULL } };
    if (MENDER_OK != mender_inventory_set(inventory)) {
        LOG_ERR("Unable to set mender inventory");
        goto RELEASE;
    }
#endif /* CONFIG_MENDER_CLIENT_ADD_ON_INVENTORY */

    /* Finally activate mender client */
    if (MENDER_OK != mender_client_activate()) {
        LOG_ERR("Unable to activate mender-client");
        goto RELEASE;
    }

    /* Wait for mender-mcu-client events, connect and disconnect network on request, restart the application if required */
    bool connected = false;
    while (1) {
        uint32_t events
            = k_event_wait(&mender_client_events, MENDER_CLIENT_EVENT_CONNECT | MENDER_CLIENT_EVENT_DISCONNECT | MENDER_CLIENT_EVENT_RESTART, false, K_FOREVER);
        k_event_clear(&mender_client_events, events);
        if (MENDER_CLIENT_EVENT_CONNECT == (events & MENDER_CLIENT_EVENT_CONNECT)) {
            /* Connect to the network */
            LOG_INF("Connecting to the network");
            if (0 != network_connect(iface)) {
                k_event_post(&mender_client_events, MENDER_CLIENT_EVENT_DISCONNECTED);
                LOG_ERR("Unable to connect network");
            } else {
                connected = true;
                k_event_post(&mender_client_events, MENDER_CLIENT_EVENT_CONNECTED);
                LOG_INF("Connected to the network");
            }
        } else if (MENDER_CLIENT_EVENT_DISCONNECT == (events & MENDER_CLIENT_EVENT_DISCONNECT)) {
            events = k_event_wait(&mender_client_events, MENDER_CLIENT_EVENT_CONNECT, false, K_SECONDS(10));
            k_event_clear(&mender_client_events, events);
            if (MENDER_CLIENT_EVENT_CONNECT == (events & MENDER_CLIENT_EVENT_CONNECT)) {
                /* Reconnection requested while not disconnected yet */
                k_event_post(&mender_client_events, MENDER_CLIENT_EVENT_CONNECTED);
                LOG_INF("Connected to the network");
            } else {
                /* Disconnect the network */
                LOG_INF("Disconnecting network");
                if (0 != network_disconnect(iface)) {
                    LOG_ERR("Unable to disconnect network");
                } else {
                    connected = false;
                    LOG_INF("Disconnected of the network");
                }
            }
        }
        if (MENDER_CLIENT_EVENT_RESTART == (events & MENDER_CLIENT_EVENT_RESTART)) {
            while (1) {
                events = k_event_wait(&mender_client_events, MENDER_CLIENT_EVENT_CONNECT | MENDER_CLIENT_EVENT_DISCONNECT, false, K_SECONDS(10));
                k_event_clear(&mender_client_events, events);
                if (MENDER_CLIENT_EVENT_CONNECT == (events & MENDER_CLIENT_EVENT_CONNECT)) {
                    /* Reconnection requested before restarting */
                    if (!connected) {
                        /* Connect to the network */
                        LOG_INF("Connecting to the network");
                        if (0 != network_connect(iface)) {
                            k_event_post(&mender_client_events, MENDER_CLIENT_EVENT_DISCONNECTED);
                            LOG_ERR("Unable to connect network");
                        } else {
                            connected = true;
                            k_event_post(&mender_client_events, MENDER_CLIENT_EVENT_CONNECTED);
                            LOG_INF("Connected to the network");
                        }
                    } else {
                        k_event_post(&mender_client_events, MENDER_CLIENT_EVENT_CONNECTED);
                        LOG_INF("Connected to the network");
                    }
                } else if (MENDER_CLIENT_EVENT_DISCONNECT == (events & MENDER_CLIENT_EVENT_DISCONNECT)) {
                    /* Disonnection requested before restarting */
                    if (connected) {
                        /* Disconnect the network */
                        LOG_INF("Disconnecting network");
                        if (0 != network_disconnect(iface)) {
                            LOG_ERR("Unable to disconnect network");
                        } else {
                            connected = false;
                            LOG_INF("Disconnected of the network");
                        }
                    }
                } else {
                    /* Application will restart now */
                    goto RELEASE;
                }
            }
        }
    }

RELEASE:

    /* Deactivate and release mender-client */
    mender_client_deactivate();
    mender_client_exit();

RESTART:

    /* Restart */
    LOG_INF("Restarting system");
    sys_reboot(SYS_REBOOT_WARM);

    return 0;
}
