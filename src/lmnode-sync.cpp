// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2016-2017 The Zcoin developers
// Copyright (c) 2017-2018 The Hppcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activelmnode.h"
#include "checkpoints.h"
#include "main.h"
#include "lmnode.h"
#include "lmnode-payments.h"
#include "lmnode-sync.h"
#include "lmnodeman.h"
#include "netfulfilledman.h"
#include "spork.h"
#include "util.h"

class CLMNodeSync;

CLMNodeSync lmnodeSync;

bool CLMNodeSync::CheckNodeHeight(CNode *pnode, bool fDisconnectStuckNodes) {
    CNodeStateStats stats;
    if (!GetNodeStateStats(pnode->id, stats) || stats.nCommonHeight == -1 || stats.nSyncHeight == -1) return false; // not enough info about this peer

    // Check blocks and headers, allow a small error margin of 1 block
    if (pCurrentBlockIndex->nHeight - 1 > stats.nCommonHeight) {
        // This peer probably stuck, don't sync any additional data from it
        if (fDisconnectStuckNodes) {
            // Disconnect to free this connection slot for another peer.
            pnode->fDisconnect = true;
            LogPrintf("CLMNodeSync::CheckNodeHeight -- disconnecting from stuck peer, nHeight=%d, nCommonHeight=%d, peer=%d\n",
                      pCurrentBlockIndex->nHeight, stats.nCommonHeight, pnode->id);
        } else {
            LogPrintf("CLMNodeSync::CheckNodeHeight -- skipping stuck peer, nHeight=%d, nCommonHeight=%d, peer=%d\n",
                      pCurrentBlockIndex->nHeight, stats.nCommonHeight, pnode->id);
        }
        return false;
    } else if (pCurrentBlockIndex->nHeight < stats.nSyncHeight - 1) {
        // This peer announced more headers than we have blocks currently
        LogPrintf("CLMNodeSync::CheckNodeHeight -- skipping peer, who announced more headers than we have blocks currently, nHeight=%d, nSyncHeight=%d, peer=%d\n",
                  pCurrentBlockIndex->nHeight, stats.nSyncHeight, pnode->id);
        return false;
    }

    return true;
}

bool CLMNodeSync::IsBlockchainSynced(bool fBlockAccepted) {
    static bool fBlockchainSynced = false;
    static int64_t nTimeLastProcess = GetTime();
    static int nSkipped = 0;
    static bool fFirstBlockAccepted = false;

    // if the last call to this function was more than 60 minutes ago (client was in sleep mode) reset the sync process
    if (GetTime() - nTimeLastProcess > 60 * 60) {
        LogPrintf("CLMNodeSync::IsBlockchainSynced time-check fBlockchainSynced=%s\n", fBlockchainSynced);
        Reset();
        fBlockchainSynced = false;
    }

    if (!pCurrentBlockIndex || !pindexBestHeader || fImporting || fReindex) return false;

    if (fBlockAccepted) {
        // this should be only triggered while we are still syncing
        if (!IsSynced()) {
            // we are trying to download smth, reset blockchain sync status
            fFirstBlockAccepted = true;
            fBlockchainSynced = false;
            nTimeLastProcess = GetTime();
            return false;
        }
    } else {
        // skip if we already checked less than 1 tick ago
        if (GetTime() - nTimeLastProcess < LMNODE_SYNC_TICK_SECONDS) {
            nSkipped++;
            return fBlockchainSynced;
        }
    }

    LogPrint("lmnode-sync", "CLMNodeSync::IsBlockchainSynced -- state before check: %ssynced, skipped %d times\n", fBlockchainSynced ? "" : "not ", nSkipped);

    nTimeLastProcess = GetTime();
    nSkipped = 0;

    if (fBlockchainSynced){
        return true;
    }

    if (fCheckpointsEnabled && pCurrentBlockIndex->nHeight < Checkpoints::GetTotalBlocksEstimate(Params().Checkpoints())) {
        return false;
    }

    std::vector < CNode * > vNodesCopy = CopyNodeVector();
    // We have enough peers and assume most of them are synced
    if (vNodesCopy.size() >= LMNODE_SYNC_ENOUGH_PEERS) {
        // Check to see how many of our peers are (almost) at the same height as we are
        int nNodesAtSameHeight = 0;
        BOOST_FOREACH(CNode * pnode, vNodesCopy)
        {
            // Make sure this peer is presumably at the same height
            if (!CheckNodeHeight(pnode)) {
                continue;
            }
            nNodesAtSameHeight++;
            // if we have decent number of such peers, most likely we are synced now
            if (nNodesAtSameHeight >= LMNODE_SYNC_ENOUGH_PEERS) {
                LogPrintf("CLMNodeSync::IsBlockchainSynced -- found enough peers on the same height as we are, done\n");
                fBlockchainSynced = true;
                ReleaseNodeVector(vNodesCopy);
                return true;
            }
        }
    }
    ReleaseNodeVector(vNodesCopy);

    // wait for at least one new block to be accepted
    if (!fFirstBlockAccepted) return false;

    // same as !IsInitialBlockDownload() but no cs_main needed here
    int64_t nMaxBlockTime = std::max(pCurrentBlockIndex->GetBlockTime(), pindexBestHeader->GetBlockTime());
    fBlockchainSynced = pindexBestHeader->nHeight - pCurrentBlockIndex->nHeight < 24 * 6 &&
                        GetTime() - nMaxBlockTime < Params().MaxTipAge();
    return fBlockchainSynced;
}

