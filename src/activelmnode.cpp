// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activelmnode.h"
#include "lmnode.h"
#include "lmnode-sync.h"
#include "lmnodeman.h"
#include "protocol.h"

extern CWallet *pwalletMain;

// Keep track of the active LMNode
CActiveLMNode activeLMNode;

void CActiveLMNode::ManageState() {
    LogPrint("lmnode", "CActiveLMNode::ManageState -- Start\n");
    if (!fZNode) {
        LogPrint("lmnode", "CActiveLMNode::ManageState -- Not a lmnode, returning\n");
        return;
    }

    if (Params().NetworkIDString() != CBaseChainParams::REGTEST && !lmnodeSync.IsBlockchainSynced()) {
        nState = ACTIVE_LMNODE_SYNC_IN_PROCESS;
        LogPrintf("CActiveLMNode::ManageState -- %s: %s\n", GetStateString(), GetStatus());
        return;
    }

    if (nState == ACTIVE_LMNODE_SYNC_IN_PROCESS) {
        nState = ACTIVE_LMNODE_INITIAL;
    }

    LogPrint("lmnode", "CActiveLMNode::ManageState -- status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);

    if (eType == LMNODE_UNKNOWN) {
        ManageStateInitial();
    }

    if (eType == LMNODE_REMOTE) {
        ManageStateRemote();
    } else if (eType == LMNODE_LOCAL) {
        // Try Remote Start first so the started local lmnode can be restarted without recreate lmnode broadcast.
        ManageStateRemote();
        if (nState != ACTIVE_LMNODE_STARTED)
            ManageStateLocal();
    }

    SendLMNodePing();
}

std::string CActiveLMNode::GetStateString() const {
    switch (nState) {
        case ACTIVE_LMNODE_INITIAL:
            return "INITIAL";
        case ACTIVE_LMNODE_SYNC_IN_PROCESS:
            return "SYNC_IN_PROCESS";
        case ACTIVE_LMNODE_INPUT_TOO_NEW:
            return "INPUT_TOO_NEW";
        case ACTIVE_LMNODE_NOT_CAPABLE:
            return "NOT_CAPABLE";
        case ACTIVE_LMNODE_STARTED:
            return "STARTED";
        default:
            return "UNKNOWN";
    }
}

std::string CActiveLMNode::GetStatus() const {
    switch (nState) {
        case ACTIVE_LMNODE_INITIAL:
            return "Node just started, not yet activated";
        case ACTIVE_LMNODE_SYNC_IN_PROCESS:
            return "Sync in progress. Must wait until sync is complete to start LMNode";
        case ACTIVE_LMNODE_INPUT_TOO_NEW:
            return strprintf("LMNode input must have at least %d confirmations",
                             Params().GetConsensus().nLMNodeMinimumConfirmations);
        case ACTIVE_LMNODE_NOT_CAPABLE:
            return "Not capable lmnode: " + strNotCapableReason;
        case ACTIVE_LMNODE_STARTED:
            return "LMNode successfully started";
        default:
            return "Unknown";
    }
}

std::string CActiveLMNode::GetTypeString() const {
    std::string strType;
    switch (eType) {
        case LMNODE_UNKNOWN:
            strType = "UNKNOWN";
            break;
        case LMNODE_REMOTE:
            strType = "REMOTE";
            break;
        case LMNODE_LOCAL:
            strType = "LOCAL";
            break;
        default:
            strType = "UNKNOWN";
            break;
    }
    return strType;
}

bool CActiveLMNode::SendLMNodePing() {
    if (!fPingerEnabled) {
        LogPrint("lmnode",
                 "CActiveLMNode::SendLMNodePing -- %s: lmnode ping service is disabled, skipping...\n",
                 GetStateString());
        return false;
    }

    if (!mnodeman.Has(vin)) {
        strNotCapableReason = "LMNode not in lmnode list";
        nState = ACTIVE_LMNODE_NOT_CAPABLE;
        LogPrintf("CActiveLMNode::SendLMNodePing -- %s: %s\n", GetStateString(), strNotCapableReason);
        return false;
    }

    CLMNodePing mnp(vin);
    if (!mnp.Sign(keyLMNode, pubKeyLMNode)) {
        LogPrintf("CActiveLMNode::SendLMNodePing -- ERROR: Couldn't sign LMNode Ping\n");
        return false;
    }

    // Update lastPing for our lmnode in LMNode list
    if (mnodeman.IsLMNodePingedWithin(vin, LMNODE_MIN_MNP_SECONDS, mnp.sigTime)) {
        LogPrintf("CActiveLMNode::SendLMNodePing -- Too early to send LMNode Ping\n");
        return false;
    }

    mnodeman.SetLMNodeLastPing(vin, mnp);

    LogPrintf("CActiveLMNode::SendLMNodePing -- Relaying ping, collateral=%s\n", vin.ToString());
    mnp.Relay();

    return true;
}

void CActiveLMNode::ManageStateInitial() {
    LogPrint("lmnode", "CActiveLMNode::ManageStateInitial -- status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);

    // Check that our local network configuration is correct
    if (!fListen) {
        // listen option is probably overwritten by smth else, no good
        nState = ACTIVE_LMNODE_NOT_CAPABLE;
        strNotCapableReason = "LMNode must accept connections from outside. Make sure listen configuration option is not overwritten by some another parameter.";
        LogPrintf("CActiveLMNode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    bool fFoundLocal = false;
    {
        LOCK(cs_vNodes);

        // First try to find whatever local address is specified by externalip option
        fFoundLocal = GetLocal(service) && CLMNode::IsValidNetAddr(service);
        if (!fFoundLocal) {
            // nothing and no live connections, can't do anything for now
            if (vNodes.empty()) {
                nState = ACTIVE_LMNODE_NOT_CAPABLE;
                strNotCapableReason = "Can't detect valid external address. Will retry when there are some connections available.";
                LogPrintf("CActiveLMNode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
                return;
            }
            // We have some peers, let's try to find our local address from one of them
            BOOST_FOREACH(CNode * pnode, vNodes)
            {
                if (pnode->fSuccessfullyConnected && pnode->addr.IsIPv4()) {
                    fFoundLocal = GetLocal(service, &pnode->addr) && CLMNode::IsValidNetAddr(service);
                    if (fFoundLocal) break;
                }
            }
        }
    }

    if (!fFoundLocal) {
        nState = ACTIVE_LMNODE_NOT_CAPABLE;
        strNotCapableReason = "Can't detect valid external address. Please consider using the externalip configuration option if problem persists. Make sure to use IPv4 address only.";
        LogPrintf("CActiveLMNode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (service.GetPort() != mainnetDefaultPort) {
            nState = ACTIVE_LMNODE_NOT_CAPABLE;
            strNotCapableReason = strprintf("Invalid port: %u - only %d is supported on mainnet.", service.GetPort(),
                                            mainnetDefaultPort);
            LogPrintf("CActiveLMNode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
    } else if (service.GetPort() == mainnetDefaultPort) {
        nState = ACTIVE_LMNODE_NOT_CAPABLE;
        strNotCapableReason = strprintf("Invalid port: %u - %d is only supported on mainnet.", service.GetPort(),
                                        mainnetDefaultPort);
        LogPrintf("CActiveLMNode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    LogPrintf("CActiveLMNode::ManageStateInitial -- Checking inbound connection to '%s'\n", service.ToString());
    //TODO
    if (!ConnectNode(CAddress(service, NODE_NETWORK), NULL, false, true)) {
        nState = ACTIVE_LMNODE_NOT_CAPABLE;
        strNotCapableReason = "Could not connect to " + service.ToString();
        LogPrintf("CActiveLMNode::ManageStateInitial -- %s: %s\n", GetStateString(), strNotCapableReason);
        return;
    }

    // Default to REMOTE
    eType = LMNODE_REMOTE;

    // Check if wallet funds are available
    if (!pwalletMain) {
        LogPrintf("CActiveLMNode::ManageStateInitial -- %s: Wallet not available\n", GetStateString());
        return;
    }

    if (pwalletMain->IsLocked()) {
        LogPrintf("CActiveLMNode::ManageStateInitial -- %s: Wallet is locked\n", GetStateString());
        return;
    }

    if (pwalletMain->GetBalance() < LMNODE_COIN_REQUIRED * COIN) {
        LogPrintf("CActiveLMNode::ManageStateInitial -- %s: Wallet balance is < 1000 HPP\n", GetStateString());
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    // If collateral is found switch to LOCAL mode
    if (pwalletMain->GetLMNodeVinAndKeys(vin, pubKeyCollateral, keyCollateral)) {
        eType = LMNODE_LOCAL;
    }

    LogPrint("lmnode", "CActiveLMNode::ManageStateInitial -- End status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);
}

void CActiveLMNode::ManageStateRemote() {
    LogPrint("lmnode",
             "CActiveLMNode::ManageStateRemote -- Start status = %s, type = %s, pinger enabled = %d, pubKeyLMNode.GetID() = %s\n",
             GetStatus(), fPingerEnabled, GetTypeString(), pubKeyLMNode.GetID().ToString());

    mnodeman.CheckLMNode(pubKeyLMNode);
    lmnode_info_t infoMn = mnodeman.GetLMNodeInfo(pubKeyLMNode);
    if (infoMn.fInfoValid) {
        if (infoMn.nProtocolVersion != PROTOCOL_VERSION) {
            nState = ACTIVE_LMNODE_NOT_CAPABLE;
            strNotCapableReason = "Invalid protocol version";
            LogPrintf("CActiveLMNode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (service != infoMn.addr) {
            nState = ACTIVE_LMNODE_NOT_CAPABLE;
            // LogPrintf("service: %s\n", service.ToString());
            // LogPrintf("infoMn.addr: %s\n", infoMn.addr.ToString());
            strNotCapableReason = "Broadcasted IP doesn't match our external address. Make sure you issued a new broadcast if IP of this lmnode changed recently.";
            LogPrintf("CActiveLMNode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (!CLMNode::IsValidStateForAutoStart(infoMn.nActiveState)) {
            nState = ACTIVE_LMNODE_NOT_CAPABLE;
            strNotCapableReason = strprintf("LMNode in %s state", CLMNode::StateToString(infoMn.nActiveState));
            LogPrintf("CActiveLMNode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }
        if (nState != ACTIVE_LMNODE_STARTED) {
            LogPrintf("CActiveLMNode::ManageStateRemote -- STARTED!\n");
            vin = infoMn.vin;
            service = infoMn.addr;
            fPingerEnabled = true;
            nState = ACTIVE_LMNODE_STARTED;
        }
    } else {
        nState = ACTIVE_LMNODE_NOT_CAPABLE;
        strNotCapableReason = "LMNode not in lmnode list";
        LogPrintf("CActiveLMNode::ManageStateRemote -- %s: %s\n", GetStateString(), strNotCapableReason);
    }
}

void CActiveLMNode::ManageStateLocal() {
    LogPrint("lmnode", "CActiveLMNode::ManageStateLocal -- status = %s, type = %s, pinger enabled = %d\n",
             GetStatus(), GetTypeString(), fPingerEnabled);
    if (nState == ACTIVE_LMNODE_STARTED) {
        return;
    }

    // Choose coins to use
    CPubKey pubKeyCollateral;
    CKey keyCollateral;

    if (pwalletMain->GetLMNodeVinAndKeys(vin, pubKeyCollateral, keyCollateral)) {
        int nInputAge = GetInputAge(vin);
        if (nInputAge < Params().GetConsensus().nLMNodeMinimumConfirmations) {
            nState = ACTIVE_LMNODE_INPUT_TOO_NEW;
            strNotCapableReason = strprintf(_("%s - %d confirmations"), GetStatus(), nInputAge);
            LogPrintf("CActiveLMNode::ManageStateLocal -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }

        {
            LOCK(pwalletMain->cs_wallet);
            pwalletMain->LockCoin(vin.prevout);
        }

        CLMNodeBroadcast mnb;
        std::string strError;
        if (!CLMNodeBroadcast::Create(vin, service, keyCollateral, pubKeyCollateral, keyLMNode,
                                     pubKeyLMNode, strError, mnb)) {
            nState = ACTIVE_LMNODE_NOT_CAPABLE;
            strNotCapableReason = "Error creating mastenode broadcast: " + strError;
            LogPrintf("CActiveLMNode::ManageStateLocal -- %s: %s\n", GetStateString(), strNotCapableReason);
            return;
        }

        fPingerEnabled = true;
        nState = ACTIVE_LMNODE_STARTED;

        //update to lmnode list
        LogPrintf("CActiveLMNode::ManageStateLocal -- Update LMNode List\n");
        mnodeman.UpdateLMNodeList(mnb);
        mnodeman.NotifyLMNodeUpdates();

        //send to all peers
        LogPrintf("CActiveLMNode::ManageStateLocal -- Relay broadcast, vin=%s\n", vin.ToString());
        mnb.RelayZNode();
    }
}
