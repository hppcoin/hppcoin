// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2016-2017 The Zcoin developers
// Copyright (c) 2017-2018 The Hppcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activelmnode.h"
#include "addrman.h"
#include "darksend.h"
//#include "governance.h"
#include "lmnode-payments.h"
#include "lmnode-sync.h"
#include "lmnodeman.h"
#include "netfulfilledman.h"
#include "util.h"

/** LMNode manager */
CLMNodeMan mnodeman;

const std::string CLMNodeMan::SERIALIZATION_VERSION_STRING = "CLMNodeMan-Version-4";

struct CompareLastPaidBlock
{
    bool operator()(const std::pair<int, CLMNode*>& t1,
                    const std::pair<int, CLMNode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

struct CompareScoreMN
{
    bool operator()(const std::pair<int64_t, CLMNode*>& t1,
                    const std::pair<int64_t, CLMNode*>& t2) const
    {
        return (t1.first != t2.first) ? (t1.first < t2.first) : (t1.second->vin < t2.second->vin);
    }
};

CLMNodeIndex::CLMNodeIndex()
    : nSize(0),
      mapIndex(),
      mapReverseIndex()
{}

bool CLMNodeIndex::Get(int nIndex, CTxIn& vinLMNode) const
{
    rindex_m_cit it = mapReverseIndex.find(nIndex);
    if(it == mapReverseIndex.end()) {
        return false;
    }
    vinLMNode = it->second;
    return true;
}

int CLMNodeIndex::GetLMNodeIndex(const CTxIn& vinLMNode) const
{
    index_m_cit it = mapIndex.find(vinLMNode);
    if(it == mapIndex.end()) {
        return -1;
    }
    return it->second;
}

void CLMNodeIndex::AddLMNodeVIN(const CTxIn& vinLMNode)
{
    index_m_it it = mapIndex.find(vinLMNode);
    if(it != mapIndex.end()) {
        return;
    }
    int nNextIndex = nSize;
    mapIndex[vinLMNode] = nNextIndex;
    mapReverseIndex[nNextIndex] = vinLMNode;
    ++nSize;
}

void CLMNodeIndex::Clear()
{
    mapIndex.clear();
    mapReverseIndex.clear();
    nSize = 0;
}
struct CompareByAddr

{
    bool operator()(const CLMNode* t1,
                    const CLMNode* t2) const
    {
        return t1->addr < t2->addr;
    }
};

void CLMNodeIndex::RebuildIndex()
{
    nSize = mapIndex.size();
    for(index_m_it it = mapIndex.begin(); it != mapIndex.end(); ++it) {
        mapReverseIndex[it->second] = it->first;
    }
}

CLMNodeMan::CLMNodeMan() : cs(),
  vLMNodes(),
  mAskedUsForLMNodeList(),
  mWeAskedForLMNodeList(),
  mWeAskedForLMNodeListEntry(),
  mWeAskedForVerification(),
  mMnbRecoveryRequests(),
  mMnbRecoveryGoodReplies(),
  listScheduledMnbRequestConnections(),
  nLastIndexRebuildTime(0),
  indexLMNodes(),
  indexLMNodesOld(),
  fIndexRebuilt(false),
  fLMNodesAdded(false),
  fLMNodesRemoved(false),
//  vecDirtyGovernanceObjectHashes(),
  nLastWatchdogVoteTime(0),
  mapSeenLMNodeBroadcast(),
  mapSeenLMNodePing(),
  nDsqCount(0)
{}

bool CLMNodeMan::Add(CLMNode &mn)
{
    LOCK(cs);

    CLMNode *pmn = Find(mn.vin);
    if (pmn == NULL) {
        LogPrint("lmnode", "CLMNodeMan::Add -- Adding new LMNode: addr=%s, %i now\n", mn.addr.ToString(), size() + 1);
        vLMNodes.push_back(mn);
        indexLMNodes.AddLMNodeVIN(mn.vin);
        fLMNodesAdded = true;
        return true;
    }

    return false;
}

void CLMNodeMan::AskForMN(CNode* pnode, const CTxIn &vin)
{
    if(!pnode) return;

    LOCK(cs);

    std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it1 = mWeAskedForLMNodeListEntry.find(vin.prevout);
    if (it1 != mWeAskedForLMNodeListEntry.end()) {
        std::map<CNetAddr, int64_t>::iterator it2 = it1->second.find(pnode->addr);
        if (it2 != it1->second.end()) {
            if (GetTime() < it2->second) {
                // we've asked recently, should not repeat too often or we could get banned
                return;
            }
            // we asked this node for this outpoint but it's ok to ask again already
            LogPrintf("CLMNodeMan::AskForMN -- Asking same peer %s for missing lmnode entry again: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
        } else {
            // we already asked for this outpoint but not this node
            LogPrintf("CLMNodeMan::AskForMN -- Asking new peer %s for missing lmnode entry: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
        }
    } else {
        // we never asked any node for this outpoint
        LogPrintf("CLMNodeMan::AskForMN -- Asking peer %s for missing lmnode entry for the first time: %s\n", pnode->addr.ToString(), vin.prevout.ToStringShort());
    }
    mWeAskedForLMNodeListEntry[vin.prevout][pnode->addr] = GetTime() + DSEG_UPDATE_SECONDS;

    pnode->PushMessage(NetMsgType::DSEG, vin);
}

void CLMNodeMan::Check()
{
    LOCK(cs);

//    LogPrint("lmnode", "CLMNodeMan::Check -- nLastWatchdogVoteTime=%d, IsWatchdogActive()=%d\n", nLastWatchdogVoteTime, IsWatchdogActive());

    BOOST_FOREACH(CLMNode& mn, vLMNodes) {
        mn.Check();
    }
}

void CLMNodeMan::CheckAndRemove()
{
    if(!lmnodeSync.IsLMNodeListSynced()) return;

    LogPrintf("CLMNodeMan::CheckAndRemove\n");

    {
        // Need LOCK2 here to ensure consistent locking order because code below locks cs_main
        // in CheckMnbAndUpdateLMNodeList()
        LOCK2(cs_main, cs);

        Check();

        // Remove spent lmnodes, prepare structures and make requests to reasure the state of inactive ones
        std::vector<CLMNode>::iterator it = vLMNodes.begin();
        std::vector<std::pair<int, CLMNode> > vecLMNodeRanks;
        // ask for up to MNB_RECOVERY_MAX_ASK_ENTRIES lmnode entries at a time
        int nAskForMnbRecovery = MNB_RECOVERY_MAX_ASK_ENTRIES;
        while(it != vLMNodes.end()) {
            CLMNodeBroadcast mnb = CLMNodeBroadcast(*it);
            uint256 hash = mnb.GetHash();
            // If collateral was spent ...
            if ((*it).IsOutpointSpent()) {
                LogPrint("lmnode", "CLMNodeMan::CheckAndRemove -- Removing LMNode: %s  addr=%s  %i now\n", (*it).GetStateString(), (*it).addr.ToString(), size() - 1);

                // erase all of the broadcasts we've seen from this txin, ...
                mapSeenLMNodeBroadcast.erase(hash);
                mWeAskedForLMNodeListEntry.erase((*it).vin.prevout);

                // and finally remove it from the list
//                it->FlagGovernanceItemsAsDirty();
                it = vLMNodes.erase(it);
                fLMNodesRemoved = true;
            } else {
                bool fAsk = pCurrentBlockIndex &&
                            (nAskForMnbRecovery > 0) &&
                            lmnodeSync.IsSynced() &&
                            it->IsNewStartRequired() &&
                            !IsMnbRecoveryRequested(hash);
                if(fAsk) {
                    // this mn is in a non-recoverable state and we haven't asked other nodes yet
                    std::set<CNetAddr> setRequested;
                    // calulate only once and only when it's needed
                    if(vecLMNodeRanks.empty()) {
                        int nRandomBlockHeight = GetRandInt(pCurrentBlockIndex->nHeight);
                        vecLMNodeRanks = GetLMNodeRanks(nRandomBlockHeight);
                    }
                    bool fAskedForMnbRecovery = false;
                    // ask first MNB_RECOVERY_QUORUM_TOTAL lmnodes we can connect to and we haven't asked recently
                    for(int i = 0; setRequested.size() < MNB_RECOVERY_QUORUM_TOTAL && i < (int)vecLMNodeRanks.size(); i++) {
                        // avoid banning
                        if(mWeAskedForLMNodeListEntry.count(it->vin.prevout) && mWeAskedForLMNodeListEntry[it->vin.prevout].count(vecLMNodeRanks[i].second.addr)) continue;
                        // didn't ask recently, ok to ask now
                        CService addr = vecLMNodeRanks[i].second.addr;
                        setRequested.insert(addr);
                        listScheduledMnbRequestConnections.push_back(std::make_pair(addr, hash));
                        fAskedForMnbRecovery = true;
                    }
                    if(fAskedForMnbRecovery) {
                        LogPrint("lmnode", "CLMNodeMan::CheckAndRemove -- Recovery initiated, lmnode=%s\n", it->vin.prevout.ToStringShort());
                        nAskForMnbRecovery--;
                    }
                    // wait for mnb recovery replies for MNB_RECOVERY_WAIT_SECONDS seconds
                    mMnbRecoveryRequests[hash] = std::make_pair(GetTime() + MNB_RECOVERY_WAIT_SECONDS, setRequested);
                }
                ++it;
            }
        }

        // proces replies for LMNODE_NEW_START_REQUIRED lmnodes
        LogPrint("lmnode", "CLMNodeMan::CheckAndRemove -- mMnbRecoveryGoodReplies size=%d\n", (int)mMnbRecoveryGoodReplies.size());
        std::map<uint256, std::vector<CLMNodeBroadcast> >::iterator itMnbReplies = mMnbRecoveryGoodReplies.begin();
        while(itMnbReplies != mMnbRecoveryGoodReplies.end()){
            if(mMnbRecoveryRequests[itMnbReplies->first].first < GetTime()) {
                // all nodes we asked should have replied now
                if(itMnbReplies->second.size() >= MNB_RECOVERY_QUORUM_REQUIRED) {
                    // majority of nodes we asked agrees that this mn doesn't require new mnb, reprocess one of new mnbs
                    LogPrint("lmnode", "CLMNodeMan::CheckAndRemove -- reprocessing mnb, lmnode=%s\n", itMnbReplies->second[0].vin.prevout.ToStringShort());
                    // mapSeenLMNodeBroadcast.erase(itMnbReplies->first);
                    int nDos;
                    itMnbReplies->second[0].fRecovery = true;
                    CheckMnbAndUpdateLMNodeList(NULL, itMnbReplies->second[0], nDos);
                }
                LogPrint("lmnode", "CLMNodeMan::CheckAndRemove -- removing mnb recovery reply, lmnode=%s, size=%d\n", itMnbReplies->second[0].vin.prevout.ToStringShort(), (int)itMnbReplies->second.size());
                mMnbRecoveryGoodReplies.erase(itMnbReplies++);
            } else {
                ++itMnbReplies;
            }
        }
    }
    {
        // no need for cm_main below
        LOCK(cs);

        std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > >::iterator itMnbRequest = mMnbRecoveryRequests.begin();
        while(itMnbRequest != mMnbRecoveryRequests.end()){
            // Allow this mnb to be re-verified again after MNB_RECOVERY_RETRY_SECONDS seconds
            // if mn is still in LMNODE_NEW_START_REQUIRED state.
            if(GetTime() - itMnbRequest->second.first > MNB_RECOVERY_RETRY_SECONDS) {
                mMnbRecoveryRequests.erase(itMnbRequest++);
            } else {
                ++itMnbRequest;
            }
        }

        // check who's asked for the LMNode list
        std::map<CNetAddr, int64_t>::iterator it1 = mAskedUsForLMNodeList.begin();
        while(it1 != mAskedUsForLMNodeList.end()){
            if((*it1).second < GetTime()) {
                mAskedUsForLMNodeList.erase(it1++);
            } else {
                ++it1;
            }
        }

        // check who we asked for the LMNode list
        it1 = mWeAskedForLMNodeList.begin();
        while(it1 != mWeAskedForLMNodeList.end()){
            if((*it1).second < GetTime()){
                mWeAskedForLMNodeList.erase(it1++);
            } else {
                ++it1;
            }
        }

        // check which LMNodes we've asked for
        std::map<COutPoint, std::map<CNetAddr, int64_t> >::iterator it2 = mWeAskedForLMNodeListEntry.begin();
        while(it2 != mWeAskedForLMNodeListEntry.end()){
            std::map<CNetAddr, int64_t>::iterator it3 = it2->second.begin();
            while(it3 != it2->second.end()){
                if(it3->second < GetTime()){
                    it2->second.erase(it3++);
                } else {
                    ++it3;
                }
            }
            if(it2->second.empty()) {
                mWeAskedForLMNodeListEntry.erase(it2++);
            } else {
                ++it2;
            }
        }

        std::map<CNetAddr, CLMNodeVerification>::iterator it3 = mWeAskedForVerification.begin();
        while(it3 != mWeAskedForVerification.end()){
            if(it3->second.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS) {
                mWeAskedForVerification.erase(it3++);
            } else {
                ++it3;
            }
        }

        // NOTE: do not expire mapSeenLMNodeBroadcast entries here, clean them on mnb updates!

        // remove expired mapSeenLMNodePing
        std::map<uint256, CLMNodePing>::iterator it4 = mapSeenLMNodePing.begin();
        while(it4 != mapSeenLMNodePing.end()){
            if((*it4).second.IsExpired()) {
                LogPrint("lmnode", "CLMNodeMan::CheckAndRemove -- Removing expired LMNode ping: hash=%s\n", (*it4).second.GetHash().ToString());
                mapSeenLMNodePing.erase(it4++);
            } else {
                ++it4;
            }
        }

        // remove expired mapSeenLMNodeVerification
        std::map<uint256, CLMNodeVerification>::iterator itv2 = mapSeenLMNodeVerification.begin();
        while(itv2 != mapSeenLMNodeVerification.end()){
            if((*itv2).second.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS){
                LogPrint("lmnode", "CLMNodeMan::CheckAndRemove -- Removing expired LMNode verification: hash=%s\n", (*itv2).first.ToString());
                mapSeenLMNodeVerification.erase(itv2++);
            } else {
                ++itv2;
            }
        }

        LogPrintf("CLMNodeMan::CheckAndRemove -- %s\n", ToString());

        if(fLMNodesRemoved) {
            CheckAndRebuildLMNodeIndex();
        }
    }

    if(fLMNodesRemoved) {
        NotifyLMNodeUpdates();
    }
}

void CLMNodeMan::Clear()
{
    LOCK(cs);
    vLMNodes.clear();
    mAskedUsForLMNodeList.clear();
    mWeAskedForLMNodeList.clear();
    mWeAskedForLMNodeListEntry.clear();
    mapSeenLMNodeBroadcast.clear();
    mapSeenLMNodePing.clear();
    nDsqCount = 0;
    nLastWatchdogVoteTime = 0;
    indexLMNodes.Clear();
    indexLMNodesOld.Clear();
}

int CLMNodeMan::CountLMNodes(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;
    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinLMNodePaymentsProto() : nProtocolVersion;

    BOOST_FOREACH(CLMNode& mn, vLMNodes) {
        if(mn.nProtocolVersion < nProtocolVersion) continue;
        nCount++;
    }

    return nCount;
}

int CLMNodeMan::CountEnabled(int nProtocolVersion)
{
    LOCK(cs);
    int nCount = 0;
    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinLMNodePaymentsProto() : nProtocolVersion;

    BOOST_FOREACH(CLMNode& mn, vLMNodes) {
        if(mn.nProtocolVersion < nProtocolVersion || !mn.IsEnabled()) continue;
        nCount++;
    }

    return nCount;
}

/* Only IPv4 lmnodes are allowed in 12.1, saving this for later
int CLMNodeMan::CountByIP(int nNetworkType)
{
    LOCK(cs);
    int nNodeCount = 0;

    BOOST_FOREACH(CLMNode& mn, vLMNodes)
        if ((nNetworkType == NET_IPV4 && mn.addr.IsIPv4()) ||
            (nNetworkType == NET_TOR  && mn.addr.IsTor())  ||
            (nNetworkType == NET_IPV6 && mn.addr.IsIPv6())) {
                nNodeCount++;
        }

    return nNodeCount;
}
*/

void CLMNodeMan::DsegUpdate(CNode* pnode)
{
    LOCK(cs);

    if(Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if(!(pnode->addr.IsRFC1918() || pnode->addr.IsLocal())) {
            std::map<CNetAddr, int64_t>::iterator it = mWeAskedForLMNodeList.find(pnode->addr);
            if(it != mWeAskedForLMNodeList.end() && GetTime() < (*it).second) {
                LogPrintf("CLMNodeMan::DsegUpdate -- we already asked %s for the list; skipping...\n", pnode->addr.ToString());
                return;
            }
        }
    }
    
    pnode->PushMessage(NetMsgType::DSEG, CTxIn());
    int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
    mWeAskedForLMNodeList[pnode->addr] = askAgain;

    LogPrint("lmnode", "CLMNodeMan::DsegUpdate -- asked %s for the list\n", pnode->addr.ToString());
}

CLMNode* CLMNodeMan::Find(const CScript &payee)
{
    LOCK(cs);

    BOOST_FOREACH(CLMNode& mn, vLMNodes)
    {
        if(GetScriptForDestination(mn.pubKeyCollateralAddress.GetID()) == payee)
            return &mn;
    }
    return NULL;
}

CLMNode* CLMNodeMan::Find(const CTxIn &vin)
{
    LOCK(cs);

    BOOST_FOREACH(CLMNode& mn, vLMNodes)
    {
        if(mn.vin.prevout == vin.prevout)
            return &mn;
    }
    return NULL;
}

CLMNode* CLMNodeMan::Find(const CPubKey &pubKeyLMNode)
{
    LOCK(cs);

    BOOST_FOREACH(CLMNode& mn, vLMNodes)
    {
        if(mn.pubKeyLMNode == pubKeyLMNode)
            return &mn;
    }
    return NULL;
}

bool CLMNodeMan::Get(const CPubKey& pubKeyLMNode, CLMNode& lmnode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CLMNode* pMN = Find(pubKeyLMNode);
    if(!pMN)  {
        return false;
    }
    lmnode = *pMN;
    return true;
}

bool CLMNodeMan::Get(const CTxIn& vin, CLMNode& lmnode)
{
    // Theses mutexes are recursive so double locking by the same thread is safe.
    LOCK(cs);
    CLMNode* pMN = Find(vin);
    if(!pMN)  {
        return false;
    }
    lmnode = *pMN;
    return true;
}

lmnode_info_t CLMNodeMan::GetLMNodeInfo(const CTxIn& vin)
{
    lmnode_info_t info;
    LOCK(cs);
    CLMNode* pMN = Find(vin);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

lmnode_info_t CLMNodeMan::GetLMNodeInfo(const CPubKey& pubKeyLMNode)
{
    lmnode_info_t info;
    LOCK(cs);
    CLMNode* pMN = Find(pubKeyLMNode);
    if(!pMN)  {
        return info;
    }
    info = pMN->GetInfo();
    return info;
}

bool CLMNodeMan::Has(const CTxIn& vin)
{
    LOCK(cs);
    CLMNode* pMN = Find(vin);
    return (pMN != NULL);
}

char* CLMNodeMan::GetNotQualifyReason(CLMNode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount)
{
    if (!mn.IsValidForPayment()) {
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'not valid for payment'");
        return reasonStr;
    }
    // //check protocol version
    if (mn.nProtocolVersion < mnpayments.GetMinLMNodePaymentsProto()) {
        // LogPrintf("Invalid nProtocolVersion!\n");
        // LogPrintf("mn.nProtocolVersion=%s!\n", mn.nProtocolVersion);
        // LogPrintf("mnpayments.GetMinLMNodePaymentsProto=%s!\n", mnpayments.GetMinLMNodePaymentsProto());
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'Invalid nProtocolVersion', nProtocolVersion=%d", mn.nProtocolVersion);
        return reasonStr;
    }
    //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
    if (mnpayments.IsScheduled(mn, nBlockHeight)) {
        // LogPrintf("mnpayments.IsScheduled!\n");
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'is scheduled'");
        return reasonStr;
    }
    //it's too new, wait for a cycle
    if (fFilterSigTime && mn.sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) {
        // LogPrintf("it's too new, wait for a cycle!\n");
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'too new', sigTime=%s, will be qualifed after=%s",
                DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime + (nMnCount * 2.6 * 60)).c_str());
        return reasonStr;
    }
    //make sure it has at least as many confirmations as there are lmnodes
    if (mn.GetCollateralAge() < nMnCount) {
        // LogPrintf("mn.GetCollateralAge()=%s!\n", mn.GetCollateralAge());
        // LogPrintf("nMnCount=%s!\n", nMnCount);
        char* reasonStr = new char[256];
        sprintf(reasonStr, "false: 'collateralAge < znCount', collateralAge=%d, znCount=%d", mn.GetCollateralAge(), nMnCount);
        return reasonStr;
    }
    return NULL;
}

//
// Deterministically select the oldest/best lmnode to pay on the network
//
CLMNode* CLMNodeMan::GetNextLMNodeInQueueForPayment(bool fFilterSigTime, int& nCount)
{
    if(!pCurrentBlockIndex) {
        nCount = 0;
        return NULL;
    }
    return GetNextLMNodeInQueueForPayment(pCurrentBlockIndex->nHeight, fFilterSigTime, nCount);
}

CLMNode* CLMNodeMan::GetNextLMNodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount)
{
    // Need LOCK2 here to ensure consistent locking order because the GetBlockHash call below locks cs_main
    LOCK2(cs_main,cs);

    CLMNode *pBestLMNode = NULL;
    std::vector<std::pair<int, CLMNode*> > vecLMNodeLastPaid;

    /*
        Make a vector with all of the last paid times
    */
    int nMnCount = CountEnabled();
    int index = 0;
    BOOST_FOREACH(CLMNode &mn, vLMNodes)
    {
        index += 1;
        // LogPrintf("index=%s, mn=%s\n", index, mn.ToString());
        /*if (!mn.IsValidForPayment()) {
            LogPrint("lmnodeman", "LMNode, %s, addr(%s), not-qualified: 'not valid for payment'\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString());
            continue;
        }
        // //check protocol version
        if (mn.nProtocolVersion < mnpayments.GetMinLMNodePaymentsProto()) {
            // LogPrintf("Invalid nProtocolVersion!\n");
            // LogPrintf("mn.nProtocolVersion=%s!\n", mn.nProtocolVersion);
            // LogPrintf("mnpayments.GetMinLMNodePaymentsProto=%s!\n", mnpayments.GetMinLMNodePaymentsProto());
            LogPrint("lmnodeman", "LMNode, %s, addr(%s), not-qualified: 'invalid nProtocolVersion'\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString());
            continue;
        }
        //it's in the list (up to 8 entries ahead of current block to allow propagation) -- so let's skip it
        if (mnpayments.IsScheduled(mn, nBlockHeight)) {
            // LogPrintf("mnpayments.IsScheduled!\n");
            LogPrint("lmnodeman", "LMNode, %s, addr(%s), not-qualified: 'IsScheduled'\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString());
            continue;
        }
        //it's too new, wait for a cycle
        if (fFilterSigTime && mn.sigTime + (nMnCount * 2.6 * 60) > GetAdjustedTime()) {
            // LogPrintf("it's too new, wait for a cycle!\n");
            LogPrint("lmnodeman", "LMNode, %s, addr(%s), not-qualified: 'it's too new, wait for a cycle!', sigTime=%s, will be qualifed after=%s\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString(), DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime).c_str(), DateTimeStrFormat("%Y-%m-%d %H:%M UTC", mn.sigTime + (nMnCount * 2.6 * 60)).c_str());
            continue;
        }
        //make sure it has at least as many confirmations as there are lmnodes
        if (mn.GetCollateralAge() < nMnCount) {
            // LogPrintf("mn.GetCollateralAge()=%s!\n", mn.GetCollateralAge());
            // LogPrintf("nMnCount=%s!\n", nMnCount);
            LogPrint("lmnodeman", "LMNode, %s, addr(%s), not-qualified: 'mn.GetCollateralAge() < nMnCount', CollateralAge=%d, nMnCount=%d\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString(), mn.GetCollateralAge(), nMnCount);
            continue;
        }*/
        char* reasonStr = GetNotQualifyReason(mn, nBlockHeight, fFilterSigTime, nMnCount);
        if (reasonStr != NULL) {
            LogPrint("lmnodeman", "LMNode, %s, addr(%s), qualify %s\n",
                     mn.vin.prevout.ToStringShort(), CBitcoinAddress(mn.pubKeyCollateralAddress.GetID()).ToString(), reasonStr);
            continue;
        }
        vecLMNodeLastPaid.push_back(std::make_pair(mn.GetLastPaidBlock(), &mn));
    }
    nCount = (int)vecLMNodeLastPaid.size();

    //when the network is in the process of upgrading, don't penalize nodes that recently restarted
    if(fFilterSigTime && nCount < nMnCount / 3) {
        // LogPrintf("Need Return, nCount=%s, nMnCount/3=%s\n", nCount, nMnCount/3);
        return GetNextLMNodeInQueueForPayment(nBlockHeight, false, nCount);
    }

    // Sort them low to high
    sort(vecLMNodeLastPaid.begin(), vecLMNodeLastPaid.end(), CompareLastPaidBlock());

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight - 101)) {
        LogPrintf("CLMNode::GetNextLMNodeInQueueForPayment -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", nBlockHeight - 101);
        return NULL;
    }
    // Look at 1/10 of the oldest nodes (by last payment), calculate their scores and pay the best one
    //  -- This doesn't look at who is being paid in the +8-10 blocks, allowing for double payments very rarely
    //  -- 1/100 payments should be a double payment on mainnet - (1/(3000/10))*2
    //  -- (chance per block * chances before IsScheduled will fire)
    int nTenthNetwork = nMnCount/10;
    int nCountTenth = 0;
    arith_uint256 nHighest = 0;
    BOOST_FOREACH (PAIRTYPE(int, CLMNode*)& s, vecLMNodeLastPaid){
        arith_uint256 nScore = s.second->CalculateScore(blockHash);
        if(nScore > nHighest){
            nHighest = nScore;
            pBestLMNode = s.second;
        }
        nCountTenth++;
        if(nCountTenth >= nTenthNetwork) break;
    }
    return pBestLMNode;
}

