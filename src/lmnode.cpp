// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activelmnode.h"
#include "consensus/validation.h"
#include "darksend.h"
#include "init.h"
//#include "governance.h"
#include "lmnode.h"
#include "lmnode-payments.h"
#include "lmnode-sync.h"
#include "lmnodeman.h"
#include "util.h"

#include <boost/lexical_cast.hpp>


CLMNode::CLMNode() :
        vin(),
        addr(),
        pubKeyCollateralAddress(),
        pubKeyLMNode(),
        lastPing(),
        vchSig(),
        sigTime(GetAdjustedTime()),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(0),
        nActiveState(LMNODE_ENABLED),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(PROTOCOL_VERSION),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

CLMNode::CLMNode(CService addrNew, CTxIn vinNew, CPubKey pubKeyCollateralAddressNew, CPubKey pubKeyLMNodeNew, int nProtocolVersionIn) :
        vin(vinNew),
        addr(addrNew),
        pubKeyCollateralAddress(pubKeyCollateralAddressNew),
        pubKeyLMNode(pubKeyLMNodeNew),
        lastPing(),
        vchSig(),
        sigTime(GetAdjustedTime()),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(0),
        nActiveState(LMNODE_ENABLED),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(nProtocolVersionIn),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

CLMNode::CLMNode(const CLMNode &other) :
        vin(other.vin),
        addr(other.addr),
        pubKeyCollateralAddress(other.pubKeyCollateralAddress),
        pubKeyLMNode(other.pubKeyLMNode),
        lastPing(other.lastPing),
        vchSig(other.vchSig),
        sigTime(other.sigTime),
        nLastDsq(other.nLastDsq),
        nTimeLastChecked(other.nTimeLastChecked),
        nTimeLastPaid(other.nTimeLastPaid),
        nTimeLastWatchdogVote(other.nTimeLastWatchdogVote),
        nActiveState(other.nActiveState),
        nCacheCollateralBlock(other.nCacheCollateralBlock),
        nBlockLastPaid(other.nBlockLastPaid),
        nProtocolVersion(other.nProtocolVersion),
        nPoSeBanScore(other.nPoSeBanScore),
        nPoSeBanHeight(other.nPoSeBanHeight),
        fAllowMixingTx(other.fAllowMixingTx),
        fUnitTest(other.fUnitTest) {}

CLMNode::CLMNode(const CLMNodeBroadcast &mnb) :
        vin(mnb.vin),
        addr(mnb.addr),
        pubKeyCollateralAddress(mnb.pubKeyCollateralAddress),
        pubKeyLMNode(mnb.pubKeyLMNode),
        lastPing(mnb.lastPing),
        vchSig(mnb.vchSig),
        sigTime(mnb.sigTime),
        nLastDsq(0),
        nTimeLastChecked(0),
        nTimeLastPaid(0),
        nTimeLastWatchdogVote(mnb.sigTime),
        nActiveState(mnb.nActiveState),
        nCacheCollateralBlock(0),
        nBlockLastPaid(0),
        nProtocolVersion(mnb.nProtocolVersion),
        nPoSeBanScore(0),
        nPoSeBanHeight(0),
        fAllowMixingTx(true),
        fUnitTest(false) {}

//CSporkManager sporkManager;
//
// When a new lmnode broadcast is sent, update our information
//
bool CLMNode::UpdateFromNewBroadcast(CLMNodeBroadcast &mnb) {
    if (mnb.sigTime <= sigTime && !mnb.fRecovery) return false;

    pubKeyLMNode = mnb.pubKeyLMNode;
    sigTime = mnb.sigTime;
    vchSig = mnb.vchSig;
    nProtocolVersion = mnb.nProtocolVersion;
    addr = mnb.addr;
    nPoSeBanScore = 0;
    nPoSeBanHeight = 0;
    nTimeLastChecked = 0;
    int nDos = 0;
    if (mnb.lastPing == CLMNodePing() || (mnb.lastPing != CLMNodePing() && mnb.lastPing.CheckAndUpdate(this, true, nDos))) {
        lastPing = mnb.lastPing;
        mnodeman.mapSeenLMNodePing.insert(std::make_pair(lastPing.GetHash(), lastPing));
    }
    // if it matches our LMNode privkey...
    if (fZNode && pubKeyLMNode == activeLMNode.pubKeyLMNode) {
        nPoSeBanScore = -LMNODE_POSE_BAN_MAX_SCORE;
        if (nProtocolVersion == PROTOCOL_VERSION) {
            // ... and PROTOCOL_VERSION, then we've been remotely activated ...
            activeLMNode.ManageState();
        } else {
            // ... otherwise we need to reactivate our node, do not add it to the list and do not relay
            // but also do not ban the node we get this message from
            LogPrintf("CLMNode::UpdateFromNewBroadcast -- wrong PROTOCOL_VERSION, re-activate your MN: message nProtocolVersion=%d  PROTOCOL_VERSION=%d\n", nProtocolVersion, PROTOCOL_VERSION);
            return false;
        }
    }
    return true;
}

