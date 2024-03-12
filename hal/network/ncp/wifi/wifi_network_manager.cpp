/*
 * Copyright (c) 2018 Particle Industries, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "wifi_network_manager.h"

#include "wifi_ncp_client.h"

#include "file_util.h"
#include "logging.h"
#include "scope_guard.h"
#include "check.h"

#include "spark_wiring_vector.h"

#include <algorithm>

// FIXME: Move nanopb utilities to a common header file
#include "../../../system/src/control/common.h"

#include "network_config.pb.h"

#include "network/ncp/wifi/ncp.h"
#include "ifapi.h"
#include "system_network.h"
#include "system_threading.h"

#define PB(_name) particle_firmware_##_name
#define PB_WIFI(_name) particle_ctrl_wifi_##_name

#define CONFIG_FILE "/sys/wifi_config.bin"
#define CONFIG_FILE_TMP "/sys/wifi_config.bin.tmp"

LOG_SOURCE_CATEGORY("ncp.mgr")

namespace particle {

namespace {

using namespace control::common;
using spark::Vector;

template<typename T>
void bssidToPb(const MacAddress& bssid, T* pbBssid) {
    if (bssid != INVALID_MAC_ADDRESS) {
        memcpy(pbBssid->bytes, &bssid, MAC_ADDRESS_SIZE);
        pbBssid->size = MAC_ADDRESS_SIZE;
    }
}

template<typename T>
void bssidFromPb(MacAddress* bssid, const T& pbBssid) {
    if (pbBssid.size == MAC_ADDRESS_SIZE) {
        memcpy(bssid, pbBssid.bytes, MAC_ADDRESS_SIZE);
    }
}

// TODO: Implement a couple functions to conveniently save/load a protobuf message to/from a file
int loadConfig(Vector<WifiNetworkConfig>* networks) {
    // Get filesystem instance
    const auto fs = filesystem_get_instance(FILESYSTEM_INSTANCE_DEFAULT, nullptr);
    CHECK_TRUE(fs, SYSTEM_ERROR_FILE);
    fs::FsLock lock(fs);
    CHECK(filesystem_mount(fs));
    // Open configuration file
    lfs_file_t file = {};
    CHECK(openFile(&file, CONFIG_FILE, LFS_O_RDONLY));
    NAMED_SCOPE_GUARD(fileGuard, {
        lfs_file_close(&fs->instance, &file);
    });
    // Parse configuration
    PB(WifiConfig) pbConf = {};
    pbConf.networks.arg = networks;
    pbConf.networks.funcs.decode = [](pb_istream_t* strm, const pb_field_iter_t* field, void** arg) {
        const auto networks = (Vector<WifiNetworkConfig>*)*arg;
        PB(WifiConfig_Network) pbConf = {};
        DecodedCString dSsid(&pbConf.ssid);
        DecodedCString dPwd(&pbConf.credentials.password);
        if (!pb_decode_noinit(strm, PB(WifiConfig_Network_fields), &pbConf)) {
            return false;
        }
        WifiCredentials cred;
        cred.type((WifiCredentials::Type)pbConf.credentials.type);
        if (pbConf.credentials.type == PB_WIFI(CredentialsType_PASSWORD)) {
            cred.password(dPwd.data);
        }
        MacAddress bssid = INVALID_MAC_ADDRESS;
        bssidFromPb(&bssid, pbConf.bssid);
        auto conf = WifiNetworkConfig()
            .ssid(dSsid.data)
            .bssid(bssid)
            .security((WifiSecurity)pbConf.security)
            .credentials(std::move(cred))
            .hidden(pbConf.hidden);
        if (!networks->append(std::move(conf))) {
            return false;
        }
        return true;
    };
    const int r = decodeProtobufFromFile(&file, PB(WifiConfig_fields), &pbConf);
    if (r < 0) {
        LOG(ERROR, "Unable to parse network settings");
        networks->clear();
        LOG(WARN, "Removing file: %s", CONFIG_FILE);
        lfs_file_close(&fs->instance, &file);
        fileGuard.dismiss();
        lfs_remove(&fs->instance, CONFIG_FILE);
    }
    return 0;
}

int saveConfig(const Vector<WifiNetworkConfig>& networks) {
    // Get filesystem instance
    const auto fs = filesystem_get_instance(FILESYSTEM_INSTANCE_DEFAULT, nullptr);
    CHECK_TRUE(fs, SYSTEM_ERROR_FILE);
    fs::FsLock lock(fs);
    CHECK(filesystem_mount(fs));
    // Open configuration file
    lfs_file_t file = {};
    CHECK(openFile(&file, CONFIG_FILE_TMP, LFS_O_WRONLY));
    bool ok = false;
    SCOPE_GUARD({
        lfs_file_close(&fs->instance, &file);
        if (ok) {
            lfs_rename(&fs->instance, CONFIG_FILE_TMP, CONFIG_FILE);
        } else {
            lfs_remove(&fs->instance, CONFIG_FILE_TMP);
        }
    });
    int r = lfs_file_truncate(&fs->instance, &file, 0);
    CHECK_TRUE(r == LFS_ERR_OK, SYSTEM_ERROR_FILE);
    // Serialize configuration
    PB(WifiConfig) pbConf = {};
    pbConf.networks.arg = const_cast<Vector<WifiNetworkConfig>*>(&networks);
    pbConf.networks.funcs.encode = [](pb_ostream_t* strm, const pb_field_iter_t* field, void* const* arg) {
        const auto networks = (const Vector<WifiNetworkConfig>*)*arg;
        for (const WifiNetworkConfig& conf: *networks) {
            PB(WifiConfig_Network) pbConf = {};
            EncodedString eSsid(&pbConf.ssid, conf.ssid(), strlen(conf.ssid()));
            EncodedString ePwd(&pbConf.credentials.password);
            bssidToPb(conf.bssid(), &pbConf.bssid);
            pbConf.security = (PB_WIFI(Security))conf.security();
            pbConf.credentials.type = (PB_WIFI(CredentialsType))conf.credentials().type();
            if (conf.credentials().type() == WifiCredentials::PASSWORD) {
                const auto s = conf.credentials().password();
                ePwd.data = s;
                ePwd.size = strlen(s);
            }
            pbConf.hidden = conf.hidden();
            if (!pb_encode_tag_for_field(strm, field)) {
                return false;
            }
            if (!pb_encode_submessage(strm, PB(WifiConfig_Network_fields), &pbConf)) {
                return false;
            }
        }
        return true;
    };
    CHECK(encodeProtobufToFile(&file, PB(WifiConfig_fields), &pbConf));
    LOG(TRACE, "Updated file: %s", CONFIG_FILE);
    ok = true;
    return 0;
}

int networkIndexForSsid(const char* ssid, const Vector<WifiNetworkConfig>& networks) {
    for (int i = 0; i < networks.size(); ++i) {
        if (strcmp(ssid, networks.at(i).ssid()) == 0) {
            return i;
        }
    }
    return -1;
}

void sortByRssi(Vector<WifiScanResult>* scanResults) {
    std::sort(scanResults->begin(), scanResults->end(), [](const WifiScanResult& ap1, const WifiScanResult& ap2) {
        return (ap1.rssi() > ap2.rssi()); // In descending order
    });
}

} // unnamed

WifiNetworkManager::WifiNetworkManager(WifiNcpClient* client) :
        client_(client) {
}

WifiNetworkManager::~WifiNetworkManager() {
}

int WifiNetworkManager::connect(WifiNetworkConfig conf) {
    auto network = &conf;
    int r = SYSTEM_ERROR_INTERNAL;
    if (client_->ncpId() != PlatformNCPIdentifier::PLATFORM_NCP_ESP32) {
        r = client_->connect(network->ssid(), network->bssid(), network->security(), network->credentials());
        if (r == 0 && network->bssid() == INVALID_MAC_ADDRESS) {
            // Update BSSID
            WifiNetworkInfo info;
            if (client_->getNetworkInfo(&info) == 0) {
                network->bssid(info.bssid());
            }
        }
    } else {
        // Perform a network scan on ESP32 devices because ESP32 doesn't support 802.11v/k/r
        Vector<WifiScanResult> scanResults;
        CHECK_TRUE(scanResults.reserve(10), SYSTEM_ERROR_NO_MEMORY);
        CHECK(client_->scan([](WifiScanResult result, void* data) -> int {
            auto scanResults = (Vector<WifiScanResult>*)data;
            CHECK_TRUE(scanResults->append(std::move(result)), SYSTEM_ERROR_NO_MEMORY);
            return 0;
        }, &scanResults));
        // Sort discovered networks by RSSI
        sortByRssi(&scanResults);
        // Traverse scanned networks and try to connect to the given network
        for (const auto& ap: scanResults) {
            if (strcmp(network->ssid(), ap.ssid()) != 0) {
                continue;
            }
            r = client_->connect(network->ssid(), ap.bssid(), network->security(), network->credentials());
            if (r == 0) {
                if (network->bssid() != ap.bssid()) {
                    // Update BSSID
                    network->bssid(ap.bssid());
                }
                break;
            }
        }
    }
    // Move the network to the beginning of the list
    if (r == 0) {
        Vector<WifiNetworkConfig> networks;
        CHECK(loadConfig(&networks));
        int index = networkIndexForSsid(network->ssid(), networks);
        if (index >= 0) {
            networks.removeAt(index);
            networks.prepend(*network);
            saveConfig(networks);
        }
    }
    return r;
}

int WifiNetworkManager::connect() {
    // Get known networks
    Vector<WifiNetworkConfig> networks;
    CHECK(loadConfig(&networks));
    if (networks.size() <= 0) {
        return SYSTEM_ERROR_NOT_FOUND;
    }
    // Perform network scan
    Vector<WifiScanResult> scanResults;
    CHECK_TRUE(scanResults.reserve(10), SYSTEM_ERROR_NO_MEMORY);
    CHECK(client_->scan([](WifiScanResult result, void* data) -> int {
        auto scanResults = (Vector<WifiScanResult>*)data;
        CHECK_TRUE(scanResults->append(std::move(result)), SYSTEM_ERROR_NO_MEMORY);
        return 0;
    }, &scanResults));
    // Sort discovered networks by RSSI
    sortByRssi(&scanResults);

    bool updateConfig = false;
    bool connected = false;
    uint8_t index = 0;
    WifiNetworkConfig* network = nullptr;
    int r = SYSTEM_ERROR_INTERNAL;
    // Traverse scanned networks and try to connect to any of them those have credentials stored
    for (const auto& ap: scanResults) {
        index = 0;
        for (; index < networks.size(); ++index) {
            network = &networks.at(index);
            if (strcmp(network->ssid(), ap.ssid()) == 0 && !network->hidden()) {
                break;
            }
        }
        if (index == networks.size()) {
            continue;
        }
        r = client_->connect(network->ssid(), ap.bssid(), network->security(), network->credentials());
        if (r == 0) {
            if (network->bssid() != ap.bssid()) {
                // Update BSSID
                network->bssid(ap.bssid());
                updateConfig = true;
            }
            connected = true;
            break;
        }
    }
    // Attempt to connect to hidden SSIDs
    if (!connected) {
        index = 0;
        for (; index < networks.size(); ++index) {
            network = &networks.at(index);
            if (network->hidden()) {
                r = client_->connect(network->ssid(), network->bssid(), network->security(), network->credentials());
                if (r == 0) {
                    break;
                }
            }
        }
        if (!connected) {
            return SYSTEM_ERROR_NOT_FOUND;
        }
    }
    if (index != 0) {
        // Move the network to the beginning of the list
        auto net = networks.takeAt(index);
        networks.prepend(std::move(net));
        updateConfig = true;
    }
    if (updateConfig) {
        saveConfig(networks);
    }
    return r;
}


int WifiNetworkManager::connect(const char* ssid) {
    if (!ssid) {
        return connect();
    }
    // Get known networks
    Vector<WifiNetworkConfig> networks;
    CHECK(loadConfig(&networks));
    int index = 0;
    // Find network with the given SSID
    for (; index < networks.size(); ++index) {
        if (strcmp(networks.at(index).ssid(), ssid) == 0) {
            break;
        }
    }
    if (index == networks.size()) {
        return SYSTEM_ERROR_NOT_FOUND;
    }
    // Connect to the network
    auto network = networks.at(index);
    return connect(network);
}

int WifiNetworkManager::setNetworkConfig(WifiNetworkConfig conf, uint8_t flags) {
    SYSTEM_THREAD_CONTEXT_SYNC(setNetworkConfig(conf, flags));

    CHECK_TRUE(conf.ssid(), SYSTEM_ERROR_INVALID_ARGUMENT);
    Vector<WifiNetworkConfig> networks;
    CHECK(loadConfig(&networks));

    if (flags & WiFiSetConfigFlags::VALIDATE) {
        const auto mgr = wifiNetworkManager();
        CHECK_TRUE(mgr, SYSTEM_ERROR_UNKNOWN);
        auto ncpClient = mgr->ncpClient();
        CHECK_TRUE(ncpClient, SYSTEM_ERROR_UNKNOWN);
        const NcpClientLock lock(ncpClient);

        NcpPowerState ncpPwrState = ncpClient->ncpPowerState();
        bool networkOn = network_is_on(NETWORK_INTERFACE_WIFI_STA, nullptr);
        bool needToConnect = network_connecting(NETWORK_INTERFACE_WIFI_STA, 0, nullptr) || network_ready(NETWORK_INTERFACE_WIFI_STA, NETWORK_READY_TYPE_ANY, nullptr);
        if_t iface = nullptr;
        CHECK(if_get_by_index(NETWORK_INTERFACE_WIFI_STA, &iface));
        CHECK_TRUE(iface, SYSTEM_ERROR_INVALID_STATE);

        bool pass = false;
        SCOPE_GUARD ({
            if (!needToConnect && !((flags & WiFiSetConfigFlags::KEEP_CONNECTED) && pass)) {
                ncpClient->disconnect();
                network_disconnect(NETWORK_INTERFACE_WIFI_STA, NETWORK_DISCONNECT_REASON_USER, nullptr);
                // network_disconnect() will disable the NCP client
                ncpClient->enable();
                if (!networkOn) {
                    network_off(NETWORK_INTERFACE_WIFI_STA, 0, 0, nullptr);
                    if_req_power pwr = {};
                    pwr.state = IF_POWER_STATE_DOWN;
                    if_request(iface, IF_REQ_POWER_STATE, &pwr, sizeof(pwr), nullptr);
                    if (ncpPwrState != NcpPowerState::ON && ncpPwrState != NcpPowerState::TRANSIENT_ON) {
                        ncpClient->off();
                    }
                }
#if HAL_PLATFORM_NRF52840
                else {
                    // The above enable() puts the NCP client into disabled and powered off state
                    // The following enable() will actually enable the NCP client and put it to OFF state
                    ncpClient->enable();
                    ncpClient->on();
                }
#endif
            }
        });

        // Connect to the network
        CHECK(ncpClient->on());
        // To unblock
        ncpClient->disable();
        CHECK(ncpClient->enable());
        // These two are in sync now
        ncpClient->disconnect(); // ignore the error
        network_disconnect(NETWORK_INTERFACE_WIFI_STA, NETWORK_DISCONNECT_REASON_USER, nullptr);
        // FIXME: We are wiating for ncpNetif to potentially fully disconnect
        // FIXME: synchronize NCP client / NcpNetif and system network manager state
        CHECK(ncpClient->enable());
        CHECK(ncpClient->on());
        network_connect(NETWORK_INTERFACE_WIFI_STA, 0, 0, nullptr);
        // If there is no credentials stored, network_connect() won't activate the connection
        if_set_flags(iface, IFF_UP);
        CHECK(mgr->connect(conf));
        pass = true;
        // Fall through to save the network credentials
    }

    int index = networkIndexForSsid(conf.ssid(), networks);
    if (index < 0) {
        // Add a new network or replace the last network in the list
        if (networks.size() < (int)MAX_CONFIGURED_WIFI_NETWORK_COUNT) {
            CHECK_TRUE(networks.resize(networks.size() + 1), SYSTEM_ERROR_NO_MEMORY);
        }
        index = networks.size() - 1;
        networks[index] = conf;
        if (flags & WiFiSetConfigFlags::VALIDATE) {
            // Move the network to the beginning of the list
            auto net = networks.takeAt(index);
            networks.prepend(std::move(net));
        }
        CHECK(saveConfig(networks));
    } else if (!(flags & WiFiSetConfigFlags::VALIDATE)) {
        networks[index] = conf;
        CHECK(saveConfig(networks));
    }
    return 0;
}

int WifiNetworkManager::getNetworkConfig(const char* ssid, WifiNetworkConfig* conf) {
    // TODO: Cache the list of networks
    CHECK_TRUE(ssid, SYSTEM_ERROR_INVALID_ARGUMENT);
    Vector<WifiNetworkConfig> networks;
    CHECK(loadConfig(&networks));
    const int index = networkIndexForSsid(ssid, networks);
    if (index < 0) {
        return SYSTEM_ERROR_NOT_FOUND;
    }
    *conf = std::move(networks[index]);
    return 0;
}

int WifiNetworkManager::getNetworkConfig(GetNetworkConfigCallback callback, void* data) {
    Vector<WifiNetworkConfig> networks;
    CHECK(loadConfig(&networks));
    for (int i = 0; i < networks.size(); ++i) {
        const int ret = callback(std::move(networks[i]), data);
        if (ret < 0) {
            return ret;
        }
    }
    return 0;
}

void WifiNetworkManager::removeNetworkConfig(const char* ssid) {
    Vector<WifiNetworkConfig> networks;
    if (loadConfig(&networks) < 0) {
        return;
    }
    const int index = networkIndexForSsid(ssid, networks);
    if (index < 0) {
        return;
    }
    networks.removeAt(index);
    saveConfig(networks);
}

void WifiNetworkManager::clearNetworkConfig() {
    Vector<WifiNetworkConfig> networks;
    saveConfig(networks);
}

bool WifiNetworkManager::hasNetworkConfig() {
    Vector<WifiNetworkConfig> networks;
    const int r = loadConfig(&networks);
    if (r < 0) {
        return false;
    }
    return !networks.isEmpty();
}

} // particle