CLMNode* CLMNodeMan::FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion)
{
    LOCK(cs);

    nProtocolVersion = nProtocolVersion == -1 ? mnpayments.GetMinLMNodePaymentsProto() : nProtocolVersion;

    int nCountEnabled = CountEnabled(nProtocolVersion);
    int nCountNotExcluded = nCountEnabled - vecToExclude.size();

    LogPrintf("CLMNodeMan::FindRandomNotInVec -- %d enabled lmnodes, %d lmnodes to choose from\n", nCountEnabled, nCountNotExcluded);
    if(nCountNotExcluded < 1) return NULL;

    // fill a vector of pointers
    std::vector<CLMNode*> vpLMNodesShuffled;
    BOOST_FOREACH(CLMNode &mn, vLMNodes) {
        vpLMNodesShuffled.push_back(&mn);
    }

    InsecureRand insecureRand;
    // shuffle pointers
    std::random_shuffle(vpLMNodesShuffled.begin(), vpLMNodesShuffled.end(), insecureRand);
    bool fExclude;

    // loop through
    BOOST_FOREACH(CLMNode* pmn, vpLMNodesShuffled) {
        if(pmn->nProtocolVersion < nProtocolVersion || !pmn->IsEnabled()) continue;
        fExclude = false;
        BOOST_FOREACH(const CTxIn &txinToExclude, vecToExclude) {
            if(pmn->vin.prevout == txinToExclude.prevout) {
                fExclude = true;
                break;
            }
        }
        if(fExclude) continue;
        // found the one not in vecToExclude
        LogPrint("lmnode", "CLMNodeMan::FindRandomNotInVec -- found, lmnode=%s\n", pmn->vin.prevout.ToStringShort());
        return pmn;
    }

    LogPrint("lmnode", "CLMNodeMan::FindRandomNotInVec -- failed\n");
    return NULL;
}