void CLMNodeSync::Fail() {
    nTimeLastFailure = GetTime();
    nRequestedLMNodeAssets = LMNODE_SYNC_FAILED;
}

void CLMNodeSync::Reset() {
    nRequestedLMNodeAssets = LMNODE_SYNC_INITIAL;
    nRequestedLMNodeAttempt = 0;
    nTimeAssetSyncStarted = GetTime();
    nTimeLastLMNodeList = GetTime();
    nTimeLastPaymentVote = GetTime();
    nTimeLastGovernanceItem = GetTime();
    nTimeLastFailure = 0;
    nCountFailures = 0;
}

std::string CLMNodeSync::GetAssetName() {
    switch (nRequestedLMNodeAssets) {
        case (LMNODE_SYNC_INITIAL):
            return "LMNODE_SYNC_INITIAL";
        case (LMNODE_SYNC_SPORKS):
            return "LMNODE_SYNC_SPORKS";
        case (LMNODE_SYNC_LIST):
            return "LMNODE_SYNC_LIST";
        case (LMNODE_SYNC_MNW):
            return "LMNODE_SYNC_MNW";
        case (LMNODE_SYNC_FAILED):
            return "LMNODE_SYNC_FAILED";
        case LMNODE_SYNC_FINISHED:
            return "LMNODE_SYNC_FINISHED";
        default:
            return "UNKNOWN";
    }
}

