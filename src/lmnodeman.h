// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2016-2017 The Zcoin developers
// Copyright (c) 2017-2018 The Hppcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef LMNODEMAN_H
#define LMNODEMAN_H

#include "lmnode.h"
#include "sync.h"

using namespace std;

class CLMNodeMan;

extern CLMNodeMan mnodeman;

/**
 * Provides a forward and reverse index between MN vin's and integers.
 *
 * This mapping is normally add-only and is expected to be permanent
 * It is only rebuilt if the size of the index exceeds the expected maximum number
 * of MN's and the current number of known MN's.
 *
 * The external interface to this index is provided via delegation by CLMNodeMan
 */
class CLMNodeIndex
{
public: // Types
    typedef std::map<CTxIn,int> index_m_t;

    typedef index_m_t::iterator index_m_it;

    typedef index_m_t::const_iterator index_m_cit;

    typedef std::map<int,CTxIn> rindex_m_t;

    typedef rindex_m_t::iterator rindex_m_it;

    typedef rindex_m_t::const_iterator rindex_m_cit;

private:
    int                  nSize;

    index_m_t            mapIndex;

    rindex_m_t           mapReverseIndex;

public:
    CLMNodeIndex();

    int GetSize() const {
        return nSize;
    }

    /// Retrieve lmnode vin by index
    bool Get(int nIndex, CTxIn& vinLMNode) const;

    /// Get index of a lmnode vin
    int GetLMNodeIndex(const CTxIn& vinLMNode) const;

    void AddLMNodeVIN(const CTxIn& vinLMNode);

    void Clear();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion)
    {
        READWRITE(mapIndex);
        if(ser_action.ForRead()) {
            RebuildIndex();
        }
    }

private:
    void RebuildIndex();

};

class CLMNodeMan
{
public:
    typedef std::map<CTxIn,int> index_m_t;

    typedef index_m_t::iterator index_m_it;

    typedef index_m_t::const_iterator index_m_cit;

private:
    static const int MAX_EXPECTED_INDEX_SIZE = 30000;

    /// Only allow 1 index rebuild per hour
    static const int64_t MIN_INDEX_REBUILD_TIME = 3600;

    static const std::string SERIALIZATION_VERSION_STRING;

    static const int DSEG_UPDATE_SECONDS        = 3 * 60 * 60;

    static const int LAST_PAID_SCAN_BLOCKS      = 100;

    static const int MIN_POSE_PROTO_VERSION     = 70203;
    static const int MAX_POSE_CONNECTIONS       = 10;
    static const int MAX_POSE_RANK              = 10;
    static const int MAX_POSE_BLOCKS            = 10;

    static const int MNB_RECOVERY_QUORUM_TOTAL      = 10;
    static const int MNB_RECOVERY_QUORUM_REQUIRED   = 6;
    static const int MNB_RECOVERY_MAX_ASK_ENTRIES   = 10;
    static const int MNB_RECOVERY_WAIT_SECONDS      = 60;
    static const int MNB_RECOVERY_RETRY_SECONDS     = 3 * 60 * 60;


    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    // Keep track of current block index
    const CBlockIndex *pCurrentBlockIndex;

    // map to hold all MNs
    std::vector<CLMNode> vLMNodes;
    // who's asked for the LMNode list and the last time
    std::map<CNetAddr, int64_t> mAskedUsForLMNodeList;
    // who we asked for the LMNode list and the last time
    std::map<CNetAddr, int64_t> mWeAskedForLMNodeList;
    // which LMNodes we've asked for
    std::map<COutPoint, std::map<CNetAddr, int64_t> > mWeAskedForLMNodeListEntry;
    // who we asked for the lmnode verification
    std::map<CNetAddr, CLMNodeVerification> mWeAskedForVerification;

    // these maps are used for lmnode recovery from LMNODE_NEW_START_REQUIRED state
    std::map<uint256, std::pair< int64_t, std::set<CNetAddr> > > mMnbRecoveryRequests;
    std::map<uint256, std::vector<CLMNodeBroadcast> > mMnbRecoveryGoodReplies;
    std::list< std::pair<CService, uint256> > listScheduledMnbRequestConnections;

    int64_t nLastIndexRebuildTime;

    CLMNodeIndex indexLMNodes;

    CLMNodeIndex indexLMNodesOld;

    /// Set when index has been rebuilt, clear when read
    bool fIndexRebuilt;

    /// Set when lmnodes are added, cleared when CGovernanceManager is notified
    bool fLMNodesAdded;