int CLMNodeMan::GetLMNodeRank(const CTxIn& vin, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CLMNode*> > vecLMNodeScores;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight)) return -1;

    LOCK(cs);

    // scan for winner
    BOOST_FOREACH(CLMNode& mn, vLMNodes) {
        if(mn.nProtocolVersion < nMinProtocol) continue;
        if(fOnlyActive) {
            if(!mn.IsEnabled()) continue;
        }
        else {
            if(!mn.IsValidForPayment()) continue;
        }
        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecLMNodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecLMNodeScores.rbegin(), vecLMNodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CLMNode*)& scorePair, vecLMNodeScores) {
        nRank++;
        if(scorePair.second->vin.prevout == vin.prevout) return nRank;
    }

    return -1;
}

std::vector<std::pair<int, CLMNode> > CLMNodeMan::GetLMNodeRanks(int nBlockHeight, int nMinProtocol)
{
    std::vector<std::pair<int64_t, CLMNode*> > vecLMNodeScores;
    std::vector<std::pair<int, CLMNode> > vecLMNodeRanks;

    //make sure we know about this block
    uint256 blockHash = uint256();
    if(!GetBlockHash(blockHash, nBlockHeight)) return vecLMNodeRanks;

    LOCK(cs);

    // scan for winner
    BOOST_FOREACH(CLMNode& mn, vLMNodes) {

        if(mn.nProtocolVersion < nMinProtocol || !mn.IsEnabled()) continue;

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecLMNodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecLMNodeScores.rbegin(), vecLMNodeScores.rend(), CompareScoreMN());

    int nRank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CLMNode*)& s, vecLMNodeScores) {
        nRank++;
        vecLMNodeRanks.push_back(std::make_pair(nRank, *s.second));
    }

    return vecLMNodeRanks;
}