void CLMNodeSync::SwitchToNextAsset() {
    switch (nRequestedLMNodeAssets) {
        case (LMNODE_SYNC_FAILED):
            throw std::runtime_error("Can't switch to next asset from failed, should use Reset() first!");
            break;
        case (LMNODE_SYNC_INITIAL):
            ClearFulfilledRequests();
            nRequestedLMNodeAssets = LMNODE_SYNC_SPORKS;
            LogPrintf("CLMNodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
            break;
        case (LMNODE_SYNC_SPORKS):
            nTimeLastLMNodeList = GetTime();
            nRequestedLMNodeAssets = LMNODE_SYNC_LIST;
            LogPrintf("CLMNodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
            break;
        case (LMNODE_SYNC_LIST):
            nTimeLastPaymentVote = GetTime();
            nRequestedLMNodeAssets = LMNODE_SYNC_MNW;
            LogPrintf("CLMNodeSync::SwitchToNextAsset -- Starting %s\n", GetAssetName());
            break;

        case (LMNODE_SYNC_MNW):
            nTimeLastGovernanceItem = GetTime();
            LogPrintf("CLMNodeSync::SwitchToNextAsset -- Sync has finished\n");
            nRequestedLMNodeAssets = LMNODE_SYNC_FINISHED;
            break;
    }
    nRequestedLMNodeAttempt = 0;
    nTimeAssetSyncStarted = GetTime();
}

std::string CLMNodeSync::GetSyncStatus() {
    switch (lmnodeSync.nRequestedLMNodeAssets) {
        case LMNODE_SYNC_INITIAL:
            return _("Synchronization pending...");
        case LMNODE_SYNC_SPORKS:
            return _("Synchronizing sporks...");
        case LMNODE_SYNC_LIST:
            return _("Synchronizing lmnodes...");
        case LMNODE_SYNC_MNW:
            return _("Synchronizing lmnode payments...");
        case LMNODE_SYNC_FAILED:
            return _("Synchronization failed");
        case LMNODE_SYNC_FINISHED:
            return _("Synchronization finished");
        default:
            return "";
    }
}

void CLMNodeSync::ProcessMessage(CNode *pfrom, std::string &strCommand, CDataStream &vRecv) {
    if (strCommand == NetMsgType::SYNCSTATUSCOUNT) { //Sync status count

        //do not care about stats if sync process finished or failed
        if (IsSynced() || IsFailed()) return;

        int nItemID;
        int nCount;
        vRecv >> nItemID >> nCount;

        LogPrintf("SYNCSTATUSCOUNT -- got inventory count: nItemID=%d  nCount=%d  peer=%d\n", nItemID, nCount, pfrom->id);
    }
}

void CLMNodeSync::ClearFulfilledRequests() {
    TRY_LOCK(cs_vNodes, lockRecv);
    if (!lockRecv) return;

    BOOST_FOREACH(CNode * pnode, vNodes)
    {
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "spork-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "lmnode-list-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "lmnode-payment-sync");
        netfulfilledman.RemoveFulfilledRequest(pnode->addr, "full-sync");
    }
}

void CLMNodeSync::ProcessTick() {
    static int nTick = 0;
    if (nTick++ % LMNODE_SYNC_TICK_SECONDS != 0) return;
    if (!pCurrentBlockIndex) return;

    //the actual count of lmnodes we have currently
    int nMnCount = mnodeman.CountLMNodes();

    LogPrint("ProcessTick", "CLMNodeSync::ProcessTick -- nTick %d nMnCount %d\n", nTick, nMnCount);

    // INITIAL SYNC SETUP / LOG REPORTING
    double nSyncProgress = double(nRequestedLMNodeAttempt + (nRequestedLMNodeAssets - 1) * 8) / (8 * 4);
    LogPrint("ProcessTick", "CLMNodeSync::ProcessTick -- nTick %d nRequestedLMNodeAssets %d nRequestedLMNodeAttempt %d nSyncProgress %f\n", nTick, nRequestedLMNodeAssets, nRequestedLMNodeAttempt, nSyncProgress);
    uiInterface.NotifyAdditionalDataSyncProgressChanged(pCurrentBlockIndex->nHeight, nSyncProgress);

    // RESET SYNCING INCASE OF FAILURE
    {
        if (IsSynced()) {
            /*
                Resync if we lost all lmnodes from sleep/wake or failed to sync originally
            */
            if (nMnCount == 0) {
                LogPrintf("CLMNodeSync::ProcessTick -- WARNING: not enough data, restarting sync\n");
                Reset();
            } else {
                std::vector < CNode * > vNodesCopy = CopyNodeVector();
                ReleaseNodeVector(vNodesCopy);
                return;
            }
        }

        //try syncing again
        if (IsFailed()) {
            if (nTimeLastFailure + (1 * 60) < GetTime()) { // 1 minute cooldown after failed sync
                Reset();
            }
            return;
        }
    }

    if (Params().NetworkIDString() != CBaseChainParams::REGTEST && !IsBlockchainSynced() && nRequestedLMNodeAssets > LMNODE_SYNC_SPORKS) {
        nTimeLastLMNodeList = GetTime();
        nTimeLastPaymentVote = GetTime();
        nTimeLastGovernanceItem = GetTime();
        return;
    }
    if (nRequestedLMNodeAssets == LMNODE_SYNC_INITIAL || (nRequestedLMNodeAssets == LMNODE_SYNC_SPORKS && IsBlockchainSynced())) {
        SwitchToNextAsset();
    }

    std::vector < CNode * > vNodesCopy = CopyNodeVector();

    BOOST_FOREACH(CNode * pnode, vNodesCopy)
    {
        // Don't try to sync any data from outbound "lmnode" connections -
        // they are temporary and should be considered unreliable for a sync process.
        // Inbound connection this early is most likely a "lmnode" connection
        // initialted from another node, so skip it too.
        if (pnode->fLMNode || (fZNode && pnode->fInbound)) continue;

        // QUICK MODE (REGTEST ONLY!)
        if (Params().NetworkIDString() == CBaseChainParams::REGTEST) {
            if (nRequestedLMNodeAttempt <= 2) {
                pnode->PushMessage(NetMsgType::GETSPORKS); //get current network sporks
            } else if (nRequestedLMNodeAttempt < 4) {
                mnodeman.DsegUpdate(pnode);
            } else if (nRequestedLMNodeAttempt < 6) {
                int nMnCount = mnodeman.CountLMNodes();
                pnode->PushMessage(NetMsgType::LMNODEPAYMENTSYNC, nMnCount); //sync payment votes
            } else {
                nRequestedLMNodeAssets = LMNODE_SYNC_FINISHED;
            }
            nRequestedLMNodeAttempt++;
            ReleaseNodeVector(vNodesCopy);
            return;
        }

        // NORMAL NETWORK MODE - TESTNET/MAINNET
        {
            if (netfulfilledman.HasFulfilledRequest(pnode->addr, "full-sync")) {
                // We already fully synced from this node recently,
                // disconnect to free this connection slot for another peer.
                pnode->fDisconnect = true;
                LogPrintf("CLMNodeSync::ProcessTick -- disconnecting from recently synced peer %d\n", pnode->id);
                continue;
            }

            // SPORK : ALWAYS ASK FOR SPORKS AS WE SYNC (we skip this mode now)

            if (!netfulfilledman.HasFulfilledRequest(pnode->addr, "spork-sync")) {
                // only request once from each peer
                netfulfilledman.AddFulfilledRequest(pnode->addr, "spork-sync");
                // get current network sporks
                pnode->PushMessage(NetMsgType::GETSPORKS);
                LogPrintf("CLMNodeSync::ProcessTick -- nTick %d nRequestedLMNodeAssets %d -- requesting sporks from peer %d\n", nTick, nRequestedLMNodeAssets, pnode->id);
                continue; // always get sporks first, switch to the next node without waiting for the next tick
            }

            // MNLIST : SYNC LMNODE LIST FROM OTHER CONNECTED CLIENTS

            if (nRequestedLMNodeAssets == LMNODE_SYNC_LIST) {
                // check for timeout first
                if (nTimeLastLMNodeList < GetTime() - LMNODE_SYNC_TIMEOUT_SECONDS) {
                    LogPrintf("CLMNodeSync::ProcessTick -- nTick %d nRequestedLMNodeAssets %d -- timeout\n", nTick, nRequestedLMNodeAssets);
                    if (nRequestedLMNodeAttempt == 0) {
                        LogPrintf("CLMNodeSync::ProcessTick -- ERROR: failed to sync %s\n", GetAssetName());
                        // there is no way we can continue without lmnode list, fail here and try later
                        Fail();
                        ReleaseNodeVector(vNodesCopy);
                        return;
                    }
                    SwitchToNextAsset();
                    ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // only request once from each peer
                if (netfulfilledman.HasFulfilledRequest(pnode->addr, "lmnode-list-sync")) continue;
                netfulfilledman.AddFulfilledRequest(pnode->addr, "lmnode-list-sync");

                if (pnode->nVersion < mnpayments.GetMinLMNodePaymentsProto()) continue;
                nRequestedLMNodeAttempt++;

                mnodeman.DsegUpdate(pnode);

                ReleaseNodeVector(vNodesCopy);
                return; //this will cause each peer to get one request each six seconds for the various assets we need
            }

            // MNW : SYNC LMNODE PAYMENT VOTES FROM OTHER CONNECTED CLIENTS

            if (nRequestedLMNodeAssets == LMNODE_SYNC_MNW) {
                LogPrint("mnpayments", "CLMNodeSync::ProcessTick -- nTick %d nRequestedLMNodeAssets %d nTimeLastPaymentVote %lld GetTime() %lld diff %lld\n", nTick, nRequestedLMNodeAssets, nTimeLastPaymentVote, GetTime(), GetTime() - nTimeLastPaymentVote);
                // check for timeout first
                // This might take a lot longer than LMNODE_SYNC_TIMEOUT_SECONDS minutes due to new blocks,
                // but that should be OK and it should timeout eventually.
                if (nTimeLastPaymentVote < GetTime() - LMNODE_SYNC_TIMEOUT_SECONDS) {
                    LogPrintf("CLMNodeSync::ProcessTick -- nTick %d nRequestedLMNodeAssets %d -- timeout\n", nTick, nRequestedLMNodeAssets);
                    if (nRequestedLMNodeAttempt == 0) {
                        LogPrintf("CLMNodeSync::ProcessTick -- ERROR: failed to sync %s\n", GetAssetName());
                        // probably not a good idea to proceed without winner list
                        Fail();
                        ReleaseNodeVector(vNodesCopy);
                        return;
                    }
                    SwitchToNextAsset();
                    ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // check for data
                // if mnpayments already has enough blocks and votes, switch to the next asset
                // try to fetch data from at least two peers though
                if (nRequestedLMNodeAttempt > 1 && mnpayments.IsEnoughData()) {
                    LogPrintf("CLMNodeSync::ProcessTick -- nTick %d nRequestedLMNodeAssets %d -- found enough data\n", nTick, nRequestedLMNodeAssets);
                    SwitchToNextAsset();
                    ReleaseNodeVector(vNodesCopy);
                    return;
                }

                // only request once from each peer
                if (netfulfilledman.HasFulfilledRequest(pnode->addr, "lmnode-payment-sync")) continue;
                netfulfilledman.AddFulfilledRequest(pnode->addr, "lmnode-payment-sync");

                if (pnode->nVersion < mnpayments.GetMinLMNodePaymentsProto()) continue;
                nRequestedLMNodeAttempt++;

                // ask node for all payment votes it has (new nodes will only return votes for future payments)
                pnode->PushMessage(NetMsgType::LMNODEPAYMENTSYNC, mnpayments.GetStorageLimit());
                // ask node for missing pieces only (old nodes will not be asked)
                mnpayments.RequestLowDataPaymentBlocks(pnode);

                ReleaseNodeVector(vNodesCopy);
                return; //this will cause each peer to get one request each six seconds for the various assets we need
            }

        }
    }
    // looped through all nodes, release them
    ReleaseNodeVector(vNodesCopy);
}

void CLMNodeSync::UpdatedBlockTip(const CBlockIndex *pindex) {
    pCurrentBlockIndex = pindex;
}