//
// Deterministically calculate a given "score" for a LMNode depending on how close it's hash is to
// the proof of work for that block. The further away they are the better, the furthest will win the election
// and get paid this block
//
arith_uint256 CLMNode::CalculateScore(const uint256 &blockHash) {
    uint256 aux = ArithToUint256(UintToArith256(vin.prevout.hash) + vin.prevout.n);

    CHashWriter ss(SER_GETHASH, PROTOCOL_VERSION);
    ss << blockHash;
    arith_uint256 hash2 = UintToArith256(ss.GetHash());

    CHashWriter ss2(SER_GETHASH, PROTOCOL_VERSION);
    ss2 << blockHash;
    ss2 << aux;
    arith_uint256 hash3 = UintToArith256(ss2.GetHash());

    return (hash3 > hash2 ? hash3 - hash2 : hash2 - hash3);
}

void CLMNode::Check(bool fForce) {
    LOCK(cs);

    if (ShutdownRequested()) return;

    if (!fForce && (GetTime() - nTimeLastChecked < LMNODE_CHECK_SECONDS)) return;
    nTimeLastChecked = GetTime();

    LogPrint("lmnode", "CLMNode::Check -- LMNode %s is in %s state\n", vin.prevout.ToStringShort(), GetStateString());

    //once spent, stop doing the checks
    if (IsOutpointSpent()) return;

    int nHeight = 0;
    if (!fUnitTest) {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) return;

        CCoins coins;
        if (!pcoinsTip->GetCoins(vin.prevout.hash, coins) ||
            (unsigned int) vin.prevout.n >= coins.vout.size() ||
            coins.vout[vin.prevout.n].IsNull()) {
            nActiveState = LMNODE_OUTPOINT_SPENT;
            LogPrint("lmnode", "CLMNode::Check -- Failed to find LMNode UTXO, lmnode=%s\n", vin.prevout.ToStringShort());
            return;
        }

        nHeight = chainActive.Height();
    }

    if (IsPoSeBanned()) {
        if (nHeight < nPoSeBanHeight) return; // too early?
        // Otherwise give it a chance to proceed further to do all the usual checks and to change its state.
        // LMNode still will be on the edge and can be banned back easily if it keeps ignoring mnverify
        // or connect attempts. Will require few mnverify messages to strengthen its position in mn list.
        LogPrintf("CLMNode::Check -- LMNode %s is unbanned and back in list now\n", vin.prevout.ToStringShort());
        DecreasePoSeBanScore();
    } else if (nPoSeBanScore >= LMNODE_POSE_BAN_MAX_SCORE) {
        nActiveState = LMNODE_POSE_BAN;
        // ban for the whole payment cycle
        nPoSeBanHeight = nHeight + mnodeman.size();
        LogPrintf("CLMNode::Check -- LMNode %s is banned till block %d now\n", vin.prevout.ToStringShort(), nPoSeBanHeight);
        return;
    }

    int nActiveStatePrev = nActiveState;
    bool fOurLMNode = fZNode && activeLMNode.pubKeyLMNode == pubKeyLMNode;

    // lmnode doesn't meet payment protocol requirements ...
    bool fRequireUpdate = nProtocolVersion < mnpayments.GetMinLMNodePaymentsProto() ||
                          // or it's our own node and we just updated it to the new protocol but we are still waiting for activation ...
                          (fOurLMNode && nProtocolVersion < PROTOCOL_VERSION);

    if (fRequireUpdate) {
        nActiveState = LMNODE_UPDATE_REQUIRED;
        if (nActiveStatePrev != nActiveState) {
            LogPrint("lmnode", "CLMNode::Check -- LMNode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
        }
        return;
    }

    // keep old lmnodes on start, give them a chance to receive updates...
    bool fWaitForPing = !lmnodeSync.IsLMNodeListSynced() && !IsPingedWithin(LMNODE_MIN_MNP_SECONDS);

    if (fWaitForPing && !fOurLMNode) {
        // ...but if it was already expired before the initial check - return right away
        if (IsExpired() || IsWatchdogExpired() || IsNewStartRequired()) {
            LogPrint("lmnode", "CLMNode::Check -- LMNode %s is in %s state, waiting for ping\n", vin.prevout.ToStringShort(), GetStateString());
            return;
        }
    }

    // don't expire if we are still in "waiting for ping" mode unless it's our own lmnode
    if (!fWaitForPing || fOurLMNode) {

        if (!IsPingedWithin(LMNODE_NEW_START_REQUIRED_SECONDS)) {
            nActiveState = LMNODE_NEW_START_REQUIRED;
            if (nActiveStatePrev != nActiveState) {
                LogPrint("lmnode", "CLMNode::Check -- LMNode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }

        bool fWatchdogActive = lmnodeSync.IsSynced() && mnodeman.IsWatchdogActive();
        bool fWatchdogExpired = (fWatchdogActive && ((GetTime() - nTimeLastWatchdogVote) > LMNODE_WATCHDOG_MAX_SECONDS));

//        LogPrint("lmnode", "CLMNode::Check -- outpoint=%s, nTimeLastWatchdogVote=%d, GetTime()=%d, fWatchdogExpired=%d\n",
//                vin.prevout.ToStringShort(), nTimeLastWatchdogVote, GetTime(), fWatchdogExpired);

        if (fWatchdogExpired) {
            nActiveState = LMNODE_WATCHDOG_EXPIRED;
            if (nActiveStatePrev != nActiveState) {
                LogPrint("lmnode", "CLMNode::Check -- LMNode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }

        if (!IsPingedWithin(LMNODE_EXPIRATION_SECONDS)) {
            nActiveState = LMNODE_EXPIRED;
            if (nActiveStatePrev != nActiveState) {
                LogPrint("lmnode", "CLMNode::Check -- LMNode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
            }
            return;
        }
    }

    if (lastPing.sigTime - sigTime < LMNODE_MIN_MNP_SECONDS) {
        nActiveState = LMNODE_PRE_ENABLED;
        if (nActiveStatePrev != nActiveState) {
            LogPrint("lmnode", "CLMNode::Check -- LMNode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
        }
        return;
    }

    nActiveState = LMNODE_ENABLED; // OK
    if (nActiveStatePrev != nActiveState) {
        LogPrint("lmnode", "CLMNode::Check -- LMNode %s is in %s state now\n", vin.prevout.ToStringShort(), GetStateString());
    }
}

bool CLMNode::IsValidNetAddr() {
    return IsValidNetAddr(addr);
}

bool CLMNode::IsValidForPayment() {
    if (nActiveState == LMNODE_ENABLED) {
        return true;
    }
//    if(!sporkManager.IsSporkActive(SPORK_14_REQUIRE_SENTINEL_FLAG) &&
//       (nActiveState == LMNODE_WATCHDOG_EXPIRED)) {
//        return true;
//    }

    return false;
}

bool CLMNode::IsValidNetAddr(CService addrIn) {
    // TODO: regtest is fine with any addresses for now,
    // should probably be a bit smarter if one day we start to implement tests for this
    return Params().NetworkIDString() == CBaseChainParams::REGTEST ||
           (addrIn.IsIPv4() && IsReachable(addrIn) && addrIn.IsRoutable());
}

lmnode_info_t CLMNode::GetInfo() {
    lmnode_info_t info;
    info.vin = vin;
    info.addr = addr;
    info.pubKeyCollateralAddress = pubKeyCollateralAddress;
    info.pubKeyLMNode = pubKeyLMNode;
    info.sigTime = sigTime;
    info.nLastDsq = nLastDsq;
    info.nTimeLastChecked = nTimeLastChecked;
    info.nTimeLastPaid = nTimeLastPaid;
    info.nTimeLastWatchdogVote = nTimeLastWatchdogVote;
    info.nTimeLastPing = lastPing.sigTime;
    info.nActiveState = nActiveState;
    info.nProtocolVersion = nProtocolVersion;
    info.fInfoValid = true;
    return info;
}

std::string CLMNode::StateToString(int nStateIn) {
    switch (nStateIn) {
        case LMNODE_PRE_ENABLED:
            return "PRE_ENABLED";
        case LMNODE_ENABLED:
            return "ENABLED";
        case LMNODE_EXPIRED:
            return "EXPIRED";
        case LMNODE_OUTPOINT_SPENT:
            return "OUTPOINT_SPENT";
        case LMNODE_UPDATE_REQUIRED:
            return "UPDATE_REQUIRED";
        case LMNODE_WATCHDOG_EXPIRED:
            return "WATCHDOG_EXPIRED";
        case LMNODE_NEW_START_REQUIRED:
            return "NEW_START_REQUIRED";
        case LMNODE_POSE_BAN:
            return "POSE_BAN";
        default:
            return "UNKNOWN";
    }
}

std::string CLMNode::GetStateString() const {
    return StateToString(nActiveState);
}

std::string CLMNode::GetStatus() const {
    // TODO: return smth a bit more human readable here
    return GetStateString();
}

std::string CLMNode::ToString() const {
    std::string str;
    str += "lmnode{";
    str += addr.ToString();
    str += " ";
    str += std::to_string(nProtocolVersion);
    str += " ";
    str += vin.prevout.ToStringShort();
    str += " ";
    str += CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString();
    str += " ";
    str += std::to_string(lastPing == CLMNodePing() ? sigTime : lastPing.sigTime);
    str += " ";
    str += std::to_string(lastPing == CLMNodePing() ? 0 : lastPing.sigTime - sigTime);
    str += " ";
    str += std::to_string(nBlockLastPaid);
    str += "}\n";
    return str;
}

int CLMNode::GetCollateralAge() {
    int nHeight;
    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain || !chainActive.Tip()) return -1;
        nHeight = chainActive.Height();
    }

    if (nCacheCollateralBlock == 0) {
        int nInputAge = GetInputAge(vin);
        if (nInputAge > 0) {
            nCacheCollateralBlock = nHeight - nInputAge;
        } else {
            return nInputAge;
        }
    }

    return nHeight - nCacheCollateralBlock;
}

void CLMNode::UpdateLastPaid(const CBlockIndex *pindex, int nMaxBlocksToScanBack) {
    if (!pindex) {
        LogPrintf("CLMNode::UpdateLastPaid pindex is NULL\n");
        return;
    }

    const CBlockIndex *BlockReading = pindex;

    CScript mnpayee = GetScriptForDestination(pubKeyCollateralAddress.GetID());
    LogPrint("lmnode", "CLMNode::UpdateLastPaidBlock -- searching for block with payment to %s\n", vin.prevout.ToStringShort());

    LOCK(cs_mapLMNodeBlocks);

    for (int i = 0; BlockReading && BlockReading->nHeight > nBlockLastPaid && i < nMaxBlocksToScanBack; i++) {
//        LogPrintf("mnpayments.mapLMNodeBlocks.count(BlockReading->nHeight)=%s\n", mnpayments.mapLMNodeBlocks.count(BlockReading->nHeight));
//        LogPrintf("mnpayments.mapLMNodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2)=%s\n", mnpayments.mapLMNodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2));
        if (mnpayments.mapLMNodeBlocks.count(BlockReading->nHeight) &&
            mnpayments.mapLMNodeBlocks[BlockReading->nHeight].HasPayeeWithVotes(mnpayee, 2)) {
            // LogPrintf("i=%s, BlockReading->nHeight=%s\n", i, BlockReading->nHeight);
            CBlock block;
            if (!ReadBlockFromDisk(block, BlockReading, Params().GetConsensus())) // shouldn't really happen
            {
                LogPrintf("ReadBlockFromDisk failed\n");
                continue;
            }

            CAmount nLMNodePayment = GetLMNodePayment(BlockReading->nHeight, block.vtx[0].GetValueOut());

            BOOST_FOREACH(CTxOut txout, block.vtx[0].vout)
            if (mnpayee == txout.scriptPubKey && nLMNodePayment == txout.nValue) {
                nBlockLastPaid = BlockReading->nHeight;
                nTimeLastPaid = BlockReading->nTime;
                LogPrint("lmnode", "CLMNode::UpdateLastPaidBlock -- searching for block with payment to %s -- found new %d\n", vin.prevout.ToStringShort(), nBlockLastPaid);
                return;
            }
        }

        if (BlockReading->pprev == NULL) {
            assert(BlockReading);
            break;
        }
        BlockReading = BlockReading->pprev;
    }

    // Last payment for this lmnode wasn't found in latest mnpayments blocks
    // or it was found in mnpayments blocks but wasn't found in the blockchain.
    // LogPrint("lmnode", "CLMNode::UpdateLastPaidBlock -- searching for block with payment to %s -- keeping old %d\n", vin.prevout.ToStringShort(), nBlockLastPaid);
}

bool CLMNodeBroadcast::Create(std::string strService, std::string strKeyLMNode, std::string strTxHash, std::string strOutputIndex, std::string &strErrorRet, CLMNodeBroadcast &mnbRet, bool fOffline) {
    LogPrintf("CLMNodeBroadcast::Create\n");
    CTxIn txin;
    CPubKey pubKeyCollateralAddressNew;
    CKey keyCollateralAddressNew;
    CPubKey pubKeyLMNodeNew;
    CKey keyLMNodeNew;
    //need correct blocks to send ping
    if (!fOffline && !lmnodeSync.IsBlockchainSynced()) {
        strErrorRet = "Sync in progress. Must wait until sync is complete to start LMNode";
        LogPrintf("CLMNodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    //TODO
    if (!darkSendSigner.GetKeysFromSecret(strKeyLMNode, keyLMNodeNew, pubKeyLMNodeNew)) {
        strErrorRet = strprintf("Invalid lmnode key %s", strKeyLMNode);
        LogPrintf("CLMNodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    if (!pwalletMain->GetLMNodeVinAndKeys(txin, pubKeyCollateralAddressNew, keyCollateralAddressNew, strTxHash, strOutputIndex)) {
        strErrorRet = strprintf("Could not allocate txin %s:%s for lmnode %s", strTxHash, strOutputIndex, strService);
        LogPrintf("CLMNodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    CService service = CService(strService);
    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (service.GetPort() != mainnetDefaultPort) {
            strErrorRet = strprintf("Invalid port %u for lmnode %s, only %d is supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort);
            LogPrintf("CLMNodeBroadcast::Create -- %s\n", strErrorRet);
            return false;
        }
    } else if (service.GetPort() == mainnetDefaultPort) {
        strErrorRet = strprintf("Invalid port %u for lmnode %s, %d is the only supported on mainnet.", service.GetPort(), strService, mainnetDefaultPort);
        LogPrintf("CLMNodeBroadcast::Create -- %s\n", strErrorRet);
        return false;
    }

    return Create(txin, CService(strService), keyCollateralAddressNew, pubKeyCollateralAddressNew, keyLMNodeNew, pubKeyLMNodeNew, strErrorRet, mnbRet);
}

bool CLMNodeBroadcast::Create(CTxIn txin, CService service, CKey keyCollateralAddressNew, CPubKey pubKeyCollateralAddressNew, CKey keyLMNodeNew, CPubKey pubKeyLMNodeNew, std::string &strErrorRet, CLMNodeBroadcast &mnbRet) {
    // wait for reindex and/or import to finish
    if (fImporting || fReindex) return false;

    LogPrint("lmnode", "CLMNodeBroadcast::Create -- pubKeyCollateralAddressNew = %s, pubKeyLMNodeNew.GetID() = %s\n",
             CBitcoinAddress(pubKeyCollateralAddressNew.GetID()).ToString(),
             pubKeyLMNodeNew.GetID().ToString());


    CLMNodePing mnp(txin);
    if (!mnp.Sign(keyLMNodeNew, pubKeyLMNodeNew)) {
        strErrorRet = strprintf("Failed to sign ping, lmnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CLMNodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CLMNodeBroadcast();
        return false;
    }

    mnbRet = CLMNodeBroadcast(service, txin, pubKeyCollateralAddressNew, pubKeyLMNodeNew, PROTOCOL_VERSION);

    if (!mnbRet.IsValidNetAddr()) {
        strErrorRet = strprintf("Invalid IP address, lmnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CLMNodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CLMNodeBroadcast();
        return false;
    }

    mnbRet.lastPing = mnp;
    if (!mnbRet.Sign(keyCollateralAddressNew)) {
        strErrorRet = strprintf("Failed to sign broadcast, lmnode=%s", txin.prevout.ToStringShort());
        LogPrintf("CLMNodeBroadcast::Create -- %s\n", strErrorRet);
        mnbRet = CLMNodeBroadcast();
        return false;
    }

    return true;
}

bool CLMNodeBroadcast::SimpleCheck(int &nDos) {
    nDos = 0;

    // make sure addr is valid
    if (!IsValidNetAddr()) {
        LogPrintf("CLMNodeBroadcast::SimpleCheck -- Invalid addr, rejected: lmnode=%s  addr=%s\n",
                  vin.prevout.ToStringShort(), addr.ToString());
        return false;
    }

    // make sure signature isn't in the future (past is OK)
    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrintf("CLMNodeBroadcast::SimpleCheck -- Signature rejected, too far into the future: lmnode=%s\n", vin.prevout.ToStringShort());
        nDos = 1;
        return false;
    }

    // empty ping or incorrect sigTime/unknown blockhash
    if (lastPing == CLMNodePing() || !lastPing.SimpleCheck(nDos)) {
        // one of us is probably forked or smth, just mark it as expired and check the rest of the rules
        nActiveState = LMNODE_EXPIRED;
    }

    if (nProtocolVersion < mnpayments.GetMinLMNodePaymentsProto()) {
        LogPrintf("CLMNodeBroadcast::SimpleCheck -- ignoring outdated LMNode: lmnode=%s  nProtocolVersion=%d\n", vin.prevout.ToStringShort(), nProtocolVersion);
        return false;
    }

    CScript pubkeyScript;
    pubkeyScript = GetScriptForDestination(pubKeyCollateralAddress.GetID());

    if (pubkeyScript.size() != 25) {
        LogPrintf("CLMNodeBroadcast::SimpleCheck -- pubKeyCollateralAddress has the wrong size\n");
        nDos = 100;
        return false;
    }

    CScript pubkeyScript2;
    pubkeyScript2 = GetScriptForDestination(pubKeyLMNode.GetID());

    if (pubkeyScript2.size() != 25) {
        LogPrintf("CLMNodeBroadcast::SimpleCheck -- pubKeyLMNode has the wrong size\n");
        nDos = 100;
        return false;
    }

    if (!vin.scriptSig.empty()) {
        LogPrintf("CLMNodeBroadcast::SimpleCheck -- Ignore Not Empty ScriptSig %s\n", vin.ToString());
        nDos = 100;
        return false;
    }

    int mainnetDefaultPort = Params(CBaseChainParams::MAIN).GetDefaultPort();
    if (Params().NetworkIDString() == CBaseChainParams::MAIN) {
        if (addr.GetPort() != mainnetDefaultPort) return false;
    } else if (addr.GetPort() == mainnetDefaultPort) return false;

    return true;
}

bool CLMNodeBroadcast::Update(CLMNode *pmn, int &nDos) {
    nDos = 0;

    if (pmn->sigTime == sigTime && !fRecovery) {
        // mapSeenLMNodeBroadcast in CLMNodeMan::CheckMnbAndUpdateLMNodeList should filter legit duplicates
        // but this still can happen if we just started, which is ok, just do nothing here.
        return false;
    }

    // this broadcast is older than the one that we already have - it's bad and should never happen
    // unless someone is doing something fishy
    if (pmn->sigTime > sigTime) {
        LogPrintf("CLMNodeBroadcast::Update -- Bad sigTime %d (existing broadcast is at %d) for LMNode %s %s\n",
                  sigTime, pmn->sigTime, vin.prevout.ToStringShort(), addr.ToString());
        return false;
    }

    pmn->Check();

    // lmnode is banned by PoSe
    if (pmn->IsPoSeBanned()) {
        LogPrintf("CLMNodeBroadcast::Update -- Banned by PoSe, lmnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    // IsVnAssociatedWithPubkey is validated once in CheckOutpoint, after that they just need to match
    if (pmn->pubKeyCollateralAddress != pubKeyCollateralAddress) {
        LogPrintf("CLMNodeBroadcast::Update -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    if (!CheckSignature(nDos)) {
        LogPrintf("CLMNodeBroadcast::Update -- CheckSignature() failed, lmnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    // if ther was no lmnode broadcast recently or if it matches our LMNode privkey...
    if (!pmn->IsBroadcastedWithin(LMNODE_MIN_MNB_SECONDS) || (fZNode && pubKeyLMNode == activeLMNode.pubKeyLMNode)) {
        // take the newest entry
        LogPrintf("CLMNodeBroadcast::Update -- Got UPDATED LMNode entry: addr=%s\n", addr.ToString());
        if (pmn->UpdateFromNewBroadcast((*this))) {
            pmn->Check();
            RelayZNode();
        }
        lmnodeSync.AddedLMNodeList();
    }

    return true;
}

bool CLMNodeBroadcast::CheckOutpoint(int &nDos) {
    // we are a lmnode with the same vin (i.e. already activated) and this mnb is ours (matches our LMNode privkey)
    // so nothing to do here for us
    if (fZNode && vin.prevout == activeLMNode.vin.prevout && pubKeyLMNode == activeLMNode.pubKeyLMNode) {
        return false;
    }

    if (!CheckSignature(nDos)) {
        LogPrintf("CLMNodeBroadcast::CheckOutpoint -- CheckSignature() failed, lmnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    {
        TRY_LOCK(cs_main, lockMain);
        if (!lockMain) {
            // not mnb fault, let it to be checked again later
            LogPrint("lmnode", "CLMNodeBroadcast::CheckOutpoint -- Failed to aquire lock, addr=%s", addr.ToString());
            mnodeman.mapSeenLMNodeBroadcast.erase(GetHash());
            return false;
        }

        CCoins coins;
        if (!pcoinsTip->GetCoins(vin.prevout.hash, coins) ||
            (unsigned int) vin.prevout.n >= coins.vout.size() ||
            coins.vout[vin.prevout.n].IsNull()) {
            LogPrint("lmnode", "CLMNodeBroadcast::CheckOutpoint -- Failed to find LMNode UTXO, lmnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
        if (coins.vout[vin.prevout.n].nValue != LMNODE_COIN_REQUIRED * COIN) {
            LogPrint("lmnode", "CLMNodeBroadcast::CheckOutpoint -- LMNode UTXO should have 1000 HPP, lmnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
        if (chainActive.Height() - coins.nHeight + 1 < Params().GetConsensus().nLMNodeMinimumConfirmations) {
            LogPrintf("CLMNodeBroadcast::CheckOutpoint -- LMNode UTXO must have at least %d confirmations, lmnode=%s\n",
                      Params().GetConsensus().nLMNodeMinimumConfirmations, vin.prevout.ToStringShort());
            // maybe we miss few blocks, let this mnb to be checked again later
            mnodeman.mapSeenLMNodeBroadcast.erase(GetHash());
            return false;
        }
    }

    LogPrint("lmnode", "CLMNodeBroadcast::CheckOutpoint -- LMNode UTXO verified\n");

    // make sure the vout that was signed is related to the transaction that spawned the LMNode
    //  - this is expensive, so it's only done once per LMNode
    if (!darkSendSigner.IsVinAssociatedWithPubkey(vin, pubKeyCollateralAddress)) {
        LogPrintf("CLMNodeMan::CheckOutpoint -- Got mismatched pubKeyCollateralAddress and vin\n");
        nDos = 33;
        return false;
    }

    // verify that sig time is legit in past
    // should be at least not earlier than block when 1000 HPP tx got nLMNodeMinimumConfirmations
    uint256 hashBlock = uint256();
    CTransaction tx2;
    GetTransaction(vin.prevout.hash, tx2, Params().GetConsensus(), hashBlock, true);
    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(hashBlock);
        if (mi != mapBlockIndex.end() && (*mi).second) {
            CBlockIndex *pMNIndex = (*mi).second; // block for 1000 HPP tx -> 1 confirmation
            CBlockIndex *pConfIndex = chainActive[pMNIndex->nHeight + Params().GetConsensus().nLMNodeMinimumConfirmations - 1]; // block where tx got nLMNodeMinimumConfirmations
            if (pConfIndex->GetBlockTime() > sigTime) {
                LogPrintf("CLMNodeBroadcast::CheckOutpoint -- Bad sigTime %d (%d conf block is at %d) for LMNode %s %s\n",
                          sigTime, Params().GetConsensus().nLMNodeMinimumConfirmations, pConfIndex->GetBlockTime(), vin.prevout.ToStringShort(), addr.ToString());
                return false;
            }
        }
    }

    return true;
}

bool CLMNodeBroadcast::Sign(CKey &keyCollateralAddress) {
    std::string strError;
    std::string strMessage;

    sigTime = GetAdjustedTime();

    strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) +
                 pubKeyCollateralAddress.GetID().ToString() + pubKeyLMNode.GetID().ToString() +
                 boost::lexical_cast<std::string>(nProtocolVersion);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, keyCollateralAddress)) {
        LogPrintf("CLMNodeBroadcast::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
        LogPrintf("CLMNodeBroadcast::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CLMNodeBroadcast::CheckSignature(int &nDos) {
    std::string strMessage;
    std::string strError = "";
    nDos = 0;

    strMessage = addr.ToString() + boost::lexical_cast<std::string>(sigTime) +
                 pubKeyCollateralAddress.GetID().ToString() + pubKeyLMNode.GetID().ToString() +
                 boost::lexical_cast<std::string>(nProtocolVersion);

    LogPrint("lmnode", "CLMNodeBroadcast::CheckSignature -- strMessage: %s  pubKeyCollateralAddress address: %s  sig: %s\n", strMessage, CBitcoinAddress(pubKeyCollateralAddress.GetID()).ToString(), EncodeBase64(&vchSig[0], vchSig.size()));

    if (!darkSendSigner.VerifyMessage(pubKeyCollateralAddress, vchSig, strMessage, strError)) {
        LogPrintf("CLMNodeBroadcast::CheckSignature -- Got bad LMNode announce signature, error: %s\n", strError);
        nDos = 100;
        return false;
    }

    return true;
}

void CLMNodeBroadcast::RelayZNode() {
    LogPrintf("CLMNodeBroadcast::RelayZNode\n");
    CInv inv(MSG_LMNODE_ANNOUNCE, GetHash());
    RelayInv(inv);
}

CLMNodePing::CLMNodePing(CTxIn &vinNew) {
    LOCK(cs_main);
    if (!chainActive.Tip() || chainActive.Height() < 12) return;

    vin = vinNew;
    blockHash = chainActive[chainActive.Height() - 12]->GetBlockHash();
    sigTime = GetAdjustedTime();
    vchSig = std::vector < unsigned char > ();
}

bool CLMNodePing::Sign(CKey &keyLMNode, CPubKey &pubKeyLMNode) {
    std::string strError;
    std::string strZNodeSignMessage;

    sigTime = GetAdjustedTime();
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, keyLMNode)) {
        LogPrintf("CLMNodePing::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(pubKeyLMNode, vchSig, strMessage, strError)) {
        LogPrintf("CLMNodePing::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CLMNodePing::CheckSignature(CPubKey &pubKeyLMNode, int &nDos) {
    std::string strMessage = vin.ToString() + blockHash.ToString() + boost::lexical_cast<std::string>(sigTime);
    std::string strError = "";
    nDos = 0;

    if (!darkSendSigner.VerifyMessage(pubKeyLMNode, vchSig, strMessage, strError)) {
        LogPrintf("CLMNodePing::CheckSignature -- Got bad LMNode ping signature, lmnode=%s, error: %s\n", vin.prevout.ToStringShort(), strError);
        nDos = 33;
        return false;
    }
    return true;
}

bool CLMNodePing::SimpleCheck(int &nDos) {
    // don't ban by default
    nDos = 0;

    if (sigTime > GetAdjustedTime() + 60 * 60) {
        LogPrintf("CLMNodePing::SimpleCheck -- Signature rejected, too far into the future, lmnode=%s\n", vin.prevout.ToStringShort());
        nDos = 1;
        return false;
    }

    {
//        LOCK(cs_main);
        AssertLockHeld(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if (mi == mapBlockIndex.end()) {
            LogPrint("lmnode", "CLMNodePing::SimpleCheck -- LMNode ping is invalid, unknown block hash: lmnode=%s blockHash=%s\n", vin.prevout.ToStringShort(), blockHash.ToString());
            // maybe we stuck or forked so we shouldn't ban this node, just fail to accept this ping
            // TODO: or should we also request this block?
            return false;
        }
    }
    LogPrint("lmnode", "CLMNodePing::SimpleCheck -- LMNode ping verified: lmnode=%s  blockHash=%s  sigTime=%d\n", vin.prevout.ToStringShort(), blockHash.ToString(), sigTime);
    return true;
}

bool CLMNodePing::CheckAndUpdate(CLMNode *pmn, bool fFromNewBroadcast, int &nDos) {
    // don't ban by default
    nDos = 0;

    if (!SimpleCheck(nDos)) {
        return false;
    }

    if (pmn == NULL) {
        LogPrint("lmnode", "CLMNodePing::CheckAndUpdate -- Couldn't find LMNode entry, lmnode=%s\n", vin.prevout.ToStringShort());
        return false;
    }

    if (!fFromNewBroadcast) {
        if (pmn->IsUpdateRequired()) {
            LogPrint("lmnode", "CLMNodePing::CheckAndUpdate -- lmnode protocol is outdated, lmnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }

        if (pmn->IsNewStartRequired()) {
            LogPrint("lmnode", "CLMNodePing::CheckAndUpdate -- lmnode is completely expired, new start is required, lmnode=%s\n", vin.prevout.ToStringShort());
            return false;
        }
    }

    {
        LOCK(cs_main);
        BlockMap::iterator mi = mapBlockIndex.find(blockHash);
        if ((*mi).second && (*mi).second->nHeight < chainActive.Height() - 24) {
            LogPrintf("CLMNodePing::CheckAndUpdate -- LMNode ping is invalid, block hash is too old: lmnode=%s  blockHash=%s\n", vin.prevout.ToStringShort(), blockHash.ToString());
            // nDos = 1;
            return false;
        }
    }

    LogPrint("lmnode", "CLMNodePing::CheckAndUpdate -- New ping: lmnode=%s  blockHash=%s  sigTime=%d\n", vin.prevout.ToStringShort(), blockHash.ToString(), sigTime);

    // LogPrintf("mnping - Found corresponding mn for vin: %s\n", vin.prevout.ToStringShort());
    // update only if there is no known ping for this lmnode or
    // last ping was more then LMNODE_MIN_MNP_SECONDS-60 ago comparing to this one
    if (pmn->IsPingedWithin(LMNODE_MIN_MNP_SECONDS - 60, sigTime)) {
        LogPrint("lmnode", "CLMNodePing::CheckAndUpdate -- LMNode ping arrived too early, lmnode=%s\n", vin.prevout.ToStringShort());
        //nDos = 1; //disable, this is happening frequently and causing banned peers
        return false;
    }

    if (!CheckSignature(pmn->pubKeyLMNode, nDos)) return false;

    // so, ping seems to be ok

    // if we are still syncing and there was no known ping for this mn for quite a while
    // (NOTE: assuming that LMNODE_EXPIRATION_SECONDS/2 should be enough to finish mn list sync)
    if (!lmnodeSync.IsLMNodeListSynced() && !pmn->IsPingedWithin(LMNODE_EXPIRATION_SECONDS / 2)) {
        // let's bump sync timeout
        LogPrint("lmnode", "CLMNodePing::CheckAndUpdate -- bumping sync timeout, lmnode=%s\n", vin.prevout.ToStringShort());
        lmnodeSync.AddedLMNodeList();
    }

    // let's store this ping as the last one
    LogPrint("lmnode", "CLMNodePing::CheckAndUpdate -- LMNode ping accepted, lmnode=%s\n", vin.prevout.ToStringShort());
    pmn->lastPing = *this;

    // and update mnodeman.mapSeenLMNodeBroadcast.lastPing which is probably outdated
    CLMNodeBroadcast mnb(*pmn);
    uint256 hash = mnb.GetHash();
    if (mnodeman.mapSeenLMNodeBroadcast.count(hash)) {
        mnodeman.mapSeenLMNodeBroadcast[hash].second.lastPing = *this;
    }

    pmn->Check(true); // force update, ignoring cache
    if (!pmn->IsEnabled()) return false;

    LogPrint("lmnode", "CLMNodePing::CheckAndUpdate -- LMNode ping acceepted and relayed, lmnode=%s\n", vin.prevout.ToStringShort());
    Relay();

    return true;
}

void CLMNodePing::Relay() {
    CInv inv(MSG_LMNODE_PING, GetHash());
    RelayInv(inv);
}

//void CLMNode::AddGovernanceVote(uint256 nGovernanceObjectHash)
//{
//    if(mapGovernanceObjectsVotedOn.count(nGovernanceObjectHash)) {
//        mapGovernanceObjectsVotedOn[nGovernanceObjectHash]++;
//    } else {
//        mapGovernanceObjectsVotedOn.insert(std::make_pair(nGovernanceObjectHash, 1));
//    }
//}

//void CLMNode::RemoveGovernanceObject(uint256 nGovernanceObjectHash)
//{
//    std::map<uint256, int>::iterator it = mapGovernanceObjectsVotedOn.find(nGovernanceObjectHash);
//    if(it == mapGovernanceObjectsVotedOn.end()) {
//        return;
//    }
//    mapGovernanceObjectsVotedOn.erase(it);
//}

void CLMNode::UpdateWatchdogVoteTime() {
    LOCK(cs);
    nTimeLastWatchdogVote = GetTime();
}

/**
*   FLAG GOVERNANCE ITEMS AS DIRTY
*
*   - When lmnode come and go on the network, we must flag the items they voted on to recalc it's cached flags
*
*/
//void CLMNode::FlagGovernanceItemsAsDirty()
//{
//    std::vector<uint256> vecDirty;
//    {
//        std::map<uint256, int>::iterator it = mapGovernanceObjectsVotedOn.begin();
//        while(it != mapGovernanceObjectsVotedOn.end()) {
//            vecDirty.push_back(it->first);
//            ++it;
//        }
//    }
//    for(size_t i = 0; i < vecDirty.size(); ++i) {
//        mnodeman.AddDirtyGovernanceObjectHash(vecDirty[i]);
//    }
//}
