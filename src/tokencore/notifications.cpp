#include "tokencore/notifications.h"

#include "tokencore/log.h"
#include "tokencore/tokencore.h"
#include "tokencore/rules.h"
#include "tokencore/utilsbitcoin.h"
#include "tokencore/version.h"

#include "main.h"
#include "util.h"
#include "guiinterface.h"

#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

#include <stdint.h>
#include <string>
#include <vector>

namespace mastercore
{

//! Vector of currently active Token alerts
std::vector<AlertData> currentTokenAlerts;

/**
 * Deletes previously broadcast alerts from sender from the alerts vector
 *
 * Note cannot be used to delete alerts from other addresses, nor to delete system generated feature alerts
 */
void DeleteAlerts(const std::string& sender)
{
    for (std::vector<AlertData>::iterator it = currentTokenAlerts.begin(); it != currentTokenAlerts.end(); ) {
        AlertData alert = *it;
        if (sender == alert.alert_sender) {
            PrintToLog("Removing deleted alert (from:%s type:%d expiry:%d message:%s)\n", alert.alert_sender,
                alert.alert_type, alert.alert_expiry, alert.alert_message);
            it = currentTokenAlerts.erase(it);
            uiInterface.TokenStateChanged();
        } else {
            it++;
        }
    }
}

/**
 * Removes all active alerts.
 *
 * A signal is fired to notify the UI about the status update.
 */
void ClearAlerts()
{
    currentTokenAlerts.clear();
    uiInterface.TokenStateChanged();
}

/**
 * Adds a new alert to the alerts vector
 *
 */
void AddAlert(const std::string& sender, uint16_t alertType, uint32_t alertExpiry, const std::string& alertMessage)
{
    AlertData newAlert;
    newAlert.alert_sender = sender;
    newAlert.alert_type = alertType;
    newAlert.alert_expiry = alertExpiry;
    newAlert.alert_message = alertMessage;

    // very basic sanity checks for broadcast alerts to catch malformed packets
    if (sender != "tokencore" && (alertType < ALERT_BLOCK_EXPIRY || alertType > ALERT_CLIENT_VERSION_EXPIRY)) {
        PrintToLog("New alert REJECTED (alert type not recognized): %s, %d, %d, %s\n", sender, alertType, alertExpiry, alertMessage);
        return;
    }

    currentTokenAlerts.push_back(newAlert);
    PrintToLog("New alert added: %s, %d, %d, %s\n", sender, alertType, alertExpiry, alertMessage);
}

/**
 * Determines whether the sender is an authorized source for Token Core alerts.
 *
 * The option "-tokenalertallowsender=source" can be used to whitelist additional sources,
 * and the option "-tokenalertignoresender=source" can be used to ignore a source.
 *
 * To consider any alert as authorized, "-tokenalertallowsender=any" can be used. This
 * should only be done for testing purposes!
 */
bool CheckAlertAuthorization(const std::string& sender)
{
    std::set<std::string> whitelisted;

    // Mainnet
    whitelisted.insert("17xr7sbehYY4YSZX9yuJe6gK9rrdRrZx26"); // Craig   <craig@token.foundation>
    whitelisted.insert("16oDZYCspsczfgKXVj3xyvsxH21NpEj94F"); // Adam    <adam@token.foundation>
    whitelisted.insert("1883ZMsRJfzKNozUBJBTCxQ7EaiNioNDWz"); // Zathras <zathras@token.foundation>
    whitelisted.insert("1HHv91gRxqBzQ3gydMob3LU8hqXcWoLfvd"); // dexX7   <dexx@bitwatch.co>
    whitelisted.insert("34kwkVRSvFVEoUwcQSgpQ4ZUasuZ54DJLD"); // multisig (signatories are Craig, Adam, Zathras, dexX7, JR)

    // Testnet / Regtest
    // use -tokenalertallowsender for testing

    // Add manually whitelisted sources
    if (mapArgs.count("-tokenalertallowsender")) {
        const std::vector<std::string>& sources = mapMultiArgs["-tokenalertallowsender"];

        for (std::vector<std::string>::const_iterator it = sources.begin(); it != sources.end(); ++it) {
            whitelisted.insert(*it);
        }
    }

    // Remove manually ignored sources
    if (mapArgs.count("-tokenalertignoresender")) {
        const std::vector<std::string>& sources = mapMultiArgs["-tokenalertignoresender"];

        for (std::vector<std::string>::const_iterator it = sources.begin(); it != sources.end(); ++it) {
            whitelisted.erase(*it);
        }
    }

    bool fAuthorized = (whitelisted.count(sender) ||
                        whitelisted.count("any"));

    return fAuthorized;
}

/**
 * Alerts including meta data.
 */
std::vector<AlertData> GetTokenCoreAlerts()
{
    return currentTokenAlerts;
}

/**
 * Human readable alert messages.
 */
std::vector<std::string> GetTokenCoreAlertMessages()
{
    std::vector<std::string> vstr;
    for (std::vector<AlertData>::iterator it = currentTokenAlerts.begin(); it != currentTokenAlerts.end(); it++) {
        vstr.push_back((*it).alert_message);
    }
    return vstr;
}

/**
 * Expires any alerts that need expiring.
 */
bool CheckExpiredAlerts(unsigned int curBlock, uint64_t curTime)
{
    for (std::vector<AlertData>::iterator it = currentTokenAlerts.begin(); it != currentTokenAlerts.end(); ) {
        AlertData alert = *it;
        switch (alert.alert_type) {
            case ALERT_BLOCK_EXPIRY:
                if (curBlock >= alert.alert_expiry) {
                    PrintToLog("Expiring alert (from %s: type:%d expiry:%d message:%s)\n", alert.alert_sender,
                        alert.alert_type, alert.alert_expiry, alert.alert_message);
                    it = currentTokenAlerts.erase(it);
                    uiInterface.TokenStateChanged();
                } else {
                    it++;
                }
            break;
            case ALERT_BLOCKTIME_EXPIRY:
                if (curTime > alert.alert_expiry) {
                    PrintToLog("Expiring alert (from %s: type:%d expiry:%d message:%s)\n", alert.alert_sender,
                        alert.alert_type, alert.alert_expiry, alert.alert_message);
                    it = currentTokenAlerts.erase(it);
                    uiInterface.TokenStateChanged();
                } else {
                    it++;
                }
            break;
            case ALERT_CLIENT_VERSION_EXPIRY:
                if (TOKENCORE_VERSION > alert.alert_expiry) {
                    PrintToLog("Expiring alert (form: %s type:%d expiry:%d message:%s)\n", alert.alert_sender,
                        alert.alert_type, alert.alert_expiry, alert.alert_message);
                    it = currentTokenAlerts.erase(it);
                    uiInterface.TokenStateChanged();
                } else {
                    it++;
                }
            break;
            default: // unrecognized alert type
                    PrintToLog("Removing invalid alert (from:%s type:%d expiry:%d message:%s)\n", alert.alert_sender,
                        alert.alert_type, alert.alert_expiry, alert.alert_message);
                    it = currentTokenAlerts.erase(it);
                    uiInterface.TokenStateChanged();
            break;
        }
    }
    return true;
}

} // namespace mastercore