CLMNode* CLMNodeMan::GetLMNodeByRank(int nRank, int nBlockHeight, int nMinProtocol, bool fOnlyActive)
{
    std::vector<std::pair<int64_t, CLMNode*> > vecLMNodeScores;

    LOCK(cs);

    uint256 blockHash;
    if(!GetBlockHash(blockHash, nBlockHeight)) {
        LogPrintf("CLMNode::GetLMNodeByRank -- ERROR: GetBlockHash() failed at nBlockHeight %d\n", nBlockHeight);
        return NULL;
    }

    // Fill scores
    BOOST_FOREACH(CLMNode& mn, vLMNodes) {

        if(mn.nProtocolVersion < nMinProtocol) continue;
        if(fOnlyActive && !mn.IsEnabled()) continue;

        int64_t nScore = mn.CalculateScore(blockHash).GetCompact(false);

        vecLMNodeScores.push_back(std::make_pair(nScore, &mn));
    }

    sort(vecLMNodeScores.rbegin(), vecLMNodeScores.rend(), CompareScoreMN());

    int rank = 0;
    BOOST_FOREACH (PAIRTYPE(int64_t, CLMNode*)& s, vecLMNodeScores){
        rank++;
        if(rank == nRank) {
            return s.second;
        }
    }

    return NULL;
}

void CLMNodeMan::ProcessLMNodeConnections()
{
    //we don't care about this for regtest
    if(Params().NetworkIDString() == CBaseChainParams::REGTEST) return;

    LOCK(cs_vNodes);
    BOOST_FOREACH(CNode* pnode, vNodes) {
        if(pnode->fLMNode) {
            if(darkSendPool.pSubmittedToLMNode != NULL && pnode->addr == darkSendPool.pSubmittedToLMNode->addr) continue;
            // LogPrintf("Closing LMNode connection: peer=%d, addr=%s\n", pnode->id, pnode->addr.ToString());
            pnode->fDisconnect = true;
        }
    }
}

std::pair<CService, std::set<uint256> > CLMNodeMan::PopScheduledMnbRequestConnection()
{
    LOCK(cs);
    if(listScheduledMnbRequestConnections.empty()) {
        return std::make_pair(CService(), std::set<uint256>());
    }

    std::set<uint256> setResult;

    listScheduledMnbRequestConnections.sort();
    std::pair<CService, uint256> pairFront = listScheduledMnbRequestConnections.front();

    // squash hashes from requests with the same CService as the first one into setResult
    std::list< std::pair<CService, uint256> >::iterator it = listScheduledMnbRequestConnections.begin();
    while(it != listScheduledMnbRequestConnections.end()) {
        if(pairFront.first == it->first) {
            setResult.insert(it->second);
            it = listScheduledMnbRequestConnections.erase(it);
        } else {
            // since list is sorted now, we can be sure that there is no more hashes left
            // to ask for from this addr
            break;
        }
    }
    return std::make_pair(pairFront.first, setResult);
}


