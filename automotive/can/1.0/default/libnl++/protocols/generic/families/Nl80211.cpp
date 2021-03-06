/*
 * Copyright (C) 2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "Nl80211.h"

#include "../../structs.h"
#include "common.h"

#include <linux/nl80211.h>

#include <iomanip>

namespace android::nl::protocols::generic::families {

/**
 * Reduce verbosity of printed Information Elements.
 */
static constexpr bool kCompactIE = true;

enum {
    // broken compatibility in Aug 2020
    NL80211_ATTR_CNTDWN_OFFS_BEACON = NL80211_ATTR_CSA_C_OFF_BEACON,
    NL80211_ATTR_CNTDWN_OFFS_PRESP = NL80211_ATTR_CSA_C_OFF_PRESP,

    // new fields not available in current Android
    NL80211_ATTR_FILS_DISCOVERY = NL80211_ATTR_HE_6GHZ_CAPABILITY + 1,
    NL80211_ATTR_UNSOL_BCAST_PROBE_RESP,
    NL80211_ATTR_S1G_CAPABILITY,
    NL80211_ATTR_S1G_CAPABILITY_MASK,

    NL80211_FREQUENCY_ATTR_1MHZ = NL80211_FREQUENCY_ATTR_OFFSET + 1,
    NL80211_FREQUENCY_ATTR_2MHZ,
    NL80211_FREQUENCY_ATTR_4MHZ,
    NL80211_FREQUENCY_ATTR_8MHZ,
    NL80211_FREQUENCY_ATTR_16MHZ,
};

enum ieee80211_eid {
    WLAN_EID_SSID = 0,
};

using DataType = AttributeDefinition::DataType;
using Flags = AttributeDefinition::Flags;

static void informationElementsToStream(std::stringstream& ss, const Buffer<nlattr> attr);
static void nl80211_pattern_supportToStream(std::stringstream& ss, const Buffer<nlattr> attr);

static const AttributeMap iftypes{
        {NL80211_IFTYPE_UNSPECIFIED, {"UNSPECIFIED", DataType::Flag}},
        {NL80211_IFTYPE_ADHOC, {"ADHOC", DataType::Flag}},
        {NL80211_IFTYPE_STATION, {"STATION", DataType::Flag}},
        {NL80211_IFTYPE_AP, {"AP", DataType::Flag}},
        {NL80211_IFTYPE_AP_VLAN, {"AP_VLAN", DataType::Flag}},
        {NL80211_IFTYPE_WDS, {"WDS", DataType::Flag}},
        {NL80211_IFTYPE_MONITOR, {"MONITOR", DataType::Flag}},
        {NL80211_IFTYPE_MESH_POINT, {"MESH_POINT", DataType::Flag}},
        {NL80211_IFTYPE_P2P_CLIENT, {"P2P_CLIENT", DataType::Flag}},
        {NL80211_IFTYPE_P2P_GO, {"P2P_GO", DataType::Flag}},
        {NL80211_IFTYPE_P2P_DEVICE, {"P2P_DEVICE", DataType::Flag}},
        {NL80211_IFTYPE_OCB, {"OCB", DataType::Flag}},
        {NL80211_IFTYPE_NAN, {"NAN", DataType::Flag}},
};