    /// Set when lmnodes are removed, cleared when CGovernanceManager is notified
    bool fLMNodesRemoved;

    std::vector<uint256> vecDirtyGovernanceObjectHashes;

    int64_t nLastWatchdogVoteTime;

    friend class CLMNodeSync;

public:
    // Keep track of all broadcasts I've seen
    std::map<uint256, std::pair<int64_t, CLMNodeBroadcast> > mapSeenLMNodeBroadcast;
    // Keep track of all pings I've seen
    std::map<uint256, CLMNodePing> mapSeenLMNodePing;
    // Keep track of all verifications I've seen
    std::map<uint256, CLMNodeVerification> mapSeenLMNodeVerification;
    // keep track of dsq count to prevent lmnodes from gaming darksend queue
    int64_t nDsqCount;


    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action, int nType, int nVersion) {
        LOCK(cs);
        std::string strVersion;
        if(ser_action.ForRead()) {
            READWRITE(strVersion);
        }
        else {
            strVersion = SERIALIZATION_VERSION_STRING; 
            READWRITE(strVersion);
        }

        READWRITE(vLMNodes);
        READWRITE(mAskedUsForLMNodeList);
        READWRITE(mWeAskedForLMNodeList);
        READWRITE(mWeAskedForLMNodeListEntry);
        READWRITE(mMnbRecoveryRequests);
        READWRITE(mMnbRecoveryGoodReplies);
        READWRITE(nLastWatchdogVoteTime);
        READWRITE(nDsqCount);

        READWRITE(mapSeenLMNodeBroadcast);
        READWRITE(mapSeenLMNodePing);
        READWRITE(indexLMNodes);
        if(ser_action.ForRead() && (strVersion != SERIALIZATION_VERSION_STRING)) {
            Clear();
        }
    }

    CLMNodeMan();

    /// Add an entry
    bool Add(CLMNode &mn);

    /// Ask (source) node for mnb
    void AskForMN(CNode *pnode, const CTxIn &vin);
    void AskForMnb(CNode *pnode, const uint256 &hash);

    /// Check all LMNodes
    void Check();

    /// Check all LMNodes and remove inactive
    void CheckAndRemove();

    /// Clear LMNode vector
    void Clear();

    /// Count LMNodes filtered by nProtocolVersion.
    /// LMNode nProtocolVersion should match or be above the one specified in param here.
    int CountLMNodes(int nProtocolVersion = -1);
    /// Count enabled LMNodes filtered by nProtocolVersion.
    /// LMNode nProtocolVersion should match or be above the one specified in param here.
    int CountEnabled(int nProtocolVersion = -1);

    /// Count LMNodes by network type - NET_IPV4, NET_IPV6, NET_TOR
    // int CountByIP(int nNetworkType);

    void DsegUpdate(CNode* pnode);

    /// Find an entry
    CLMNode* Find(const CScript &payee);
    CLMNode* Find(const CTxIn& vin);
    CLMNode* Find(const CPubKey& pubKeyLMNode);

    /// Versions of Find that are safe to use from outside the class
    bool Get(const CPubKey& pubKeyLMNode, CLMNode& lmnode);
    bool Get(const CTxIn& vin, CLMNode& lmnode);

    /// Retrieve lmnode vin by index
    bool Get(int nIndex, CTxIn& vinLMNode, bool& fIndexRebuiltOut) {
        LOCK(cs);
        fIndexRebuiltOut = fIndexRebuilt;
        return indexLMNodes.Get(nIndex, vinLMNode);
    }

    bool GetIndexRebuiltFlag() {
        LOCK(cs);
        return fIndexRebuilt;
    }

    /// Get index of a lmnode vin
    int GetLMNodeIndex(const CTxIn& vinLMNode) {
        LOCK(cs);
        return indexLMNodes.GetLMNodeIndex(vinLMNode);
    }

    /// Get old index of a lmnode vin
    int GetLMNodeIndexOld(const CTxIn& vinLMNode) {
        LOCK(cs);
        return indexLMNodesOld.GetLMNodeIndex(vinLMNode);
    }

    /// Get lmnode VIN for an old index value
    bool GetLMNodeVinForIndexOld(int nLMNodeIndex, CTxIn& vinLMNodeOut) {
        LOCK(cs);
        return indexLMNodesOld.Get(nLMNodeIndex, vinLMNodeOut);
    }

    /// Get index of a lmnode vin, returning rebuild flag
    int GetLMNodeIndex(const CTxIn& vinLMNode, bool& fIndexRebuiltOut) {
        LOCK(cs);
        fIndexRebuiltOut = fIndexRebuilt;
        return indexLMNodes.GetLMNodeIndex(vinLMNode);
    }

    void ClearOldLMNodeIndex() {
        LOCK(cs);
        indexLMNodesOld.Clear();
        fIndexRebuilt = false;
    }

    bool Has(const CTxIn& vin);

    lmnode_info_t GetLMNodeInfo(const CTxIn& vin);

    lmnode_info_t GetLMNodeInfo(const CPubKey& pubKeyLMNode);

    char* GetNotQualifyReason(CLMNode& mn, int nBlockHeight, bool fFilterSigTime, int nMnCount);

    /// Find an entry in the lmnode list that is next to be paid
    CLMNode* GetNextLMNodeInQueueForPayment(int nBlockHeight, bool fFilterSigTime, int& nCount);
    /// Same as above but use current block height
    CLMNode* GetNextLMNodeInQueueForPayment(bool fFilterSigTime, int& nCount);

    /// Find a random entry
    CLMNode* FindRandomNotInVec(const std::vector<CTxIn> &vecToExclude, int nProtocolVersion = -1);

    std::vector<CLMNode> GetFullLMNodeVector() { return vLMNodes; }

    std::vector<std::pair<int, CLMNode> > GetLMNodeRanks(int nBlockHeight = -1, int nMinProtocol=0);
    int GetLMNodeRank(const CTxIn &vin, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);
    CLMNode* GetLMNodeByRank(int nRank, int nBlockHeight, int nMinProtocol=0, bool fOnlyActive=true);

    void ProcessLMNodeConnections();
    std::pair<CService, std::set<uint256> > PopScheduledMnbRequestConnection();

    void ProcessMessage(CNode* pfrom, std::string& strCommand, CDataStream& vRecv);

    void DoFullVerificationStep();
    void CheckSameAddr();
    bool SendVerifyRequest(const CAddress& addr, const std::vector<CLMNode*>& vSortedByAddr);
    void SendVerifyReply(CNode* pnode, CLMNodeVerification& mnv);
    void ProcessVerifyReply(CNode* pnode, CLMNodeVerification& mnv);
    void ProcessVerifyBroadcast(CNode* pnode, const CLMNodeVerification& mnv);

    /// Return the number of (unique) LMNodes
    int size() { return vLMNodes.size(); }

    std::string ToString() const;

    /// Update lmnode list and maps using provided CLMNodeBroadcast
    void UpdateLMNodeList(CLMNodeBroadcast mnb);
    /// Perform complete check and only then update list and maps
    bool CheckMnbAndUpdateLMNodeList(CNode* pfrom, CLMNodeBroadcast mnb, int& nDos);
    bool IsMnbRecoveryRequested(const uint256& hash) { return mMnbRecoveryRequests.count(hash); }

    void UpdateLastPaid();

    void CheckAndRebuildLMNodeIndex();

    void AddDirtyGovernanceObjectHash(const uint256& nHash)
    {
        LOCK(cs);
        vecDirtyGovernanceObjectHashes.push_back(nHash);
    }

    std::vector<uint256> GetAndClearDirtyGovernanceObjectHashes()
    {
        LOCK(cs);
        std::vector<uint256> vecTmp = vecDirtyGovernanceObjectHashes;
        vecDirtyGovernanceObjectHashes.clear();
        return vecTmp;;
    }

    bool IsWatchdogActive();
    void UpdateWatchdogVoteTime(const CTxIn& vin);
    bool AddGovernanceVote(const CTxIn& vin, uint256 nGovernanceObjectHash);
    void RemoveGovernanceObject(uint256 nGovernanceObjectHash);

    void CheckLMNode(const CTxIn& vin, bool fForce = false);
    void CheckLMNode(const CPubKey& pubKeyLMNode, bool fForce = false);

    int GetLMNodeState(const CTxIn& vin);
    int GetLMNodeState(const CPubKey& pubKeyLMNode);

    bool IsLMNodePingedWithin(const CTxIn& vin, int nSeconds, int64_t nTimeToCheckAt = -1);
    void SetLMNodeLastPing(const CTxIn& vin, const CLMNodePing& mnp);

    void UpdatedBlockTip(const CBlockIndex *pindex);

    /**
     * Called to notify CGovernanceManager that the lmnode index has been updated.
     * Must be called while not holding the CLMNodeMan::cs mutex
     */
    void NotifyLMNodeUpdates();

};

#endif