void CLMNodeMan::ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv)
{

//    LogPrint("lmnode", "CLMNodeMan::ProcessMessage, strCommand=%s\n", strCommand);
    if(fLiteMode) return; // disable all Dash specific functionality
    if(!lmnodeSync.IsBlockchainSynced()) return;

    if (strCommand == NetMsgType::MNANNOUNCE) { //LMNode Broadcast
        CLMNodeBroadcast mnb;
        vRecv >> mnb;

        pfrom->setAskFor.erase(mnb.GetHash());

        LogPrintf("MNANNOUNCE -- LMNode announce, lmnode=%s\n", mnb.vin.prevout.ToStringShort());

        int nDos = 0;

        if (CheckMnbAndUpdateLMNodeList(pfrom, mnb, nDos)) {
            // use announced LMNode as a peer
            addrman.Add(CAddress(mnb.addr, NODE_NETWORK), pfrom->addr, 2*60*60);
        } else if(nDos > 0) {
            Misbehaving(pfrom->GetId(), nDos);
        }

        if(fLMNodesAdded) {
            NotifyLMNodeUpdates();
        }
    } else if (strCommand == NetMsgType::MNPING) { //LMNode Ping

        CLMNodePing mnp;
        vRecv >> mnp;

        uint256 nHash = mnp.GetHash();

        pfrom->setAskFor.erase(nHash);

        LogPrint("lmnode", "MNPING -- LMNode ping, lmnode=%s\n", mnp.vin.prevout.ToStringShort());

        // Need LOCK2 here to ensure consistent locking order because the CheckAndUpdate call below locks cs_main
        LOCK2(cs_main, cs);

        if(mapSeenLMNodePing.count(nHash)) return; //seen
        mapSeenLMNodePing.insert(std::make_pair(nHash, mnp));

        LogPrint("lmnode", "MNPING -- LMNode ping, lmnode=%s new\n", mnp.vin.prevout.ToStringShort());

        // see if we have this LMNode
        CLMNode* pmn = mnodeman.Find(mnp.vin);

        // too late, new MNANNOUNCE is required
        if(pmn && pmn->IsNewStartRequired()) return;

        int nDos = 0;
        if(mnp.CheckAndUpdate(pmn, false, nDos)) return;

        if(nDos > 0) {
            // if anything significant failed, mark that node
            Misbehaving(pfrom->GetId(), nDos);
        } else if(pmn != NULL) {
            // nothing significant failed, mn is a known one too
            return;
        }

        // something significant is broken or mn is unknown,
        // we might have to ask for a lmnode entry once
        AskForMN(pfrom, mnp.vin);

    } else if (strCommand == NetMsgType::DSEG) { //Get LMNode list or specific entry
        // Ignore such requests until we are fully synced.
        // We could start processing this after lmnode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!lmnodeSync.IsSynced()) return;

        CTxIn vin;
        vRecv >> vin;

        LogPrint("lmnode", "DSEG -- LMNode list, lmnode=%s\n", vin.prevout.ToStringShort());

        LOCK(cs);

        if(vin == CTxIn()) { //only should ask for this once
            //local network
            bool isLocal = (pfrom->addr.IsRFC1918() || pfrom->addr.IsLocal());

            if(!isLocal && Params().NetworkIDString() == CBaseChainParams::MAIN) {
                std::map<CNetAddr, int64_t>::iterator i = mAskedUsForLMNodeList.find(pfrom->addr);
                if (i != mAskedUsForLMNodeList.end()){
                    int64_t t = (*i).second;
                    if (GetTime() < t) {
                        Misbehaving(pfrom->GetId(), 34);
                        LogPrintf("DSEG -- peer already asked me for the list, peer=%d\n", pfrom->id);
                        return;
                    }
                }
                int64_t askAgain = GetTime() + DSEG_UPDATE_SECONDS;
                mAskedUsForLMNodeList[pfrom->addr] = askAgain;
            }
        } //else, asking for a specific node which is ok

        int nInvCount = 0;

        BOOST_FOREACH(CLMNode& mn, vLMNodes) {
            if (vin != CTxIn() && vin != mn.vin) continue; // asked for specific vin but we are not there yet
            if (mn.addr.IsRFC1918() || mn.addr.IsLocal()) continue; // do not send local network lmnode
            if (mn.IsUpdateRequired()) continue; // do not send outdated lmnodes

            LogPrint("lmnode", "DSEG -- Sending LMNode entry: lmnode=%s  addr=%s\n", mn.vin.prevout.ToStringShort(), mn.addr.ToString());
            CLMNodeBroadcast mnb = CLMNodeBroadcast(mn);
            uint256 hash = mnb.GetHash();
            pfrom->PushInventory(CInv(MSG_LMNODE_ANNOUNCE, hash));
            pfrom->PushInventory(CInv(MSG_LMNODE_PING, mn.lastPing.GetHash()));
            nInvCount++;

            if (!mapSeenLMNodeBroadcast.count(hash)) {
                mapSeenLMNodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));
            }

            if (vin == mn.vin) {
                LogPrintf("DSEG -- Sent 1 LMNode inv to peer %d\n", pfrom->id);
                return;
            }
        }

        if(vin == CTxIn()) {
            pfrom->PushMessage(NetMsgType::SYNCSTATUSCOUNT, LMNODE_SYNC_LIST, nInvCount);
            LogPrintf("DSEG -- Sent %d LMNode invs to peer %d\n", nInvCount, pfrom->id);
            return;
        }
        // smth weird happen - someone asked us for vin we have no idea about?
        LogPrint("lmnode", "DSEG -- No invs sent to peer %d\n", pfrom->id);

    } else if (strCommand == NetMsgType::MNVERIFY) { // LMNode Verify

        // Need LOCK2 here to ensure consistent locking order because the all functions below call GetBlockHash which locks cs_main
        LOCK2(cs_main, cs);

        CLMNodeVerification mnv;
        vRecv >> mnv;

        if(mnv.vchSig1.empty()) {
            // CASE 1: someone asked me to verify myself /IP we are using/
            SendVerifyReply(pfrom, mnv);
        } else if (mnv.vchSig2.empty()) {
            // CASE 2: we _probably_ got verification we requested from some lmnode
            ProcessVerifyReply(pfrom, mnv);
        } else {
            // CASE 3: we _probably_ got verification broadcast signed by some lmnode which verified another one
            ProcessVerifyBroadcast(pfrom, mnv);
        }
    }
}

// Verification of lmnodes via unique direct requests.

void CLMNodeMan::DoFullVerificationStep()
{
    if(activeLMNode.vin == CTxIn()) return;
    if(!lmnodeSync.IsSynced()) return;

    std::vector<std::pair<int, CLMNode> > vecLMNodeRanks = GetLMNodeRanks(pCurrentBlockIndex->nHeight - 1, MIN_POSE_PROTO_VERSION);

    // Need LOCK2 here to ensure consistent locking order because the SendVerifyRequest call below locks cs_main
    // through GetHeight() signal in ConnectNode
    LOCK2(cs_main, cs);

    int nCount = 0;

    int nMyRank = -1;
    int nRanksTotal = (int)vecLMNodeRanks.size();

    // send verify requests only if we are in top MAX_POSE_RANK
    std::vector<std::pair<int, CLMNode> >::iterator it = vecLMNodeRanks.begin();
    while(it != vecLMNodeRanks.end()) {
        if(it->first > MAX_POSE_RANK) {
            LogPrint("lmnode", "CLMNodeMan::DoFullVerificationStep -- Must be in top %d to send verify request\n",
                        (int)MAX_POSE_RANK);
            return;
        }
        if(it->second.vin == activeLMNode.vin) {
            nMyRank = it->first;
            LogPrint("lmnode", "CLMNodeMan::DoFullVerificationStep -- Found self at rank %d/%d, verifying up to %d lmnodes\n",
                        nMyRank, nRanksTotal, (int)MAX_POSE_CONNECTIONS);
            break;
        }
        ++it;
    }

    // edge case: list is too short and this lmnode is not enabled
    if(nMyRank == -1) return;

    // send verify requests to up to MAX_POSE_CONNECTIONS lmnodes
    // starting from MAX_POSE_RANK + nMyRank and using MAX_POSE_CONNECTIONS as a step
    int nOffset = MAX_POSE_RANK + nMyRank - 1;
    if(nOffset >= (int)vecLMNodeRanks.size()) return;

    std::vector<CLMNode*> vSortedByAddr;
    BOOST_FOREACH(CLMNode& mn, vLMNodes) {
        vSortedByAddr.push_back(&mn);
    }

    sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

    it = vecLMNodeRanks.begin() + nOffset;
    while(it != vecLMNodeRanks.end()) {
        if(it->second.IsPoSeVerified() || it->second.IsPoSeBanned()) {
            LogPrint("lmnode", "CLMNodeMan::DoFullVerificationStep -- Already %s%s%s lmnode %s address %s, skipping...\n",
                        it->second.IsPoSeVerified() ? "verified" : "",
                        it->second.IsPoSeVerified() && it->second.IsPoSeBanned() ? " and " : "",
                        it->second.IsPoSeBanned() ? "banned" : "",
                        it->second.vin.prevout.ToStringShort(), it->second.addr.ToString());
            nOffset += MAX_POSE_CONNECTIONS;
            if(nOffset >= (int)vecLMNodeRanks.size()) break;
            it += MAX_POSE_CONNECTIONS;
            continue;
        }
        LogPrint("lmnode", "CLMNodeMan::DoFullVerificationStep -- Verifying lmnode %s rank %d/%d address %s\n",
                    it->second.vin.prevout.ToStringShort(), it->first, nRanksTotal, it->second.addr.ToString());
        if(SendVerifyRequest(CAddress(it->second.addr, NODE_NETWORK), vSortedByAddr)) {
            nCount++;
            if(nCount >= MAX_POSE_CONNECTIONS) break;
        }
        nOffset += MAX_POSE_CONNECTIONS;
        if(nOffset >= (int)vecLMNodeRanks.size()) break;
        it += MAX_POSE_CONNECTIONS;
    }

    LogPrint("lmnode", "CLMNodeMan::DoFullVerificationStep -- Sent verification requests to %d lmnodes\n", nCount);
}

// This function tries to find lmnodes with the same addr,
// find a verified one and ban all the other. If there are many nodes
// with the same addr but none of them is verified yet, then none of them are banned.
// It could take many times to run this before most of the duplicate nodes are banned.

