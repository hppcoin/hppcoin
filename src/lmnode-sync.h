// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2016-2017 The Zcoin developers
// Copyright (c) 2017-2018 The Hppcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef LMNODE_SYNC_H
#define LMNODE_SYNC_H

#include "chain.h"
#include "net.h"

#include <univalue.h>

class CLMNodeSync;

static const int LMNODE_SYNC_FAILED          = -1;
static const int LMNODE_SYNC_INITIAL         = 0;
static const int LMNODE_SYNC_SPORKS          = 1;
static const int LMNODE_SYNC_LIST            = 2;
static const int LMNODE_SYNC_MNW             = 3;
static const int LMNODE_SYNC_FINISHED        = 999;

static const int LMNODE_SYNC_TICK_SECONDS    = 6;
static const int LMNODE_SYNC_TIMEOUT_SECONDS = 30; // our blocks are 2.5 minutes so 30 seconds should be fine

//static const int LMNODE_SYNC_ENOUGH_PEERS    = 6;
static const int LMNODE_SYNC_ENOUGH_PEERS    = 3;

extern CLMNodeSync lmnodeSync;

//
// CLMNodeSync : Sync lmnode assets in stages
//

class CLMNodeSync
{
private:
    // Keep track of current asset
    int nRequestedLMNodeAssets;
    // Count peers we've requested the asset from
    int nRequestedLMNodeAttempt;

    // Time when current lmnode asset sync started
    int64_t nTimeAssetSyncStarted;

    // Last time when we received some lmnode asset ...
    int64_t nTimeLastLMNodeList;
    int64_t nTimeLastPaymentVote;
    int64_t nTimeLastGovernanceItem;
    // ... or failed
    int64_t nTimeLastFailure;

    // How many times we failed
    int nCountFailures;

    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

    bool CheckNodeHeight(CNode* pnode, bool fDisconnectStuckNodes = false);
    void Fail();
    void ClearFulfilledRequests();

public:
    CLMNodeSync() { Reset(); }

    void AddedLMNodeList() { nTimeLastLMNodeList = GetTime(); }
    void AddedPaymentVote() { nTimeLastPaymentVote = GetTime(); }
    void AddedGovernanceItem() { nTimeLastGovernanceItem = GetTime(); };

    void SendGovernanceSyncRequest(CNode* pnode);

    bool IsFailed() { return nRequestedLMNodeAssets == LMNODE_SYNC_FAILED; }
    bool IsBlockchainSynced(bool fBlockAccepted = false);
    bool IsLMNodeListSynced() { return nRequestedLMNodeAssets > LMNODE_SYNC_LIST; }
    bool IsWinnersListSynced() { return nRequestedLMNodeAssets > LMNODE_SYNC_MNW; }
    bool IsSynced() { return nRequestedLMNodeAssets == LMNODE_SYNC_FINISHED; }

    int GetAssetID() { return nRequestedLMNodeAssets; }
    int GetAttempt() { return nRequestedLMNodeAttempt; }
    std::string GetAssetName();
    std::string GetSyncStatus();

    void Reset();
    void SwitchToNextAsset();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);
    void ProcessTick();

    void UpdatedBlockTip(const CBlockIndex *pindex);
};

#endif