// clang-format off
Nl80211::Nl80211(nlmsgtype_t familyId) : GenericMessageBase(familyId, "nl80211", {
    /* Script to generate the (initial) top-level list from linux/nl80211.h:
     * sed -e 's/^  NL80211_CMD_\(.*\),$/    {NL80211_CMD_\1, "\1"},/g'
     */
    {NL80211_CMD_UNSPEC, "UNSPEC"},

    {NL80211_CMD_GET_WIPHY, "GET_WIPHY"},
    {NL80211_CMD_SET_WIPHY, "SET_WIPHY"},
    {NL80211_CMD_NEW_WIPHY, "NEW_WIPHY"},
    {NL80211_CMD_DEL_WIPHY, "DEL_WIPHY"},

    {NL80211_CMD_GET_INTERFACE, "GET_INTERFACE"},
    {NL80211_CMD_SET_INTERFACE, "SET_INTERFACE"},
    {NL80211_CMD_NEW_INTERFACE, "NEW_INTERFACE"},
    {NL80211_CMD_DEL_INTERFACE, "DEL_INTERFACE"},

    {NL80211_CMD_GET_KEY, "GET_KEY"},
    {NL80211_CMD_SET_KEY, "SET_KEY"},
    {NL80211_CMD_NEW_KEY, "NEW_KEY"},
    {NL80211_CMD_DEL_KEY, "DEL_KEY"},

    {NL80211_CMD_GET_BEACON, "GET_BEACON"},
    {NL80211_CMD_SET_BEACON, "SET_BEACON"},
    {NL80211_CMD_START_AP, "START_AP"},
    {NL80211_CMD_STOP_AP, "STOP_AP"},

    {NL80211_CMD_GET_STATION, "GET_STATION"},
    {NL80211_CMD_SET_STATION, "SET_STATION"},
    {NL80211_CMD_NEW_STATION, "NEW_STATION"},
    {NL80211_CMD_DEL_STATION, "DEL_STATION"},

    {NL80211_CMD_GET_MPATH, "GET_MPATH"},
    {NL80211_CMD_SET_MPATH, "SET_MPATH"},
    {NL80211_CMD_NEW_MPATH, "NEW_MPATH"},
    {NL80211_CMD_DEL_MPATH, "DEL_MPATH"},

    {NL80211_CMD_SET_BSS, "SET_BSS"},

    {NL80211_CMD_SET_REG, "SET_REG"},
    {NL80211_CMD_REQ_SET_REG, "REQ_SET_REG"},

    {NL80211_CMD_GET_MESH_CONFIG, "GET_MESH_CONFIG"},
    {NL80211_CMD_SET_MESH_CONFIG, "SET_MESH_CONFIG"},

    {NL80211_CMD_SET_MGMT_EXTRA_IE, "SET_MGMT_EXTRA_IE"},

    {NL80211_CMD_GET_REG, "GET_REG"},

    {NL80211_CMD_GET_SCAN, "GET_SCAN"},
    {NL80211_CMD_TRIGGER_SCAN, "TRIGGER_SCAN"},
    {NL80211_CMD_NEW_SCAN_RESULTS, "NEW_SCAN_RESULTS"},
    {NL80211_CMD_SCAN_ABORTED, "SCAN_ABORTED"},

    {NL80211_CMD_REG_CHANGE, "REG_CHANGE"},

    {NL80211_CMD_AUTHENTICATE, "AUTHENTICATE"},
    {NL80211_CMD_ASSOCIATE, "ASSOCIATE"},
    {NL80211_CMD_DEAUTHENTICATE, "DEAUTHENTICATE"},
    {NL80211_CMD_DISASSOCIATE, "DISASSOCIATE"},

    {NL80211_CMD_MICHAEL_MIC_FAILURE, "MICHAEL_MIC_FAILURE"},

    {NL80211_CMD_REG_BEACON_HINT, "REG_BEACON_HINT"},

    {NL80211_CMD_JOIN_IBSS, "JOIN_IBSS"},
    {NL80211_CMD_LEAVE_IBSS, "LEAVE_IBSS"},

    {NL80211_CMD_TESTMODE, "TESTMODE"},

    {NL80211_CMD_CONNECT, "CONNECT"},
    {NL80211_CMD_ROAM, "ROAM"},
    {NL80211_CMD_DISCONNECT, "DISCONNECT"},

    {NL80211_CMD_SET_WIPHY_NETNS, "SET_WIPHY_NETNS"},

    {NL80211_CMD_GET_SURVEY, "GET_SURVEY"},
    {NL80211_CMD_NEW_SURVEY_RESULTS, "NEW_SURVEY_RESULTS"},

    {NL80211_CMD_SET_PMKSA, "SET_PMKSA"},
    {NL80211_CMD_DEL_PMKSA, "DEL_PMKSA"},
    {NL80211_CMD_FLUSH_PMKSA, "FLUSH_PMKSA"},

    {NL80211_CMD_REMAIN_ON_CHANNEL, "REMAIN_ON_CHANNEL"},
    {NL80211_CMD_CANCEL_REMAIN_ON_CHANNEL, "CANCEL_REMAIN_ON_CHANNEL"},

    {NL80211_CMD_SET_TX_BITRATE_MASK, "SET_TX_BITRATE_MASK"},

    {NL80211_CMD_REGISTER_FRAME, "REGISTER_FRAME"},
    {NL80211_CMD_FRAME, "FRAME"},
    {NL80211_CMD_FRAME_TX_STATUS, "FRAME_TX_STATUS"},

    {NL80211_CMD_SET_POWER_SAVE, "SET_POWER_SAVE"},
    {NL80211_CMD_GET_POWER_SAVE, "GET_POWER_SAVE"},

    {NL80211_CMD_SET_CQM, "SET_CQM"},
    {NL80211_CMD_NOTIFY_CQM, "NOTIFY_CQM"},

    {NL80211_CMD_SET_CHANNEL, "SET_CHANNEL"},
    {NL80211_CMD_SET_WDS_PEER, "SET_WDS_PEER"},

    {NL80211_CMD_FRAME_WAIT_CANCEL, "FRAME_WAIT_CANCEL"},

    {NL80211_CMD_JOIN_MESH, "JOIN_MESH"},
    {NL80211_CMD_LEAVE_MESH, "LEAVE_MESH"},

    {NL80211_CMD_UNPROT_DEAUTHENTICATE, "UNPROT_DEAUTHENTICATE"},
    {NL80211_CMD_UNPROT_DISASSOCIATE, "UNPROT_DISASSOCIATE"},

    {NL80211_CMD_NEW_PEER_CANDIDATE, "NEW_PEER_CANDIDATE"},

    {NL80211_CMD_GET_WOWLAN, "GET_WOWLAN"},
    {NL80211_CMD_SET_WOWLAN, "SET_WOWLAN"},

    {NL80211_CMD_START_SCHED_SCAN, "START_SCHED_SCAN"},
    {NL80211_CMD_STOP_SCHED_SCAN, "STOP_SCHED_SCAN"},
    {NL80211_CMD_SCHED_SCAN_RESULTS, "SCHED_SCAN_RESULTS"},
    {NL80211_CMD_SCHED_SCAN_STOPPED, "SCHED_SCAN_STOPPED"},

    {NL80211_CMD_SET_REKEY_OFFLOAD, "SET_REKEY_OFFLOAD"},

    {NL80211_CMD_PMKSA_CANDIDATE, "PMKSA_CANDIDATE"},

    {NL80211_CMD_TDLS_OPER, "TDLS_OPER"},
    {NL80211_CMD_TDLS_MGMT, "TDLS_MGMT"},

    {NL80211_CMD_UNEXPECTED_FRAME, "UNEXPECTED_FRAME"},

    {NL80211_CMD_PROBE_CLIENT, "PROBE_CLIENT"},

    {NL80211_CMD_REGISTER_BEACONS, "REGISTER_BEACONS"},

    {NL80211_CMD_UNEXPECTED_4ADDR_FRAME, "UNEXPECTED_4ADDR_FRAME"},

    {NL80211_CMD_SET_NOACK_MAP, "SET_NOACK_MAP"},

    {NL80211_CMD_CH_SWITCH_NOTIFY, "CH_SWITCH_NOTIFY"},

    {NL80211_CMD_START_P2P_DEVICE, "START_P2P_DEVICE"},
    {NL80211_CMD_STOP_P2P_DEVICE, "STOP_P2P_DEVICE"},

    {NL80211_CMD_CONN_FAILED, "CONN_FAILED"},

    {NL80211_CMD_SET_MCAST_RATE, "SET_MCAST_RATE"},

    {NL80211_CMD_SET_MAC_ACL, "SET_MAC_ACL"},

    {NL80211_CMD_RADAR_DETECT, "RADAR_DETECT"},

    {NL80211_CMD_GET_PROTOCOL_FEATURES, "GET_PROTOCOL_FEATURES"},

    {NL80211_CMD_UPDATE_FT_IES, "UPDATE_FT_IES"},
    {NL80211_CMD_FT_EVENT, "FT_EVENT"},

    {NL80211_CMD_CRIT_PROTOCOL_START, "CRIT_PROTOCOL_START"},
    {NL80211_CMD_CRIT_PROTOCOL_STOP, "CRIT_PROTOCOL_STOP"},

    {NL80211_CMD_GET_COALESCE, "GET_COALESCE"},
    {NL80211_CMD_SET_COALESCE, "SET_COALESCE"},

    {NL80211_CMD_CHANNEL_SWITCH, "CHANNEL_SWITCH"},

    {NL80211_CMD_VENDOR, "VENDOR"},

    {NL80211_CMD_SET_QOS_MAP, "SET_QOS_MAP"},

    {NL80211_CMD_ADD_TX_TS, "ADD_TX_TS"},
    {NL80211_CMD_DEL_TX_TS, "DEL_TX_TS"},

    {NL80211_CMD_GET_MPP, "GET_MPP"},

    {NL80211_CMD_JOIN_OCB, "JOIN_OCB"},
    {NL80211_CMD_LEAVE_OCB, "LEAVE_OCB"},

    {NL80211_CMD_CH_SWITCH_STARTED_NOTIFY, "CH_SWITCH_STARTED_NOTIFY"},

    {NL80211_CMD_TDLS_CHANNEL_SWITCH, "TDLS_CHANNEL_SWITCH"},
    {NL80211_CMD_TDLS_CANCEL_CHANNEL_SWITCH, "TDLS_CANCEL_CHANNEL_SWITCH"},

    {NL80211_CMD_WIPHY_REG_CHANGE, "WIPHY_REG_CHANGE"},

    {NL80211_CMD_ABORT_SCAN, "ABORT_SCAN"},

    {NL80211_CMD_START_NAN, "START_NAN"},
    {NL80211_CMD_STOP_NAN, "STOP_NAN"},
    {NL80211_CMD_ADD_NAN_FUNCTION, "ADD_NAN_FUNCTION"},
    {NL80211_CMD_DEL_NAN_FUNCTION, "DEL_NAN_FUNCTION"},
    {NL80211_CMD_CHANGE_NAN_CONFIG, "CHANGE_NAN_CONFIG"},
    {NL80211_CMD_NAN_MATCH, "NAN_MATCH"},

    {NL80211_CMD_SET_MULTICAST_TO_UNICAST, "SET_MULTICAST_TO_UNICAST"},

    {NL80211_CMD_UPDATE_CONNECT_PARAMS, "UPDATE_CONNECT_PARAMS"},

    {NL80211_CMD_SET_PMK, "SET_PMK"},
    {NL80211_CMD_DEL_PMK, "DEL_PMK"},

    {NL80211_CMD_PORT_AUTHORIZED, "PORT_AUTHORIZED"},

    {NL80211_CMD_RELOAD_REGDB, "RELOAD_REGDB"},

    {NL80211_CMD_EXTERNAL_AUTH, "EXTERNAL_AUTH"},

    {NL80211_CMD_STA_OPMODE_CHANGED, "STA_OPMODE_CHANGED"},

    {NL80211_CMD_CONTROL_PORT_FRAME, "CONTROL_PORT_FRAME"},

    {NL80211_CMD_GET_FTM_RESPONDER_STATS, "GET_FTM_RESPONDER_STATS"},

    {NL80211_CMD_PEER_MEASUREMENT_START, "PEER_MEASUREMENT_START"},
    {NL80211_CMD_PEER_MEASUREMENT_RESULT, "PEER_MEASUREMENT_RESULT"},
    {NL80211_CMD_PEER_MEASUREMENT_COMPLETE, "PEER_MEASUREMENT_COMPLETE"},

    {NL80211_CMD_NOTIFY_RADAR, "NOTIFY_RADAR"},

    {NL80211_CMD_UPDATE_OWE_INFO, "UPDATE_OWE_INFO"},

    {NL80211_CMD_PROBE_MESH_LINK, "PROBE_MESH_LINK"},

    {NL80211_CMD_SET_TID_CONFIG, "SET_TID_CONFIG"},

    {NL80211_CMD_UNPROT_BEACON, "UNPROT_BEACON"},

    {NL80211_CMD_CONTROL_PORT_FRAME_TX_STATUS, "CONTROL_PORT_FRAME_TX_STATUS"},
}, {
    /* Script to generate the (initial) top-level list from linux/nl80211.h:
     * sed -e 's/^\tNL80211_ATTR_\(.*\),$/    {NL80211_ATTR_\1, {"\1"}},/g'
     */
    {NL80211_ATTR_UNSPEC, {"UNSPEC"}},

    {NL80211_ATTR_WIPHY, {"WIPHY", DataType::Uint}},
    {NL80211_ATTR_WIPHY_NAME, {"WIPHY_NAME", DataType::StringNul}},

    {NL80211_ATTR_IFINDEX, {"IFINDEX", DataType::Uint}},
    {NL80211_ATTR_IFNAME, {"IFNAME", DataType::StringNul}},
    {NL80211_ATTR_IFTYPE, {"IFTYPE", DataType::Uint}},

    {NL80211_ATTR_MAC, {"MAC", DataType::Raw}},

    {NL80211_ATTR_KEY_DATA, {"KEY_DATA"}},
    {NL80211_ATTR_KEY_IDX, {"KEY_IDX"}},
    {NL80211_ATTR_KEY_CIPHER, {"KEY_CIPHER"}},
    {NL80211_ATTR_KEY_SEQ, {"KEY_SEQ"}},
    {NL80211_ATTR_KEY_DEFAULT, {"KEY_DEFAULT"}},

    {NL80211_ATTR_BEACON_INTERVAL, {"BEACON_INTERVAL"}},
    {NL80211_ATTR_DTIM_PERIOD, {"DTIM_PERIOD"}},
    {NL80211_ATTR_BEACON_HEAD, {"BEACON_HEAD"}},
    {NL80211_ATTR_BEACON_TAIL, {"BEACON_TAIL"}},

    {NL80211_ATTR_STA_AID, {"STA_AID"}},
    {NL80211_ATTR_STA_FLAGS, {"STA_FLAGS"}},
    {NL80211_ATTR_STA_LISTEN_INTERVAL, {"STA_LISTEN_INTERVAL"}},
    {NL80211_ATTR_STA_SUPPORTED_RATES, {"STA_SUPPORTED_RATES"}},
    {NL80211_ATTR_STA_VLAN, {"STA_VLAN"}},
    {NL80211_ATTR_STA_INFO, {"STA_INFO"}},

    {NL80211_ATTR_WIPHY_BANDS, {"WIPHY_BANDS", DataType::Nested, AttributeMap{
        {std::nullopt, {"BAND", DataType::Nested, AttributeMap{
            {NL80211_BAND_ATTR_FREQS, {"FREQS", DataType::Nested, AttributeMap{
                {std::nullopt, {"FQ", DataType::Nested, AttributeMap{
                    {NL80211_FREQUENCY_ATTR_FREQ, {"FREQ", DataType::Uint}},
                    {NL80211_FREQUENCY_ATTR_DISABLED, {"DISABLED", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_NO_IR, {"NO_IR", DataType::Flag}},
                    {__NL80211_FREQUENCY_ATTR_NO_IBSS, {"_NO_IBSS", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_RADAR, {"RADAR", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_MAX_TX_POWER, {"MAX_TX_POWER", DataType::Uint}},
                    {NL80211_FREQUENCY_ATTR_DFS_STATE, {"DFS_STATE", DataType::Uint}},
                    {NL80211_FREQUENCY_ATTR_DFS_TIME, {"DFS_TIME", DataType::Uint}},
                    {NL80211_FREQUENCY_ATTR_NO_HT40_MINUS, {"NO_HT40_MINUS", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_NO_HT40_PLUS, {"NO_HT40_PLUS", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_NO_80MHZ, {"NO_80MHZ", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_NO_160MHZ, {"NO_160MHZ", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_DFS_CAC_TIME, {"DFS_CAC_TIME", DataType::Uint}},
                    {NL80211_FREQUENCY_ATTR_INDOOR_ONLY, {"INDOOR_ONLY", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_IR_CONCURRENT, {"IR_CONCURRENT", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_NO_20MHZ, {"NO_20MHZ", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_NO_10MHZ, {"NO_10MHZ", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_WMM, {"WMM"}},
                    {NL80211_FREQUENCY_ATTR_NO_HE, {"NO_HE", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_OFFSET, {"OFFSET", DataType::Uint}},
                    {NL80211_FREQUENCY_ATTR_1MHZ, {"1MHZ", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_2MHZ, {"2MHZ", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_4MHZ, {"4MHZ", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_8MHZ, {"8MHZ", DataType::Flag}},
                    {NL80211_FREQUENCY_ATTR_16MHZ, {"16MHZ", DataType::Flag}},
                }}},
            }, Flags::Verbose}},
            {NL80211_BAND_ATTR_RATES, {"RATES", DataType::Nested, AttributeMap{
                {std::nullopt, {"RATE", DataType::Nested, AttributeMap{
                    {NL80211_BITRATE_ATTR_RATE, {"RATE", DataType::Uint}},
                    {NL80211_BITRATE_ATTR_2GHZ_SHORTPREAMBLE,
                            {"2GHZ_SHORTPREAMBLE", DataType::Flag}},
                }}},
            }}},

            {NL80211_BAND_ATTR_HT_MCS_SET, {"HT_MCS_SET"}},  // struct ieee80211_mcs_info
            {NL80211_BAND_ATTR_HT_CAPA, {"HT_CAPA", DataType::Uint}},
            {NL80211_BAND_ATTR_HT_AMPDU_FACTOR, {"HT_AMPDU_FACTOR", DataType::Uint}},
            {NL80211_BAND_ATTR_HT_AMPDU_DENSITY, {"HT_AMPDU_DENSITY", DataType::Uint}},

            {NL80211_BAND_ATTR_VHT_MCS_SET, {"VHT_MCS_SET"}},  // struct ieee80211_vht_mcs_info
            {NL80211_BAND_ATTR_VHT_CAPA, {"VHT_CAPA", DataType::Uint}},
            {NL80211_BAND_ATTR_IFTYPE_DATA, {"IFTYPE_DATA"}},

            {NL80211_BAND_ATTR_EDMG_CHANNELS, {"EDMG_CHANNELS"}},
            {NL80211_BAND_ATTR_EDMG_BW_CONFIG, {"EDMG_BW_CONFIG"}},
        }}},
    }, Flags::Verbose}},

    {NL80211_ATTR_MNTR_FLAGS, {"MNTR_FLAGS"}},

    {NL80211_ATTR_MESH_ID, {"MESH_ID"}},
    {NL80211_ATTR_STA_PLINK_ACTION, {"STA_PLINK_ACTION"}},
    {NL80211_ATTR_MPATH_NEXT_HOP, {"MPATH_NEXT_HOP"}},
    {NL80211_ATTR_MPATH_INFO, {"MPATH_INFO"}},

    {NL80211_ATTR_BSS_CTS_PROT, {"BSS_CTS_PROT"}},
    {NL80211_ATTR_BSS_SHORT_PREAMBLE, {"BSS_SHORT_PREAMBLE"}},
    {NL80211_ATTR_BSS_SHORT_SLOT_TIME, {"BSS_SHORT_SLOT_TIME"}},

    {NL80211_ATTR_HT_CAPABILITY, {"HT_CAPABILITY"}},

    {NL80211_ATTR_SUPPORTED_IFTYPES, {"SUPPORTED_IFTYPES", DataType::Nested, iftypes}},

    {NL80211_ATTR_REG_ALPHA2, {"REG_ALPHA2"}},
    {NL80211_ATTR_REG_RULES, {"REG_RULES"}},

    {NL80211_ATTR_MESH_CONFIG, {"MESH_CONFIG"}},

    {NL80211_ATTR_BSS_BASIC_RATES, {"BSS_BASIC_RATES"}},

    {NL80211_ATTR_WIPHY_TXQ_PARAMS, {"WIPHY_TXQ_PARAMS"}},
    {NL80211_ATTR_WIPHY_FREQ, {"WIPHY_FREQ"}},
    {NL80211_ATTR_WIPHY_CHANNEL_TYPE, {"WIPHY_CHANNEL_TYPE"}},

    {NL80211_ATTR_KEY_DEFAULT_MGMT, {"KEY_DEFAULT_MGMT"}},

    {NL80211_ATTR_MGMT_SUBTYPE, {"MGMT_SUBTYPE"}},
    {NL80211_ATTR_IE, {"IE"}},

    {NL80211_ATTR_MAX_NUM_SCAN_SSIDS, {"MAX_NUM_SCAN_SSIDS", DataType::Uint}},

    {NL80211_ATTR_SCAN_FREQUENCIES, {"SCAN_FREQUENCIES", DataType::Nested, AttributeMap{
        {std::nullopt, {"FQ", DataType::Uint}},
    }, Flags::Verbose}},
    {NL80211_ATTR_SCAN_SSIDS, {"SCAN_SSIDS", DataType::Nested, AttributeMap{
        {std::nullopt, {"SSID", DataType::String}},
    }}},
    {NL80211_ATTR_GENERATION, {"GENERATION", DataType::Uint}},
    {NL80211_ATTR_BSS, {"BSS", DataType::Nested, AttributeMap{
        {NL80211_BSS_BSSID, {"BSSID", DataType::Raw}},
        {NL80211_BSS_FREQUENCY, {"FREQUENCY", DataType::Uint}},
        {NL80211_BSS_TSF, {"TSF", DataType::Uint}},
        {NL80211_BSS_BEACON_INTERVAL, {"BEACON_INTERVAL", DataType::Uint}},
        {NL80211_BSS_CAPABILITY, {"CAPABILITY", DataType::Uint}},
        {NL80211_BSS_INFORMATION_ELEMENTS, {"INFORMATION_ELEMENTS",
                DataType::Struct, informationElementsToStream}},
        {NL80211_BSS_SIGNAL_MBM, {"SIGNAL_MBM", DataType::Uint}},
        {NL80211_BSS_SIGNAL_UNSPEC, {"SIGNAL_UNSPEC", DataType::Uint}},
        {NL80211_BSS_STATUS, {"STATUS", DataType::Uint}},  // enum nl80211_bss_status
        {NL80211_BSS_SEEN_MS_AGO, {"SEEN_MS_AGO", DataType::Uint}},
        {NL80211_BSS_BEACON_IES, {"BEACON_IES", DataType::Struct, informationElementsToStream}},
        {NL80211_BSS_CHAN_WIDTH, {"CHAN_WIDTH", DataType::Uint}},
        {NL80211_BSS_BEACON_TSF, {"BEACON_TSF", DataType::Uint}},
        {NL80211_BSS_PRESP_DATA, {"PRESP_DATA", DataType::Flag}},
        {NL80211_BSS_LAST_SEEN_BOOTTIME, {"LAST_SEEN_BOOTTIME", DataType::Uint}},
        {NL80211_BSS_PAD, {"PAD"}},
        {NL80211_BSS_PARENT_TSF, {"PARENT_TSF"}},
        {NL80211_BSS_PARENT_BSSID, {"PARENT_BSSID"}},
        {NL80211_BSS_CHAIN_SIGNAL, {"CHAIN_SIGNAL", DataType::Nested, AttributeMap{
            {std::nullopt, {"SIG", DataType::Uint}},
        }}},
        {NL80211_BSS_FREQUENCY_OFFSET, {"FREQUENCY_OFFSET"}},
    }}},

    {NL80211_ATTR_REG_INITIATOR, {"REG_INITIATOR"}},
    {NL80211_ATTR_REG_TYPE, {"REG_TYPE"}},

    {NL80211_ATTR_SUPPORTED_COMMANDS, {"SUPPORTED_COMMANDS", DataType::Nested,AttributeMap{
        {std::nullopt, {"CMD", DataType::Uint}}, // enum nl80211_commands
    }}},

    {NL80211_ATTR_FRAME, {"FRAME"}},
    {NL80211_ATTR_SSID, {"SSID"}},
    {NL80211_ATTR_AUTH_TYPE, {"AUTH_TYPE"}},
    {NL80211_ATTR_REASON_CODE, {"REASON_CODE"}},

    {NL80211_ATTR_KEY_TYPE, {"KEY_TYPE"}},

    {NL80211_ATTR_MAX_SCAN_IE_LEN, {"MAX_SCAN_IE_LEN", DataType::Uint}},
    {NL80211_ATTR_CIPHER_SUITES, {"CIPHER_SUITES", DataType::Struct, arrayToStream<int32_t>}},

    {NL80211_ATTR_FREQ_BEFORE, {"FREQ_BEFORE"}},
    {NL80211_ATTR_FREQ_AFTER, {"FREQ_AFTER"}},

    {NL80211_ATTR_FREQ_FIXED, {"FREQ_FIXED"}},

    {NL80211_ATTR_WIPHY_RETRY_SHORT, {"WIPHY_RETRY_SHORT", DataType::Uint}},
    {NL80211_ATTR_WIPHY_RETRY_LONG, {"WIPHY_RETRY_LONG", DataType::Uint}},
    {NL80211_ATTR_WIPHY_FRAG_THRESHOLD, {"WIPHY_FRAG_THRESHOLD", DataType::Uint}},
    {NL80211_ATTR_WIPHY_RTS_THRESHOLD, {"WIPHY_RTS_THRESHOLD", DataType::Uint}},

    {NL80211_ATTR_TIMED_OUT, {"TIMED_OUT"}},

    {NL80211_ATTR_USE_MFP, {"USE_MFP"}},

    {NL80211_ATTR_STA_FLAGS2, {"STA_FLAGS2"}},

    {NL80211_ATTR_CONTROL_PORT, {"CONTROL_PORT"}},

    {NL80211_ATTR_TESTDATA, {"TESTDATA"}},

    {NL80211_ATTR_PRIVACY, {"PRIVACY"}},

    {NL80211_ATTR_DISCONNECTED_BY_AP, {"DISCONNECTED_BY_AP"}},
    {NL80211_ATTR_STATUS_CODE, {"STATUS_CODE"}},

    {NL80211_ATTR_CIPHER_SUITES_PAIRWISE, {"CIPHER_SUITES_PAIRWISE"}},
    {NL80211_ATTR_CIPHER_SUITE_GROUP, {"CIPHER_SUITE_GROUP"}},
    {NL80211_ATTR_WPA_VERSIONS, {"WPA_VERSIONS"}},
    {NL80211_ATTR_AKM_SUITES, {"AKM_SUITES"}},

    {NL80211_ATTR_REQ_IE, {"REQ_IE"}},
    {NL80211_ATTR_RESP_IE, {"RESP_IE"}},

    {NL80211_ATTR_PREV_BSSID, {"PREV_BSSID"}},

    {NL80211_ATTR_KEY, {"KEY"}},
    {NL80211_ATTR_KEYS, {"KEYS"}},

    {NL80211_ATTR_PID, {"PID"}},

    {NL80211_ATTR_4ADDR, {"4ADDR"}},

    {NL80211_ATTR_SURVEY_INFO, {"SURVEY_INFO"}},

    {NL80211_ATTR_PMKID, {"PMKID"}},
    {NL80211_ATTR_MAX_NUM_PMKIDS, {"MAX_NUM_PMKIDS", DataType::Uint}},

    {NL80211_ATTR_DURATION, {"DURATION"}},

    {NL80211_ATTR_COOKIE, {"COOKIE"}},

    {NL80211_ATTR_WIPHY_COVERAGE_CLASS, {"WIPHY_COVERAGE_CLASS", DataType::Uint}},

    {NL80211_ATTR_TX_RATES, {"TX_RATES"}},

    {NL80211_ATTR_FRAME_MATCH, {"FRAME_MATCH"}},

    {NL80211_ATTR_ACK, {"ACK"}},

    {NL80211_ATTR_PS_STATE, {"PS_STATE"}},

    {NL80211_ATTR_CQM, {"CQM"}},

    {NL80211_ATTR_LOCAL_STATE_CHANGE, {"LOCAL_STATE_CHANGE"}},

    {NL80211_ATTR_AP_ISOLATE, {"AP_ISOLATE"}},

    {NL80211_ATTR_WIPHY_TX_POWER_SETTING, {"WIPHY_TX_POWER_SETTING"}},
    {NL80211_ATTR_WIPHY_TX_POWER_LEVEL, {"WIPHY_TX_POWER_LEVEL"}},

    {NL80211_ATTR_TX_FRAME_TYPES, {"TX_FRAME_TYPES", DataType::Nested, AttributeMap{
        {std::nullopt, {"TFT", DataType::Nested, AttributeMap{
            {NL80211_ATTR_FRAME_TYPE, {"FRAME_TYPE", DataType::Uint}},
        }}},
    }, Flags::Verbose}},
    {NL80211_ATTR_RX_FRAME_TYPES, {"RX_FRAME_TYPES", DataType::Nested, AttributeMap{
        {std::nullopt, {"RFT", DataType::Nested, AttributeMap{
            {NL80211_ATTR_FRAME_TYPE, {"FRAME_TYPE", DataType::Uint}},
        }}},
    }, Flags::Verbose}},

    {NL80211_ATTR_FRAME_TYPE, {"FRAME_TYPE", DataType::Uint}},

    {NL80211_ATTR_CONTROL_PORT_ETHERTYPE, {"CONTROL_PORT_ETHERTYPE"}},
    {NL80211_ATTR_CONTROL_PORT_NO_ENCRYPT, {"CONTROL_PORT_NO_ENCRYPT"}},

    {NL80211_ATTR_SUPPORT_IBSS_RSN, {"SUPPORT_IBSS_RSN"}},

    {NL80211_ATTR_WIPHY_ANTENNA_TX, {"WIPHY_ANTENNA_TX"}},
    {NL80211_ATTR_WIPHY_ANTENNA_RX, {"WIPHY_ANTENNA_RX"}},

    {NL80211_ATTR_MCAST_RATE, {"MCAST_RATE"}},

    {NL80211_ATTR_OFFCHANNEL_TX_OK, {"OFFCHANNEL_TX_OK", DataType::Flag}},

    {NL80211_ATTR_BSS_HT_OPMODE, {"BSS_HT_OPMODE"}},

    {NL80211_ATTR_KEY_DEFAULT_TYPES, {"KEY_DEFAULT_TYPES"}},

    {NL80211_ATTR_MAX_REMAIN_ON_CHANNEL_DURATION,
            {"MAX_REMAIN_ON_CHANNEL_DURATION", DataType::Uint}},

    {NL80211_ATTR_MESH_SETUP, {"MESH_SETUP"}},

    {NL80211_ATTR_WIPHY_ANTENNA_AVAIL_TX, {"WIPHY_ANTENNA_AVAIL_TX", DataType::Uint}},
    {NL80211_ATTR_WIPHY_ANTENNA_AVAIL_RX, {"WIPHY_ANTENNA_AVAIL_RX", DataType::Uint}},

    {NL80211_ATTR_SUPPORT_MESH_AUTH, {"SUPPORT_MESH_AUTH"}},
    {NL80211_ATTR_STA_PLINK_STATE, {"STA_PLINK_STATE"}},

    {NL80211_ATTR_WOWLAN_TRIGGERS, {"WOWLAN_TRIGGERS"}},
    {NL80211_ATTR_WOWLAN_TRIGGERS_SUPPORTED,
            {"WOWLAN_TRIGGERS_SUPPORTED", DataType::Nested, AttributeMap{
        {NL80211_WOWLAN_TRIG_ANY, {"ANY", DataType::Flag}},
        {NL80211_WOWLAN_TRIG_DISCONNECT, {"DISCONNECT", DataType::Flag}},
        {NL80211_WOWLAN_TRIG_MAGIC_PKT, {"MAGIC_PKT", DataType::Flag}},
        {NL80211_WOWLAN_TRIG_PKT_PATTERN,
                {"PKT_PATTERN", DataType::Struct, nl80211_pattern_supportToStream}},
        {NL80211_WOWLAN_TRIG_GTK_REKEY_SUPPORTED, {"GTK_REKEY_SUPPORTED", DataType::Flag}},
        {NL80211_WOWLAN_TRIG_GTK_REKEY_FAILURE, {"GTK_REKEY_FAILURE", DataType::Flag}},
        {NL80211_WOWLAN_TRIG_EAP_IDENT_REQUEST, {"EAP_IDENT_REQUEST", DataType::Flag}},
        {NL80211_WOWLAN_TRIG_4WAY_HANDSHAKE, {"4WAY_HANDSHAKE", DataType::Flag}},
        {NL80211_WOWLAN_TRIG_RFKILL_RELEASE, {"RFKILL_RELEASE", DataType::Flag}},
        {NL80211_WOWLAN_TRIG_TCP_CONNECTION, {"TCP_CONNECTION", DataType::Nested, AttributeMap{
            {NL80211_WOWLAN_TCP_SRC_IPV4, {"SRC_IPV4"}},
            {NL80211_WOWLAN_TCP_DST_IPV4, {"DST_IPV4"}},
            {NL80211_WOWLAN_TCP_DST_MAC, {"DST_MAC"}},
            {NL80211_WOWLAN_TCP_SRC_PORT, {"SRC_PORT", DataType::Uint}},
            {NL80211_WOWLAN_TCP_DST_PORT, {"DST_PORT", DataType::Uint}},
            {NL80211_WOWLAN_TCP_DATA_PAYLOAD, {"DATA_PAYLOAD"}},
            {NL80211_WOWLAN_TCP_DATA_PAYLOAD_SEQ, {"DATA_PAYLOAD_SEQ"}},
            {NL80211_WOWLAN_TCP_DATA_PAYLOAD_TOKEN, {"DATA_PAYLOAD_TOKEN"}},
            {NL80211_WOWLAN_TCP_DATA_INTERVAL, {"DATA_INTERVAL", DataType::Uint}},
            {NL80211_WOWLAN_TCP_WAKE_PAYLOAD, {"WAKE_PAYLOAD"}},
            {NL80211_WOWLAN_TCP_WAKE_MASK, {"WAKE_MASK"}},
        }}},
        {NL80211_WOWLAN_TRIG_NET_DETECT, {"NET_DETECT", DataType::Uint}},

        /* Not in WOWLAN_TRIGGERS_SUPPORTED:
         * - NL80211_WOWLAN_TRIG_WAKEUP_PKT_80211
         * - NL80211_WOWLAN_TRIG_WAKEUP_PKT_80211_LEN
         * - NL80211_WOWLAN_TRIG_WAKEUP_PKT_8023
         * - NL80211_WOWLAN_TRIG_WAKEUP_PKT_8023_LEN
         * - NL80211_WOWLAN_TRIG_WAKEUP_TCP_MATCH
         * - NL80211_WOWLAN_TRIG_WAKEUP_TCP_CONNLOST
         * - NL80211_WOWLAN_TRIG_WAKEUP_TCP_NOMORETOKENS
         * - NL80211_WOWLAN_TRIG_NET_DETECT_RESULTS
         */
    }}},

    {NL80211_ATTR_SCHED_SCAN_INTERVAL, {"SCHED_SCAN_INTERVAL"}},

    {NL80211_ATTR_INTERFACE_COMBINATIONS, {"INTERFACE_COMBINATIONS", DataType::Nested, AttributeMap{
        {std::nullopt, {"IC", DataType::Nested, AttributeMap{
            {NL80211_IFACE_COMB_UNSPEC, {"UNSPEC"}},
            {NL80211_IFACE_COMB_LIMITS, {"LIMITS", DataType::Nested, AttributeMap{
                {std::nullopt, {"LT", DataType::Nested, AttributeMap{
                    {NL80211_IFACE_LIMIT_UNSPEC, {"UNSPEC"}},
                    {NL80211_IFACE_LIMIT_MAX, {"MAX", DataType::Uint}},
                    {NL80211_IFACE_LIMIT_TYPES, {"TYPES", DataType::Nested, iftypes}},
                }}},
            }}},
            {NL80211_IFACE_COMB_MAXNUM, {"MAXNUM", DataType::Uint}},
            {NL80211_IFACE_COMB_STA_AP_BI_MATCH, {"STA_AP_BI_MATCH", DataType::Flag}},
            {NL80211_IFACE_COMB_NUM_CHANNELS, {"NUM_CHANNELS", DataType::Uint}},
            {NL80211_IFACE_COMB_RADAR_DETECT_WIDTHS, {"RADAR_DETECT_WIDTHS", DataType::Uint}},
            {NL80211_IFACE_COMB_RADAR_DETECT_REGIONS, {"RADAR_DETECT_REGIONS", DataType::Uint}},
            {NL80211_IFACE_COMB_BI_MIN_GCD, {"BI_MIN_GCD"}},
        }}},
    }, Flags::Verbose}},
    {NL80211_ATTR_SOFTWARE_IFTYPES, {"SOFTWARE_IFTYPES", DataType::Nested, iftypes}},

    {NL80211_ATTR_REKEY_DATA, {"REKEY_DATA"}},

    {NL80211_ATTR_MAX_NUM_SCHED_SCAN_SSIDS, {"MAX_NUM_SCHED_SCAN_SSIDS", DataType::Uint}},
    {NL80211_ATTR_MAX_SCHED_SCAN_IE_LEN, {"MAX_SCHED_SCAN_IE_LEN", DataType::Uint}},

    {NL80211_ATTR_SCAN_SUPP_RATES, {"SCAN_SUPP_RATES"}},

    {NL80211_ATTR_HIDDEN_SSID, {"HIDDEN_SSID"}},

    {NL80211_ATTR_IE_PROBE_RESP, {"IE_PROBE_RESP"}},
    {NL80211_ATTR_IE_ASSOC_RESP, {"IE_ASSOC_RESP"}},

    {NL80211_ATTR_STA_WME, {"STA_WME"}},
    {NL80211_ATTR_SUPPORT_AP_UAPSD, {"SUPPORT_AP_UAPSD"}},

    {NL80211_ATTR_ROAM_SUPPORT, {"ROAM_SUPPORT", DataType::Flag}},

    {NL80211_ATTR_SCHED_SCAN_MATCH, {"SCHED_SCAN_MATCH"}},
    {NL80211_ATTR_MAX_MATCH_SETS, {"MAX_MATCH_SETS", DataType::Uint}},

    {NL80211_ATTR_PMKSA_CANDIDATE, {"PMKSA_CANDIDATE"}},

    {NL80211_ATTR_TX_NO_CCK_RATE, {"TX_NO_CCK_RATE"}},

    {NL80211_ATTR_TDLS_ACTION, {"TDLS_ACTION"}},
    {NL80211_ATTR_TDLS_DIALOG_TOKEN, {"TDLS_DIALOG_TOKEN"}},
    {NL80211_ATTR_TDLS_OPERATION, {"TDLS_OPERATION"}},
    {NL80211_ATTR_TDLS_SUPPORT, {"TDLS_SUPPORT", DataType::Flag}},
    {NL80211_ATTR_TDLS_EXTERNAL_SETUP, {"TDLS_EXTERNAL_SETUP", DataType::Flag}},

    {NL80211_ATTR_DEVICE_AP_SME, {"DEVICE_AP_SME", DataType::Uint}},

    {NL80211_ATTR_DONT_WAIT_FOR_ACK, {"DONT_WAIT_FOR_ACK"}},

    {NL80211_ATTR_FEATURE_FLAGS, {"FEATURE_FLAGS", DataType::Uint}},

    {NL80211_ATTR_PROBE_RESP_OFFLOAD, {"PROBE_RESP_OFFLOAD", DataType::Uint}},

    {NL80211_ATTR_PROBE_RESP, {"PROBE_RESP"}},

    {NL80211_ATTR_DFS_REGION, {"DFS_REGION"}},

    {NL80211_ATTR_DISABLE_HT, {"DISABLE_HT"}},
    {NL80211_ATTR_HT_CAPABILITY_MASK, {"HT_CAPABILITY_MASK"}},

    {NL80211_ATTR_NOACK_MAP, {"NOACK_MAP"}},

    {NL80211_ATTR_INACTIVITY_TIMEOUT, {"INACTIVITY_TIMEOUT"}},

    {NL80211_ATTR_RX_SIGNAL_DBM, {"RX_SIGNAL_DBM"}},

    {NL80211_ATTR_BG_SCAN_PERIOD, {"BG_SCAN_PERIOD"}},

    {NL80211_ATTR_WDEV, {"WDEV", DataType::Uint}},

    {NL80211_ATTR_USER_REG_HINT_TYPE, {"USER_REG_HINT_TYPE"}},

    {NL80211_ATTR_CONN_FAILED_REASON, {"CONN_FAILED_REASON"}},

    {NL80211_ATTR_AUTH_DATA, {"AUTH_DATA"}},

    {NL80211_ATTR_VHT_CAPABILITY, {"VHT_CAPABILITY"}},

    {NL80211_ATTR_SCAN_FLAGS, {"SCAN_FLAGS", DataType::Uint}},

    {NL80211_ATTR_CHANNEL_WIDTH, {"CHANNEL_WIDTH"}},
    {NL80211_ATTR_CENTER_FREQ1, {"CENTER_FREQ1"}},
    {NL80211_ATTR_CENTER_FREQ2, {"CENTER_FREQ2"}},

    {NL80211_ATTR_P2P_CTWINDOW, {"P2P_CTWINDOW"}},
    {NL80211_ATTR_P2P_OPPPS, {"P2P_OPPPS"}},

    {NL80211_ATTR_LOCAL_MESH_POWER_MODE, {"LOCAL_MESH_POWER_MODE"}},

    {NL80211_ATTR_ACL_POLICY, {"ACL_POLICY"}},

    {NL80211_ATTR_MAC_ADDRS, {"MAC_ADDRS"}},

    {NL80211_ATTR_MAC_ACL_MAX, {"MAC_ACL_MAX", DataType::Uint}},

    {NL80211_ATTR_RADAR_EVENT, {"RADAR_EVENT"}},

    {NL80211_ATTR_EXT_CAPA, {"EXT_CAPA"}},
    {NL80211_ATTR_EXT_CAPA_MASK, {"EXT_CAPA_MASK"}},

    {NL80211_ATTR_STA_CAPABILITY, {"STA_CAPABILITY"}},
    {NL80211_ATTR_STA_EXT_CAPABILITY, {"STA_EXT_CAPABILITY"}},

    {NL80211_ATTR_PROTOCOL_FEATURES, {"PROTOCOL_FEATURES", DataType::Uint}},
    {NL80211_ATTR_SPLIT_WIPHY_DUMP, {"SPLIT_WIPHY_DUMP", DataType::Flag}},

    {NL80211_ATTR_DISABLE_VHT, {"DISABLE_VHT", DataType::Flag}},
    {NL80211_ATTR_VHT_CAPABILITY_MASK, {"VHT_CAPABILITY_MASK"}},

    {NL80211_ATTR_MDID, {"MDID"}},
    {NL80211_ATTR_IE_RIC, {"IE_RIC"}},

    {NL80211_ATTR_CRIT_PROT_ID, {"CRIT_PROT_ID"}},
    {NL80211_ATTR_MAX_CRIT_PROT_DURATION, {"MAX_CRIT_PROT_DURATION"}},

    {NL80211_ATTR_PEER_AID, {"PEER_AID"}},

    {NL80211_ATTR_COALESCE_RULE, {"COALESCE_RULE"}},

    {NL80211_ATTR_CH_SWITCH_COUNT, {"CH_SWITCH_COUNT"}},
    {NL80211_ATTR_CH_SWITCH_BLOCK_TX, {"CH_SWITCH_BLOCK_TX"}},
    {NL80211_ATTR_CSA_IES, {"CSA_IES"}},
    {NL80211_ATTR_CNTDWN_OFFS_BEACON, {"CNTDWN_OFFS_BEACON"}},
    {NL80211_ATTR_CNTDWN_OFFS_PRESP, {"CNTDWN_OFFS_PRESP"}},

    {NL80211_ATTR_RXMGMT_FLAGS, {"RXMGMT_FLAGS"}},

    {NL80211_ATTR_STA_SUPPORTED_CHANNELS, {"STA_SUPPORTED_CHANNELS"}},

    {NL80211_ATTR_STA_SUPPORTED_OPER_CLASSES, {"STA_SUPPORTED_OPER_CLASSES"}},

    {NL80211_ATTR_HANDLE_DFS, {"HANDLE_DFS"}},

    {NL80211_ATTR_SUPPORT_5_MHZ, {"SUPPORT_5_MHZ"}},
    {NL80211_ATTR_SUPPORT_10_MHZ, {"SUPPORT_10_MHZ"}},

    {NL80211_ATTR_OPMODE_NOTIF, {"OPMODE_NOTIF"}},

    {NL80211_ATTR_VENDOR_ID, {"VENDOR_ID"}},
    {NL80211_ATTR_VENDOR_SUBCMD, {"VENDOR_SUBCMD"}},
    {NL80211_ATTR_VENDOR_DATA, {"VENDOR_DATA", DataType::Raw, AttributeMap{}, Flags::Verbose}},
    {NL80211_ATTR_VENDOR_EVENTS, {"VENDOR_EVENTS", DataType::Nested, AttributeMap{},
            Flags::Verbose}},

    {NL80211_ATTR_QOS_MAP, {"QOS_MAP"}},

    {NL80211_ATTR_MAC_HINT, {"MAC_HINT"}},
    {NL80211_ATTR_WIPHY_FREQ_HINT, {"WIPHY_FREQ_HINT"}},

    {NL80211_ATTR_MAX_AP_ASSOC_STA, {"MAX_AP_ASSOC_STA"}},

    {NL80211_ATTR_TDLS_PEER_CAPABILITY, {"TDLS_PEER_CAPABILITY"}},

    {NL80211_ATTR_SOCKET_OWNER, {"SOCKET_OWNER"}},

    {NL80211_ATTR_CSA_C_OFFSETS_TX, {"CSA_C_OFFSETS_TX"}},
    {NL80211_ATTR_MAX_CSA_COUNTERS, {"MAX_CSA_COUNTERS"}},

    {NL80211_ATTR_TDLS_INITIATOR, {"TDLS_INITIATOR"}},

    {NL80211_ATTR_USE_RRM, {"USE_RRM"}},

    {NL80211_ATTR_WIPHY_DYN_ACK, {"WIPHY_DYN_ACK"}},

    {NL80211_ATTR_TSID, {"TSID"}},
    {NL80211_ATTR_USER_PRIO, {"USER_PRIO"}},
    {NL80211_ATTR_ADMITTED_TIME, {"ADMITTED_TIME"}},

    {NL80211_ATTR_SMPS_MODE, {"SMPS_MODE"}},

    {NL80211_ATTR_OPER_CLASS, {"OPER_CLASS"}},

    {NL80211_ATTR_MAC_MASK, {"MAC_MASK"}},

    {NL80211_ATTR_WIPHY_SELF_MANAGED_REG, {"WIPHY_SELF_MANAGED_REG"}},

    {NL80211_ATTR_EXT_FEATURES, {"EXT_FEATURES"}},

    {NL80211_ATTR_SURVEY_RADIO_STATS, {"SURVEY_RADIO_STATS"}},

    {NL80211_ATTR_NETNS_FD, {"NETNS_FD"}},

    {NL80211_ATTR_SCHED_SCAN_DELAY, {"SCHED_SCAN_DELAY"}},

    {NL80211_ATTR_REG_INDOOR, {"REG_INDOOR"}},

    {NL80211_ATTR_MAX_NUM_SCHED_SCAN_PLANS, {"MAX_NUM_SCHED_SCAN_PLANS", DataType::Uint}},
    {NL80211_ATTR_MAX_SCAN_PLAN_INTERVAL, {"MAX_SCAN_PLAN_INTERVAL", DataType::Uint}},
    {NL80211_ATTR_MAX_SCAN_PLAN_ITERATIONS, {"MAX_SCAN_PLAN_ITERATIONS", DataType::Uint}},
    {NL80211_ATTR_SCHED_SCAN_PLANS, {"SCHED_SCAN_PLANS"}},

    {NL80211_ATTR_PBSS, {"PBSS"}},

    {NL80211_ATTR_BSS_SELECT, {"BSS_SELECT"}},

    {NL80211_ATTR_STA_SUPPORT_P2P_PS, {"STA_SUPPORT_P2P_PS"}},

    {NL80211_ATTR_PAD, {"PAD"}},

    {NL80211_ATTR_IFTYPE_EXT_CAPA, {"IFTYPE_EXT_CAPA"}},

    {NL80211_ATTR_MU_MIMO_GROUP_DATA, {"MU_MIMO_GROUP_DATA"}},
    {NL80211_ATTR_MU_MIMO_FOLLOW_MAC_ADDR, {"MU_MIMO_FOLLOW_MAC_ADDR"}},

    {NL80211_ATTR_SCAN_START_TIME_TSF, {"SCAN_START_TIME_TSF"}},
    {NL80211_ATTR_SCAN_START_TIME_TSF_BSSID, {"SCAN_START_TIME_TSF_BSSID"}},
    {NL80211_ATTR_MEASUREMENT_DURATION, {"MEASUREMENT_DURATION"}},
    {NL80211_ATTR_MEASUREMENT_DURATION_MANDATORY, {"MEASUREMENT_DURATION_MANDATORY"}},

    {NL80211_ATTR_MESH_PEER_AID, {"MESH_PEER_AID"}},

    {NL80211_ATTR_NAN_MASTER_PREF, {"NAN_MASTER_PREF"}},
    {NL80211_ATTR_BANDS, {"BANDS"}},
    {NL80211_ATTR_NAN_FUNC, {"NAN_FUNC"}},
    {NL80211_ATTR_NAN_MATCH, {"NAN_MATCH"}},

    {NL80211_ATTR_FILS_KEK, {"FILS_KEK"}},
    {NL80211_ATTR_FILS_NONCES, {"FILS_NONCES"}},

    {NL80211_ATTR_MULTICAST_TO_UNICAST_ENABLED, {"MULTICAST_TO_UNICAST_ENABLED"}},

    {NL80211_ATTR_BSSID, {"BSSID"}},

    {NL80211_ATTR_SCHED_SCAN_RELATIVE_RSSI, {"SCHED_SCAN_RELATIVE_RSSI"}},
    {NL80211_ATTR_SCHED_SCAN_RSSI_ADJUST, {"SCHED_SCAN_RSSI_ADJUST"}},

    {NL80211_ATTR_TIMEOUT_REASON, {"TIMEOUT_REASON"}},

    {NL80211_ATTR_FILS_ERP_USERNAME, {"FILS_ERP_USERNAME"}},
    {NL80211_ATTR_FILS_ERP_REALM, {"FILS_ERP_REALM"}},
    {NL80211_ATTR_FILS_ERP_NEXT_SEQ_NUM, {"FILS_ERP_NEXT_SEQ_NUM"}},
    {NL80211_ATTR_FILS_ERP_RRK, {"FILS_ERP_RRK"}},
    {NL80211_ATTR_FILS_CACHE_ID, {"FILS_CACHE_ID"}},

    {NL80211_ATTR_PMK, {"PMK"}},

    {NL80211_ATTR_SCHED_SCAN_MULTI, {"SCHED_SCAN_MULTI"}},
    {NL80211_ATTR_SCHED_SCAN_MAX_REQS, {"SCHED_SCAN_MAX_REQS"}},

    {NL80211_ATTR_WANT_1X_4WAY_HS, {"WANT_1X_4WAY_HS"}},
    {NL80211_ATTR_PMKR0_NAME, {"PMKR0_NAME"}},
    {NL80211_ATTR_PORT_AUTHORIZED, {"PORT_AUTHORIZED"}},

    {NL80211_ATTR_EXTERNAL_AUTH_ACTION, {"EXTERNAL_AUTH_ACTION"}},
    {NL80211_ATTR_EXTERNAL_AUTH_SUPPORT, {"EXTERNAL_AUTH_SUPPORT"}},

    {NL80211_ATTR_NSS, {"NSS"}},
    {NL80211_ATTR_ACK_SIGNAL, {"ACK_SIGNAL"}},

    {NL80211_ATTR_CONTROL_PORT_OVER_NL80211, {"CONTROL_PORT_OVER_NL80211"}},

    {NL80211_ATTR_TXQ_STATS, {"TXQ_STATS"}},
    {NL80211_ATTR_TXQ_LIMIT, {"TXQ_LIMIT"}},
    {NL80211_ATTR_TXQ_MEMORY_LIMIT, {"TXQ_MEMORY_LIMIT"}},
    {NL80211_ATTR_TXQ_QUANTUM, {"TXQ_QUANTUM"}},

    {NL80211_ATTR_HE_CAPABILITY, {"HE_CAPABILITY"}},

    {NL80211_ATTR_FTM_RESPONDER, {"FTM_RESPONDER"}},

    {NL80211_ATTR_FTM_RESPONDER_STATS, {"FTM_RESPONDER_STATS"}},

    {NL80211_ATTR_TIMEOUT, {"TIMEOUT"}},

    {NL80211_ATTR_PEER_MEASUREMENTS, {"PEER_MEASUREMENTS"}},

    {NL80211_ATTR_AIRTIME_WEIGHT, {"AIRTIME_WEIGHT"}},
    {NL80211_ATTR_STA_TX_POWER_SETTING, {"STA_TX_POWER_SETTING"}},
    {NL80211_ATTR_STA_TX_POWER, {"STA_TX_POWER"}},

    {NL80211_ATTR_SAE_PASSWORD, {"SAE_PASSWORD"}},

    {NL80211_ATTR_TWT_RESPONDER, {"TWT_RESPONDER"}},

    {NL80211_ATTR_HE_OBSS_PD, {"HE_OBSS_PD"}},

    {NL80211_ATTR_WIPHY_EDMG_CHANNELS, {"WIPHY_EDMG_CHANNELS"}},
    {NL80211_ATTR_WIPHY_EDMG_BW_CONFIG, {"WIPHY_EDMG_BW_CONFIG"}},

    {NL80211_ATTR_VLAN_ID, {"VLAN_ID"}},

    {NL80211_ATTR_HE_BSS_COLOR, {"HE_BSS_COLOR"}},

    {NL80211_ATTR_IFTYPE_AKM_SUITES, {"IFTYPE_AKM_SUITES"}},

    {NL80211_ATTR_TID_CONFIG, {"TID_CONFIG"}},

    {NL80211_ATTR_CONTROL_PORT_NO_PREAUTH, {"CONTROL_PORT_NO_PREAUTH"}},

    {NL80211_ATTR_PMK_LIFETIME, {"PMK_LIFETIME"}},
    {NL80211_ATTR_PMK_REAUTH_THRESHOLD, {"PMK_REAUTH_THRESHOLD"}},

    {NL80211_ATTR_RECEIVE_MULTICAST, {"RECEIVE_MULTICAST"}},
    {NL80211_ATTR_WIPHY_FREQ_OFFSET, {"WIPHY_FREQ_OFFSET"}},
    {NL80211_ATTR_CENTER_FREQ1_OFFSET, {"CENTER_FREQ1_OFFSET"}},
    {NL80211_ATTR_SCAN_FREQ_KHZ, {"SCAN_FREQ_KHZ"}},

    {NL80211_ATTR_HE_6GHZ_CAPABILITY, {"HE_6GHZ_CAPABILITY"}},

    {NL80211_ATTR_FILS_DISCOVERY, {"FILS_DISCOVERY"}},

    {NL80211_ATTR_UNSOL_BCAST_PROBE_RESP, {"UNSOL_BCAST_PROBE_RESP"}},

    {NL80211_ATTR_S1G_CAPABILITY, {"S1G_CAPABILITY"}},
    {NL80211_ATTR_S1G_CAPABILITY_MASK, {"S1G_CAPABILITY_MASK"}},
}) {}
// clang-format on

static void informationElementsToStream(std::stringstream& ss, const Buffer<nlattr> attr) {
    struct IEHeader {
        uint8_t elementId;
        uint8_t length;
    } __attribute__((packed));
    static_assert(sizeof(IEHeader) == 2);

    const auto alldata = attr.data<uint8_t>();
    const auto bytes = alldata.getRaw();

    ss << '{';

    if constexpr (kCompactIE) {
        ss << "len=" << bytes.len() << ", ";
        ss << "crc=" << std::hex << std::setw(4) << crc16(alldata) << std::dec << ", ";
    }

    bool first = true;
    auto printComma = [&first, &ss]() {
        // put separator at every but first entry
        if (!first) ss << ", ";
        first = false;
    };

    for (size_t offset = 0; offset < bytes.len();) {
        const auto ptr = bytes.ptr() + offset;
        const auto remainingLen = bytes.len() - offset;

        // can we fit one more header?
        if (sizeof(IEHeader) > remainingLen) break;
        IEHeader ieHeader;
        memcpy(&ieHeader, ptr, sizeof(IEHeader));
        if (sizeof(IEHeader) + ieHeader.length > remainingLen) {
            printComma();
            ss << "ERR";
            break;
        }
        offset += sizeof(IEHeader) + ieHeader.length;

        const Buffer<uint8_t> data(ptr + sizeof(IEHeader), ieHeader.length);

        if (ieHeader.elementId == WLAN_EID_SSID) {
            printComma();

            const auto str = data.getRaw();
            const std::string ssid(reinterpret_cast<const char*>(str.ptr()), str.len());
            ss << "SSID=\"" << printableOnly(ssid) << '"';

            continue;
        }

        if constexpr (kCompactIE) continue;

        // print entry ID:LENGTH/CRC16
        printComma();
        ss << (int)ieHeader.elementId << ':' << (int)ieHeader.length << '/';
        ss << std::hex << std::setw(4) << crc16(data) << std::dec;
    }
    ss << '}';
}

static void nl80211_pattern_supportToStream(std::stringstream& ss, const Buffer<nlattr> attr) {
    const auto& [ok, data] = attr.data<nl80211_pattern_support>().getFirst();
    if (!ok) {
        ss << "invalid structure";
        return;
    }
    ss << '{'                          //
       << data.max_patterns << ','     //
       << data.min_pattern_len << ','  //
       << data.max_pattern_len << ','  //
       << data.max_pkt_offset << '}';
}

}  // namespace android::nl::protocols::generic::families