void CLMNodeMan::CheckSameAddr()
{
    if(!lmnodeSync.IsSynced() || vLMNodes.empty()) return;

    std::vector<CLMNode*> vBan;
    std::vector<CLMNode*> vSortedByAddr;

    {
        LOCK(cs);

        CLMNode* pprevLMNode = NULL;
        CLMNode* pverifiedLMNode = NULL;

        BOOST_FOREACH(CLMNode& mn, vLMNodes) {
            vSortedByAddr.push_back(&mn);
        }

        sort(vSortedByAddr.begin(), vSortedByAddr.end(), CompareByAddr());

        BOOST_FOREACH(CLMNode* pmn, vSortedByAddr) {
            // check only (pre)enabled lmnodes
            if(!pmn->IsEnabled() && !pmn->IsPreEnabled()) continue;
            // initial step
            if(!pprevLMNode) {
                pprevLMNode = pmn;
                pverifiedLMNode = pmn->IsPoSeVerified() ? pmn : NULL;
                continue;
            }
            // second+ step
            if(pmn->addr == pprevLMNode->addr) {
                if(pverifiedLMNode) {
                    // another lmnode with the same ip is verified, ban this one
                    vBan.push_back(pmn);
                } else if(pmn->IsPoSeVerified()) {
                    // this lmnode with the same ip is verified, ban previous one
                    vBan.push_back(pprevLMNode);
                    // and keep a reference to be able to ban following lmnodes with the same ip
                    pverifiedLMNode = pmn;
                }
            } else {
                pverifiedLMNode = pmn->IsPoSeVerified() ? pmn : NULL;
            }
            pprevLMNode = pmn;
        }
    }

    // ban duplicates
    BOOST_FOREACH(CLMNode* pmn, vBan) {
        LogPrintf("CLMNodeMan::CheckSameAddr -- increasing PoSe ban score for lmnode %s\n", pmn->vin.prevout.ToStringShort());
        pmn->IncreasePoSeBanScore();
    }
}

bool CLMNodeMan::SendVerifyRequest(const CAddress& addr, const std::vector<CLMNode*>& vSortedByAddr)
{
    if(netfulfilledman.HasFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request")) {
        // we already asked for verification, not a good idea to do this too often, skip it
        LogPrint("lmnode", "CLMNodeMan::SendVerifyRequest -- too many requests, skipping... addr=%s\n", addr.ToString());
        return false;
    }

    CNode* pnode = ConnectNode(addr, NULL, false, true);
    if(pnode == NULL) {
        LogPrintf("CLMNodeMan::SendVerifyRequest -- can't connect to node to verify it, addr=%s\n", addr.ToString());
        return false;
    }

    netfulfilledman.AddFulfilledRequest(addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request");
    // use random nonce, store it and require node to reply with correct one later
    CLMNodeVerification mnv(addr, GetRandInt(999999), pCurrentBlockIndex->nHeight - 1);
    mWeAskedForVerification[addr] = mnv;
    LogPrintf("CLMNodeMan::SendVerifyRequest -- verifying node using nonce %d addr=%s\n", mnv.nonce, addr.ToString());
    pnode->PushMessage(NetMsgType::MNVERIFY, mnv);

    return true;
}

void CLMNodeMan::SendVerifyReply(CNode* pnode, CLMNodeVerification& mnv)
{
    // only lmnodes can sign this, why would someone ask regular node?
    if(!fZNode) {
        // do not ban, malicious node might be using my IP
        // and trying to confuse the node which tries to verify it
        return;
    }

    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply")) {
//        // peer should not ask us that often
        LogPrintf("LMNodeMan::SendVerifyReply -- ERROR: peer already asked me recently, peer=%d\n", pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        LogPrintf("LMNodeMan::SendVerifyReply -- can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

    std::string strMessage = strprintf("%s%d%s", activeLMNode.service.ToString(), mnv.nonce, blockHash.ToString());

    if(!darkSendSigner.SignMessage(strMessage, mnv.vchSig1, activeLMNode.keyLMNode)) {
        LogPrintf("LMNodeMan::SendVerifyReply -- SignMessage() failed\n");
        return;
    }

    std::string strError;

    if(!darkSendSigner.VerifyMessage(activeLMNode.pubKeyLMNode, mnv.vchSig1, strMessage, strError)) {
        LogPrintf("LMNodeMan::SendVerifyReply -- VerifyMessage() failed, error: %s\n", strError);
        return;
    }

    pnode->PushMessage(NetMsgType::MNVERIFY, mnv);
    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-reply");
}

void CLMNodeMan::ProcessVerifyReply(CNode* pnode, CLMNodeVerification& mnv)
{
    std::string strError;

    // did we even ask for it? if that's the case we should have matching fulfilled request
    if(!netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-request")) {
        LogPrintf("CLMNodeMan::ProcessVerifyReply -- ERROR: we didn't ask for verification of %s, peer=%d\n", pnode->addr.ToString(), pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    // Received nonce for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nonce != mnv.nonce) {
        LogPrintf("CLMNodeMan::ProcessVerifyReply -- ERROR: wrong nounce: requested=%d, received=%d, peer=%d\n",
                    mWeAskedForVerification[pnode->addr].nonce, mnv.nonce, pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    // Received nBlockHeight for a known address must match the one we sent
    if(mWeAskedForVerification[pnode->addr].nBlockHeight != mnv.nBlockHeight) {
        LogPrintf("CLMNodeMan::ProcessVerifyReply -- ERROR: wrong nBlockHeight: requested=%d, received=%d, peer=%d\n",
                    mWeAskedForVerification[pnode->addr].nBlockHeight, mnv.nBlockHeight, pnode->id);
        Misbehaving(pnode->id, 20);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        // this shouldn't happen...
        LogPrintf("LMNodeMan::ProcessVerifyReply -- can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

//    // we already verified this address, why node is spamming?
    if(netfulfilledman.HasFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done")) {
        LogPrintf("CLMNodeMan::ProcessVerifyReply -- ERROR: already verified %s recently\n", pnode->addr.ToString());
        Misbehaving(pnode->id, 20);
        return;
    }

    {
        LOCK(cs);

        CLMNode* prealLMNode = NULL;
        std::vector<CLMNode*> vpLMNodesToBan;
        std::vector<CLMNode>::iterator it = vLMNodes.begin();
        std::string strMessage1 = strprintf("%s%d%s", pnode->addr.ToString(), mnv.nonce, blockHash.ToString());
        while(it != vLMNodes.end()) {
            if(CAddress(it->addr, NODE_NETWORK) == pnode->addr) {
                if(darkSendSigner.VerifyMessage(it->pubKeyLMNode, mnv.vchSig1, strMessage1, strError)) {
                    // found it!
                    prealLMNode = &(*it);
                    if(!it->IsPoSeVerified()) {
                        it->DecreasePoSeBanScore();
                    }
                    netfulfilledman.AddFulfilledRequest(pnode->addr, strprintf("%s", NetMsgType::MNVERIFY)+"-done");

                    // we can only broadcast it if we are an activated lmnode
                    if(activeLMNode.vin == CTxIn()) continue;
                    // update ...
                    mnv.addr = it->addr;
                    mnv.vin1 = it->vin;
                    mnv.vin2 = activeLMNode.vin;
                    std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString(),
                                            mnv.vin1.prevout.ToStringShort(), mnv.vin2.prevout.ToStringShort());
                    // ... and sign it
                    if(!darkSendSigner.SignMessage(strMessage2, mnv.vchSig2, activeLMNode.keyLMNode)) {
                        LogPrintf("LMNodeMan::ProcessVerifyReply -- SignMessage() failed\n");
                        return;
                    }

                    std::string strError;

                    if(!darkSendSigner.VerifyMessage(activeLMNode.pubKeyLMNode, mnv.vchSig2, strMessage2, strError)) {
                        LogPrintf("LMNodeMan::ProcessVerifyReply -- VerifyMessage() failed, error: %s\n", strError);
                        return;
                    }

                    mWeAskedForVerification[pnode->addr] = mnv;
                    mnv.Relay();

                } else {
                    vpLMNodesToBan.push_back(&(*it));
                }
            }
            ++it;
        }
        // no real lmnode found?...
        if(!prealLMNode) {
            // this should never be the case normally,
            // only if someone is trying to game the system in some way or smth like that
            LogPrintf("CLMNodeMan::ProcessVerifyReply -- ERROR: no real lmnode found for addr %s\n", pnode->addr.ToString());
            Misbehaving(pnode->id, 20);
            return;
        }
        LogPrintf("CLMNodeMan::ProcessVerifyReply -- verified real lmnode %s for addr %s\n",
                    prealLMNode->vin.prevout.ToStringShort(), pnode->addr.ToString());
        // increase ban score for everyone else
        BOOST_FOREACH(CLMNode* pmn, vpLMNodesToBan) {
            pmn->IncreasePoSeBanScore();
            LogPrint("lmnode", "CLMNodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        prealLMNode->vin.prevout.ToStringShort(), pnode->addr.ToString(), pmn->nPoSeBanScore);
        }
        LogPrintf("CLMNodeMan::ProcessVerifyBroadcast -- PoSe score increased for %d fake lmnodes, addr %s\n",
                    (int)vpLMNodesToBan.size(), pnode->addr.ToString());
    }
}

void CLMNodeMan::ProcessVerifyBroadcast(CNode* pnode, const CLMNodeVerification& mnv)
{
    std::string strError;

    if(mapSeenLMNodeVerification.find(mnv.GetHash()) != mapSeenLMNodeVerification.end()) {
        // we already have one
        return;
    }
    mapSeenLMNodeVerification[mnv.GetHash()] = mnv;

    // we don't care about history
    if(mnv.nBlockHeight < pCurrentBlockIndex->nHeight - MAX_POSE_BLOCKS) {
        LogPrint("lmnode", "LMNodeMan::ProcessVerifyBroadcast -- Outdated: current block %d, verification block %d, peer=%d\n",
                    pCurrentBlockIndex->nHeight, mnv.nBlockHeight, pnode->id);
        return;
    }

    if(mnv.vin1.prevout == mnv.vin2.prevout) {
        LogPrint("lmnode", "LMNodeMan::ProcessVerifyBroadcast -- ERROR: same vins %s, peer=%d\n",
                    mnv.vin1.prevout.ToStringShort(), pnode->id);
        // that was NOT a good idea to cheat and verify itself,
        // ban the node we received such message from
        Misbehaving(pnode->id, 100);
        return;
    }

    uint256 blockHash;
    if(!GetBlockHash(blockHash, mnv.nBlockHeight)) {
        // this shouldn't happen...
        LogPrintf("LMNodeMan::ProcessVerifyBroadcast -- Can't get block hash for unknown block height %d, peer=%d\n", mnv.nBlockHeight, pnode->id);
        return;
    }

    int nRank = GetLMNodeRank(mnv.vin2, mnv.nBlockHeight, MIN_POSE_PROTO_VERSION);

    if (nRank == -1) {
        LogPrint("lmnode", "CLMNodeMan::ProcessVerifyBroadcast -- Can't calculate rank for lmnode %s\n",
                    mnv.vin2.prevout.ToStringShort());
        return;
    }

    if(nRank > MAX_POSE_RANK) {
        LogPrint("lmnode", "CLMNodeMan::ProcessVerifyBroadcast -- Mastrernode %s is not in top %d, current rank %d, peer=%d\n",
                    mnv.vin2.prevout.ToStringShort(), (int)MAX_POSE_RANK, nRank, pnode->id);
        return;
    }

    {
        LOCK(cs);

        std::string strMessage1 = strprintf("%s%d%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString());
        std::string strMessage2 = strprintf("%s%d%s%s%s", mnv.addr.ToString(), mnv.nonce, blockHash.ToString(),
                                mnv.vin1.prevout.ToStringShort(), mnv.vin2.prevout.ToStringShort());

        CLMNode* pmn1 = Find(mnv.vin1);
        if(!pmn1) {
            LogPrintf("CLMNodeMan::ProcessVerifyBroadcast -- can't find lmnode1 %s\n", mnv.vin1.prevout.ToStringShort());
            return;
        }

        CLMNode* pmn2 = Find(mnv.vin2);
        if(!pmn2) {
            LogPrintf("CLMNodeMan::ProcessVerifyBroadcast -- can't find lmnode2 %s\n", mnv.vin2.prevout.ToStringShort());
            return;
        }

        if(pmn1->addr != mnv.addr) {
            LogPrintf("CLMNodeMan::ProcessVerifyBroadcast -- addr %s do not match %s\n", mnv.addr.ToString(), pnode->addr.ToString());
            return;
        }

        if(darkSendSigner.VerifyMessage(pmn1->pubKeyLMNode, mnv.vchSig1, strMessage1, strError)) {
            LogPrintf("LMNodeMan::ProcessVerifyBroadcast -- VerifyMessage() for lmnode1 failed, error: %s\n", strError);
            return;
        }

        if(darkSendSigner.VerifyMessage(pmn2->pubKeyLMNode, mnv.vchSig2, strMessage2, strError)) {
            LogPrintf("LMNodeMan::ProcessVerifyBroadcast -- VerifyMessage() for lmnode2 failed, error: %s\n", strError);
            return;
        }

        if(!pmn1->IsPoSeVerified()) {
            pmn1->DecreasePoSeBanScore();
        }
        mnv.Relay();

        LogPrintf("CLMNodeMan::ProcessVerifyBroadcast -- verified lmnode %s for addr %s\n",
                    pmn1->vin.prevout.ToStringShort(), pnode->addr.ToString());

        // increase ban score for everyone else with the same addr
        int nCount = 0;
        BOOST_FOREACH(CLMNode& mn, vLMNodes) {
            if(mn.addr != mnv.addr || mn.vin.prevout == mnv.vin1.prevout) continue;
            mn.IncreasePoSeBanScore();
            nCount++;
            LogPrint("lmnode", "CLMNodeMan::ProcessVerifyBroadcast -- increased PoSe ban score for %s addr %s, new score %d\n",
                        mn.vin.prevout.ToStringShort(), mn.addr.ToString(), mn.nPoSeBanScore);
        }
        LogPrintf("CLMNodeMan::ProcessVerifyBroadcast -- PoSe score incresed for %d fake lmnodes, addr %s\n",
                    nCount, pnode->addr.ToString());
    }
}

std::string CLMNodeMan::ToString() const
{
    std::ostringstream info;

    info << "LMNodes: " << (int)vLMNodes.size() <<
            ", peers who asked us for LMNode list: " << (int)mAskedUsForLMNodeList.size() <<
            ", peers we asked for LMNode list: " << (int)mWeAskedForLMNodeList.size() <<
            ", entries in LMNode list we asked for: " << (int)mWeAskedForLMNodeListEntry.size() <<
            ", lmnode index size: " << indexLMNodes.GetSize() <<
            ", nDsqCount: " << (int)nDsqCount;

    return info.str();
}

void CLMNodeMan::UpdateLMNodeList(CLMNodeBroadcast mnb)
{
    try {
        LogPrintf("CLMNodeMan::UpdateLMNodeList\n");
        LOCK2(cs_main, cs);
        mapSeenLMNodePing.insert(std::make_pair(mnb.lastPing.GetHash(), mnb.lastPing));
        mapSeenLMNodeBroadcast.insert(std::make_pair(mnb.GetHash(), std::make_pair(GetTime(), mnb)));

        LogPrintf("CLMNodeMan::UpdateLMNodeList -- lmnode=%s  addr=%s\n", mnb.vin.prevout.ToStringShort(), mnb.addr.ToString());

        CLMNode *pmn = Find(mnb.vin);
        if (pmn == NULL) {
            CLMNode mn(mnb);
            if (Add(mn)) {
                lmnodeSync.AddedLMNodeList();
            }
        } else {
            CLMNodeBroadcast mnbOld = mapSeenLMNodeBroadcast[CLMNodeBroadcast(*pmn).GetHash()].second;
            if (pmn->UpdateFromNewBroadcast(mnb)) {
                lmnodeSync.AddedLMNodeList();
                mapSeenLMNodeBroadcast.erase(mnbOld.GetHash());
            }
        }
    } catch (const std::exception &e) {
        PrintExceptionContinue(&e, "UpdateLMNodeList");
    }
}

bool CLMNodeMan::CheckMnbAndUpdateLMNodeList(CNode* pfrom, CLMNodeBroadcast mnb, int& nDos)
{
    // Need LOCK2 here to ensure consistent locking order because the SimpleCheck call below locks cs_main
    LOCK(cs_main);

    {
        LOCK(cs);
        nDos = 0;
        LogPrint("lmnode", "CLMNodeMan::CheckMnbAndUpdateLMNodeList -- lmnode=%s\n", mnb.vin.prevout.ToStringShort());

        uint256 hash = mnb.GetHash();
        if (mapSeenLMNodeBroadcast.count(hash) && !mnb.fRecovery) { //seen
            LogPrint("lmnode", "CLMNodeMan::CheckMnbAndUpdateLMNodeList -- lmnode=%s seen\n", mnb.vin.prevout.ToStringShort());
            // less then 2 pings left before this MN goes into non-recoverable state, bump sync timeout
            if (GetTime() - mapSeenLMNodeBroadcast[hash].first > LMNODE_NEW_START_REQUIRED_SECONDS - LMNODE_MIN_MNP_SECONDS * 2) {
                LogPrint("lmnode", "CLMNodeMan::CheckMnbAndUpdateLMNodeList -- lmnode=%s seen update\n", mnb.vin.prevout.ToStringShort());
                mapSeenLMNodeBroadcast[hash].first = GetTime();
                lmnodeSync.AddedLMNodeList();
            }
            // did we ask this node for it?
            if (pfrom && IsMnbRecoveryRequested(hash) && GetTime() < mMnbRecoveryRequests[hash].first) {
                LogPrint("lmnode", "CLMNodeMan::CheckMnbAndUpdateLMNodeList -- mnb=%s seen request\n", hash.ToString());
                if (mMnbRecoveryRequests[hash].second.count(pfrom->addr)) {
                    LogPrint("lmnode", "CLMNodeMan::CheckMnbAndUpdateLMNodeList -- mnb=%s seen request, addr=%s\n", hash.ToString(), pfrom->addr.ToString());
                    // do not allow node to send same mnb multiple times in recovery mode
                    mMnbRecoveryRequests[hash].second.erase(pfrom->addr);
                    // does it have newer lastPing?
                    if (mnb.lastPing.sigTime > mapSeenLMNodeBroadcast[hash].second.lastPing.sigTime) {
                        // simulate Check
                        CLMNode mnTemp = CLMNode(mnb);
                        mnTemp.Check();
                        LogPrint("lmnode", "CLMNodeMan::CheckMnbAndUpdateLMNodeList -- mnb=%s seen request, addr=%s, better lastPing: %d min ago, projected mn state: %s\n", hash.ToString(), pfrom->addr.ToString(), (GetTime() - mnb.lastPing.sigTime) / 60, mnTemp.GetStateString());
                        if (mnTemp.IsValidStateForAutoStart(mnTemp.nActiveState)) {
                            // this node thinks it's a good one
                            LogPrint("lmnode", "CLMNodeMan::CheckMnbAndUpdateLMNodeList -- lmnode=%s seen good\n", mnb.vin.prevout.ToStringShort());
                            mMnbRecoveryGoodReplies[hash].push_back(mnb);
                        }
                    }
                }
            }
            return true;
        }
        mapSeenLMNodeBroadcast.insert(std::make_pair(hash, std::make_pair(GetTime(), mnb)));

        LogPrint("lmnode", "CLMNodeMan::CheckMnbAndUpdateLMNodeList -- lmnode=%s new\n", mnb.vin.prevout.ToStringShort());

        if (!mnb.SimpleCheck(nDos)) {
            LogPrint("lmnode", "CLMNodeMan::CheckMnbAndUpdateLMNodeList -- SimpleCheck() failed, lmnode=%s\n", mnb.vin.prevout.ToStringShort());
            return false;
        }

        // search LMNode list
        CLMNode *pmn = Find(mnb.vin);
        if (pmn) {
            CLMNodeBroadcast mnbOld = mapSeenLMNodeBroadcast[CLMNodeBroadcast(*pmn).GetHash()].second;
            if (!mnb.Update(pmn, nDos)) {
                LogPrint("lmnode", "CLMNodeMan::CheckMnbAndUpdateLMNodeList -- Update() failed, lmnode=%s\n", mnb.vin.prevout.ToStringShort());
                return false;
            }
            if (hash != mnbOld.GetHash()) {
                mapSeenLMNodeBroadcast.erase(mnbOld.GetHash());
            }
        }
    } // end of LOCK(cs);

    if(mnb.CheckOutpoint(nDos)) {
        Add(mnb);
        lmnodeSync.AddedLMNodeList();
        // if it matches our LMNode privkey...
        if(fZNode && mnb.pubKeyLMNode == activeLMNode.pubKeyLMNode) {
            mnb.nPoSeBanScore = -LMNODE_POSE_BAN_MAX_SCORE;
            if(mnb.nProtocolVersion == PROTOCOL_VERSION) {
                // ... and PROTOCOL_VERSION, then we've been remotely activated ...
                LogPrintf("CLMNodeMan::CheckMnbAndUpdateLMNodeList -- Got NEW LMNode entry: lmnode=%s  sigTime=%lld  addr=%s\n",
                            mnb.vin.prevout.ToStringShort(), mnb.sigTime, mnb.addr.ToString());
                activeLMNode.ManageState();
            } else {
                // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
                // but also do not ban the node we get this message from
                LogPrintf("CLMNodeMan::CheckMnbAndUpdateLMNodeList -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", mnb.nProtocolVersion, PROTOCOL_VERSION);
                return false;
            }
        }
        mnb.RelayZNode();
    } else {
        LogPrintf("CLMNodeMan::CheckMnbAndUpdateLMNodeList -- Rejected LMNode entry: %s  addr=%s\n", mnb.vin.prevout.ToStringShort(), mnb.addr.ToString());
        return false;
    }

    return true;
}

void CLMNodeMan::UpdateLastPaid()
{
    LOCK(cs);
    if(fLiteMode) return;
    if(!pCurrentBlockIndex) {
        // LogPrintf("CLMNodeMan::UpdateLastPaid, pCurrentBlockIndex=NULL\n");
        return;
    }

    static bool IsFirstRun = true;
    // Do full scan on first run or if we are not a lmnode
    // (MNs should update this info on every block, so limited scan should be enough for them)
    int nMaxBlocksToScanBack = (IsFirstRun || !fZNode) ? mnpayments.GetStorageLimit() : LAST_PAID_SCAN_BLOCKS;

    LogPrint("mnpayments", "CLMNodeMan::UpdateLastPaid -- nHeight=%d, nMaxBlocksToScanBack=%d, IsFirstRun=%s\n",
                             pCurrentBlockIndex->nHeight, nMaxBlocksToScanBack, IsFirstRun ? "true" : "false");

    BOOST_FOREACH(CLMNode& mn, vLMNodes) {
        mn.UpdateLastPaid(pCurrentBlockIndex, nMaxBlocksToScanBack);
    }

    // every time is like the first time if winners list is not synced
    IsFirstRun = !lmnodeSync.IsWinnersListSynced();
}

void CLMNodeMan::CheckAndRebuildLMNodeIndex()
{
    LOCK(cs);

    if(GetTime() - nLastIndexRebuildTime < MIN_INDEX_REBUILD_TIME) {
        return;
    }

    if(indexLMNodes.GetSize() <= MAX_EXPECTED_INDEX_SIZE) {
        return;
    }

    if(indexLMNodes.GetSize() <= int(vLMNodes.size())) {
        return;
    }

    indexLMNodesOld = indexLMNodes;
    indexLMNodes.Clear();
    for(size_t i = 0; i < vLMNodes.size(); ++i) {
        indexLMNodes.AddLMNodeVIN(vLMNodes[i].vin);
    }

    fIndexRebuilt = true;
    nLastIndexRebuildTime = GetTime();
}

void CLMNodeMan::UpdateWatchdogVoteTime(const CTxIn& vin)
{
    LOCK(cs);
    CLMNode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->UpdateWatchdogVoteTime();
    nLastWatchdogVoteTime = GetTime();
}

bool CLMNodeMan::IsWatchdogActive()
{
    LOCK(cs);
    // Check if any lmnodes have voted recently, otherwise return false
    return (GetTime() - nLastWatchdogVoteTime) <= LMNODE_WATCHDOG_MAX_SECONDS;
}

void CLMNodeMan::CheckLMNode(const CTxIn& vin, bool fForce)
{
    LOCK(cs);
    CLMNode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

void CLMNodeMan::CheckLMNode(const CPubKey& pubKeyLMNode, bool fForce)
{
    LOCK(cs);
    CLMNode* pMN = Find(pubKeyLMNode);
    if(!pMN)  {
        return;
    }
    pMN->Check(fForce);
}

int CLMNodeMan::GetLMNodeState(const CTxIn& vin)
{
    LOCK(cs);
    CLMNode* pMN = Find(vin);
    if(!pMN)  {
        return CLMNode::LMNODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

int CLMNodeMan::GetLMNodeState(const CPubKey& pubKeyLMNode)
{
    LOCK(cs);
    CLMNode* pMN = Find(pubKeyLMNode);
    if(!pMN)  {
        return CLMNode::LMNODE_NEW_START_REQUIRED;
    }
    return pMN->nActiveState;
}

bool CLMNodeMan::IsLMNodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt)
{
    LOCK(cs);
    CLMNode* pMN = Find(vin);
    if(!pMN) {
        return false;
    }
    return pMN->IsPingedWithin(nSeconds, nTimeToCheckAt);
}

void CLMNodeMan::SetLMNodeLastPing(const CTxIn& vin, const CLMNodePing& mnp)
{
    LOCK(cs);
    CLMNode* pMN = Find(vin);
    if(!pMN)  {
        return;
    }
    pMN->lastPing = mnp;
    mapSeenLMNodePing.insert(std::make_pair(mnp.GetHash(), mnp));

    CLMNodeBroadcast mnb(*pMN);
    uint256 hash = mnb.GetHash();
    if(mapSeenLMNodeBroadcast.count(hash)) {
        mapSeenLMNodeBroadcast[hash].second.lastPing = mnp;
    }
}

void CLMNodeMan::UpdatedBlockTip(const CBlockIndex *pindex)
{
    pCurrentBlockIndex = pindex;
    LogPrint("lmnode", "CLMNodeMan::UpdatedBlockTip -- pCurrentBlockIndex->nHeight=%d\n", pCurrentBlockIndex->nHeight);

    CheckSameAddr();

    if(fZNode) {
        // normal wallet does not need to update this every block, doing update on rpc call should be enough
        UpdateLastPaid();
    }
}

void CLMNodeMan::NotifyLMNodeUpdates()
{
    // Avoid double locking
    bool fLMNodesAddedLocal = false;
    bool fLMNodesRemovedLocal = false;
    {
        LOCK(cs);
        fLMNodesAddedLocal = fLMNodesAdded;
        fLMNodesRemovedLocal = fLMNodesRemoved;
    }

    if(fLMNodesAddedLocal) {
//        governance.CheckLMNodeOrphanObjects();
//        governance.CheckLMNodeOrphanVotes();
    }
    if(fLMNodesRemovedLocal) {
//        governance.UpdateCachesAndClean();
    }

    LOCK(cs);
    fLMNodesAdded = false;
    fLMNodesRemoved = false;
}
