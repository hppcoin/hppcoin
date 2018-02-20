// Copyright (c) 2014-2017 The Dash Core developers
// Copyright (c) 2016-2017 The Zcoin developers
// Copyright (c) 2017-2018 The Hppcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "activelmnode.h"
#include "darksend.h"
#include "lmnode-payments.h"
#include "lmnode-sync.h"
#include "lmnodeman.h"
#include "netfulfilledman.h"
#include "spork.h"
#include "util.h"

#include <boost/lexical_cast.hpp>

/** Object for who's going to get paid on which blocks */
CLMNodePayments mnpayments;

CCriticalSection cs_vecPayees;
CCriticalSection cs_mapLMNodeBlocks;
CCriticalSection cs_mapLMNodePaymentVotes;

/**
* IsBlockValueValid
*
*   Determine if coinbase outgoing created money is the correct value
*
*   Why is this needed?
*   - In Dash some blocks are superblocks, which output much higher amounts of coins
*   - Otherblocks are 10% lower in outgoing value, so in total, no extra coins are created
*   - When non-superblocks are detected, the normal schedule should be maintained
*/

bool IsBlockValueValid(const CBlock &block, int nBlockHeight, CAmount blockReward, std::string &strErrorRet) {
    strErrorRet = "";

    bool isBlockRewardValueMet = (block.vtx[0].GetValueOut() <= blockReward);
    if (fDebug) LogPrintf("block.vtx[0].GetValueOut() %lld <= blockReward %lld\n", block.vtx[0].GetValueOut(), blockReward);

    // we are still using budgets, but we have no data about them anymore,
    // all we know is predefined budget cycle and window

//    const Consensus::Params &consensusParams = Params().GetConsensus();
//
////    if (nBlockHeight < consensusParams.nSuperblockStartBlock) {
//        int nOffset = nBlockHeight % consensusParams.nBudgetPaymentsCycleBlocks;
//        if (nBlockHeight >= consensusParams.nBudgetPaymentsStartBlock &&
//            nOffset < consensusParams.nBudgetPaymentsWindowBlocks) {
//            // NOTE: make sure SPORK_13_OLD_SUPERBLOCK_FLAG is disabled when 12.1 starts to go live
//            if (lmnodeSync.IsSynced() && !sporkManager.IsSporkActive(SPORK_13_OLD_SUPERBLOCK_FLAG)) {
//                // no budget blocks should be accepted here, if SPORK_13_OLD_SUPERBLOCK_FLAG is disabled
//                LogPrint("gobject", "IsBlockValueValid -- Client synced but budget spork is disabled, checking block value against block reward\n");
//                if (!isBlockRewardValueMet) {
//                    strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, budgets are disabled",
//                                            nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//                }
//                return isBlockRewardValueMet;
//            }
//            LogPrint("gobject", "IsBlockValueValid -- WARNING: Skipping budget block value checks, accepting block\n");
//            // TODO: reprocess blocks to make sure they are legit?
//            return true;
//        }
//        // LogPrint("gobject", "IsBlockValueValid -- Block is not in budget cycle window, checking block value against block reward\n");
//        if (!isBlockRewardValueMet) {
//            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, block is not in budget cycle window",
//                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//        }
//        return isBlockRewardValueMet;
//    }

    // superblocks started

//    CAmount nSuperblockMaxValue =  blockReward + CSuperblock::GetPaymentsLimit(nBlockHeight);
//    bool isSuperblockMaxValueMet = (block.vtx[0].GetValueOut() <= nSuperblockMaxValue);
//    bool isSuperblockMaxValueMet = false;

//    LogPrint("gobject", "block.vtx[0].GetValueOut() %lld <= nSuperblockMaxValue %lld\n", block.vtx[0].GetValueOut(), nSuperblockMaxValue);

    if (!lmnodeSync.IsSynced()) {
        // not enough data but at least it must NOT exceed superblock max value
//        if(CSuperblock::IsValidBlockHeight(nBlockHeight)) {
//            if(fDebug) LogPrintf("IsBlockPayeeValid -- WARNING: Client not synced, checking superblock max bounds only\n");
//            if(!isSuperblockMaxValueMet) {
//                strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded superblock max value",
//                                        nBlockHeight, block.vtx[0].GetValueOut(), nSuperblockMaxValue);
//            }
//            return isSuperblockMaxValueMet;
//        }
        if (!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, only regular blocks are allowed at this height",
                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
        }
        // it MUST be a regular block otherwise
        return isBlockRewardValueMet;
    }

    // we are synced, let's try to check as much data as we can

    if (sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED)) {
////        if(CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
////            if(CSuperblockManager::IsValid(block.vtx[0], nBlockHeight, blockReward)) {
////                LogPrint("gobject", "IsBlockValueValid -- Valid superblock at height %d: %s", nBlockHeight, block.vtx[0].ToString());
////                // all checks are done in CSuperblock::IsValid, nothing to do here
////                return true;
////            }
////
////            // triggered but invalid? that's weird
////            LogPrintf("IsBlockValueValid -- ERROR: Invalid superblock detected at height %d: %s", nBlockHeight, block.vtx[0].ToString());
////            // should NOT allow invalid superblocks, when superblocks are enabled
////            strErrorRet = strprintf("invalid superblock detected at height %d", nBlockHeight);
////            return false;
////        }
//        LogPrint("gobject", "IsBlockValueValid -- No triggered superblock detected at height %d\n", nBlockHeight);
//        if(!isBlockRewardValueMet) {
//            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, no triggered superblock detected",
//                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
//        }
    } else {
//        // should NOT allow superblocks at all, when superblocks are disabled
        LogPrint("gobject", "IsBlockValueValid -- Superblocks are disabled, no superblocks allowed\n");
        if (!isBlockRewardValueMet) {
            strErrorRet = strprintf("coinbase pays too much at height %d (actual=%d vs limit=%d), exceeded block reward, superblocks are disabled",
                                    nBlockHeight, block.vtx[0].GetValueOut(), blockReward);
        }
    }

    // it MUST be a regular block
    return isBlockRewardValueMet;
}

bool IsBlockPayeeValid(const CTransaction &txNew, int nBlockHeight, CAmount blockReward) {
    // we can only check lmnode payment /
    const Consensus::Params &consensusParams = Params().GetConsensus();

    if (nBlockHeight < consensusParams.nLMNodePaymentsStartBlock) {
        //there is no budget data to use to check anything, let's just accept the longest chain
        if (fDebug) LogPrintf("IsBlockPayeeValid -- lmnode isn't start\n");
        return true;
    }
    if (!lmnodeSync.IsSynced()) {
        //there is no budget data to use to check anything, let's just accept the longest chain
        if (fDebug) LogPrintf("IsBlockPayeeValid -- WARNING: Client not synced, skipping block payee checks\n");
        return true;
    }

    //check for lmnode payee
    if (mnpayments.IsTransactionValid(txNew, nBlockHeight)) {
        LogPrint("mnpayments", "IsBlockPayeeValid -- Valid lmnode payment at height %d: %s", nBlockHeight, txNew.ToString());
        return true;
    } else {
        if(sporkManager.IsSporkActive(SPORK_8_LMNODE_PAYMENT_ENFORCEMENT)){
            return false;
        } else {
            LogPrintf("ZNode payment enforcement is disabled, accepting block\n");
            return true;
        }
    }
}

void FillBlockPayments(CMutableTransaction &txNew, int nBlockHeight, CAmount lmnodePayment, CTxOut &txoutLMNodeRet, std::vector <CTxOut> &voutSuperblockRet) {
    // only create superblocks if spork is enabled AND if superblock is actually triggered
    // (height should be validated inside)
//    if(sporkManager.IsSporkActive(SPORK_9_SUPERBLOCKS_ENABLED) &&
//        CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
//            LogPrint("gobject", "FillBlockPayments -- triggered superblock creation at height %d\n", nBlockHeight);
//            CSuperblockManager::CreateSuperblock(txNew, nBlockHeight, voutSuperblockRet);
//            return;
//    }

    // FILL BLOCK PAYEE WITH LMNODE PAYMENT OTHERWISE
    mnpayments.FillBlockPayee(txNew, nBlockHeight, lmnodePayment, txoutLMNodeRet);
    LogPrint("mnpayments", "FillBlockPayments -- nBlockHeight %d lmnodePayment %lld txoutLMNodeRet %s txNew %s",
             nBlockHeight, lmnodePayment, txoutLMNodeRet.ToString(), txNew.ToString());
}

std::string GetRequiredPaymentsString(int nBlockHeight) {
    // IF WE HAVE A ACTIVATED TRIGGER FOR THIS HEIGHT - IT IS A SUPERBLOCK, GET THE REQUIRED PAYEES
//    if(CSuperblockManager::IsSuperblockTriggered(nBlockHeight)) {
//        return CSuperblockManager::GetRequiredPaymentsString(nBlockHeight);
//    }

    // OTHERWISE, PAY LMNODE
    return mnpayments.GetRequiredPaymentsString(nBlockHeight);
}

void CLMNodePayments::Clear() {
    LOCK2(cs_mapLMNodeBlocks, cs_mapLMNodePaymentVotes);
    mapLMNodeBlocks.clear();
    mapLMNodePaymentVotes.clear();
}

bool CLMNodePayments::CanVote(COutPoint outLMNode, int nBlockHeight) {
    LOCK(cs_mapLMNodePaymentVotes);

    if (mapLMNodesLastVote.count(outLMNode) && mapLMNodesLastVote[outLMNode] == nBlockHeight) {
        return false;
    }

    //record this lmnode voted
    mapLMNodesLastVote[outLMNode] = nBlockHeight;
    return true;
}

std::string CLMNodePayee::ToString() const {
    CTxDestination address1;
    ExtractDestination(scriptPubKey, address1);
    CBitcoinAddress address2(address1);
    std::string str;
    str += "(address: ";
    str += address2.ToString();
    str += ")\n";
    return str;
}

/**
*   FillBlockPayee
*
*   Fill LMNode ONLY payment block
*/

void CLMNodePayments::FillBlockPayee(CMutableTransaction &txNew, int nBlockHeight, CAmount lmnodePayment, CTxOut &txoutLMNodeRet) {
    // make sure it's not filled yet
    txoutLMNodeRet = CTxOut();

    CScript payee;
    bool foundMaxVotedPayee = true;

    if (!mnpayments.GetBlockPayee(nBlockHeight, payee)) {
        // no lmnode detected...
        // LogPrintf("no lmnode detected...\n");
        foundMaxVotedPayee = false;
        int nCount = 0;
        CLMNode *winningNode = mnodeman.GetNextLMNodeInQueueForPayment(nBlockHeight, true, nCount);
        if (!winningNode) {
            // ...and we can't calculate it on our own
            LogPrintf("CLMNodePayments::FillBlockPayee -- Failed to detect lmnode to pay\n");
            return;
        }
        // fill payee with locally calculated winner and hope for the best
        payee = GetScriptForDestination(winningNode->pubKeyCollateralAddress.GetID());
        LogPrintf("payee=%s\n", winningNode->ToString());
    }
    txoutLMNodeRet = CTxOut(lmnodePayment, payee);
    txNew.vout.push_back(txoutLMNodeRet);

    CTxDestination address1;
    ExtractDestination(payee, address1);
    CBitcoinAddress address2(address1);
    if (foundMaxVotedPayee) {
        LogPrintf("CLMNodePayments::FillBlockPayee::foundMaxVotedPayee -- LMNode payment %lld to %s\n", lmnodePayment, address2.ToString());
    } else {
        LogPrintf("CLMNodePayments::FillBlockPayee -- LMNode payment %lld to %s\n", lmnodePayment, address2.ToString());
    }

}

int CLMNodePayments::GetMinLMNodePaymentsProto() {
    return sporkManager.IsSporkActive(SPORK_10_LMNODE_PAY_UPDATED_NODES)
           ? MIN_LMNODE_PAYMENT_PROTO_VERSION_2
           : MIN_LMNODE_PAYMENT_PROTO_VERSION_1;
}

void CLMNodePayments::ProcessMessage(CNode *pfrom, std::string &strCommand, CDataStream &vRecv) {

//    LogPrintf("CLMNodePayments::ProcessMessage strCommand=%s\n", strCommand);
    // Ignore any payments messages until lmnode list is synced
    if (!lmnodeSync.IsLMNodeListSynced()) return;

    if (fLiteMode) return; // disable all Dash specific functionality

    if (strCommand == NetMsgType::LMNODEPAYMENTSYNC) { //LMNode Payments Request Sync

        // Ignore such requests until we are fully synced.
        // We could start processing this after lmnode list is synced
        // but this is a heavy one so it's better to finish sync first.
        if (!lmnodeSync.IsSynced()) return;

        int nCountNeeded;
        vRecv >> nCountNeeded;

        if (netfulfilledman.HasFulfilledRequest(pfrom->addr, NetMsgType::LMNODEPAYMENTSYNC)) {
            // Asking for the payments list multiple times in a short period of time is no good
            LogPrintf("LMNODEPAYMENTSYNC -- peer already asked me for the list, peer=%d\n", pfrom->id);
            Misbehaving(pfrom->GetId(), 20);
            return;
        }
        netfulfilledman.AddFulfilledRequest(pfrom->addr, NetMsgType::LMNODEPAYMENTSYNC);

        Sync(pfrom);
        LogPrint("mnpayments", "LMNODEPAYMENTSYNC -- Sent LMNode payment votes to peer %d\n", pfrom->id);

    } else if (strCommand == NetMsgType::LMNODEPAYMENTVOTE) { // LMNode Payments Vote for the Winner

        CLMNodePaymentVote vote;
        vRecv >> vote;

        if (pfrom->nVersion < GetMinLMNodePaymentsProto()) return;

        if (!pCurrentBlockIndex) return;

        uint256 nHash = vote.GetHash();

        pfrom->setAskFor.erase(nHash);

        {
            LOCK(cs_mapLMNodePaymentVotes);
            if (mapLMNodePaymentVotes.count(nHash)) {
                LogPrint("mnpayments", "LMNODEPAYMENTVOTE -- hash=%s, nHeight=%d seen\n", nHash.ToString(), pCurrentBlockIndex->nHeight);
                return;
            }

            // Avoid processing same vote multiple times
            mapLMNodePaymentVotes[nHash] = vote;
            // but first mark vote as non-verified,
            // AddPaymentVote() below should take care of it if vote is actually ok
            mapLMNodePaymentVotes[nHash].MarkAsNotVerified();
        }

        int nFirstBlock = pCurrentBlockIndex->nHeight - GetStorageLimit();
        if (vote.nBlockHeight < nFirstBlock || vote.nBlockHeight > pCurrentBlockIndex->nHeight + 20) {
            LogPrint("mnpayments", "LMNODEPAYMENTVOTE -- vote out of range: nFirstBlock=%d, nBlockHeight=%d, nHeight=%d\n", nFirstBlock, vote.nBlockHeight, pCurrentBlockIndex->nHeight);
            return;
        }

        std::string strError = "";
        if (!vote.IsValid(pfrom, pCurrentBlockIndex->nHeight, strError)) {
            LogPrint("mnpayments", "LMNODEPAYMENTVOTE -- invalid message, error: %s\n", strError);
            return;
        }

        if (!CanVote(vote.vinLMNode.prevout, vote.nBlockHeight)) {
            LogPrintf("LMNODEPAYMENTVOTE -- lmnode already voted, lmnode=%s\n", vote.vinLMNode.prevout.ToStringShort());
            return;
        }

        lmnode_info_t mnInfo = mnodeman.GetLMNodeInfo(vote.vinLMNode);
        if (!mnInfo.fInfoValid) {
            // mn was not found, so we can't check vote, some info is probably missing
            LogPrintf("LMNODEPAYMENTVOTE -- lmnode is missing %s\n", vote.vinLMNode.prevout.ToStringShort());
            mnodeman.AskForMN(pfrom, vote.vinLMNode);
            return;
        }

        int nDos = 0;
        if (!vote.CheckSignature(mnInfo.pubKeyLMNode, pCurrentBlockIndex->nHeight, nDos)) {
            if (nDos) {
                LogPrintf("LMNODEPAYMENTVOTE -- ERROR: invalid signature\n");
                Misbehaving(pfrom->GetId(), nDos);
            } else {
                // only warn about anything non-critical (i.e. nDos == 0) in debug mode
                LogPrint("mnpayments", "LMNODEPAYMENTVOTE -- WARNING: invalid signature\n");
            }
            // Either our info or vote info could be outdated.
            // In case our info is outdated, ask for an update,
            mnodeman.AskForMN(pfrom, vote.vinLMNode);
            // but there is nothing we can do if vote info itself is outdated
            // (i.e. it was signed by a mn which changed its key),
            // so just quit here.
            return;
        }

        CTxDestination address1;
        ExtractDestination(vote.payee, address1);
        CBitcoinAddress address2(address1);

        LogPrint("mnpayments", "LMNODEPAYMENTVOTE -- vote: address=%s, nBlockHeight=%d, nHeight=%d, prevout=%s\n", address2.ToString(), vote.nBlockHeight, pCurrentBlockIndex->nHeight, vote.vinLMNode.prevout.ToStringShort());

        if (AddPaymentVote(vote)) {
            vote.Relay();
            lmnodeSync.AddedPaymentVote();
        }
    }
}

bool CLMNodePaymentVote::Sign() {
    std::string strError;
    std::string strMessage = vinLMNode.prevout.ToStringShort() +
                             boost::lexical_cast<std::string>(nBlockHeight) +
                             ScriptToAsmStr(payee);

    if (!darkSendSigner.SignMessage(strMessage, vchSig, activeLMNode.keyLMNode)) {
        LogPrintf("CLMNodePaymentVote::Sign -- SignMessage() failed\n");
        return false;
    }

    if (!darkSendSigner.VerifyMessage(activeLMNode.pubKeyLMNode, vchSig, strMessage, strError)) {
        LogPrintf("CLMNodePaymentVote::Sign -- VerifyMessage() failed, error: %s\n", strError);
        return false;
    }

    return true;
}

bool CLMNodePayments::GetBlockPayee(int nBlockHeight, CScript &payee) {
    if (mapLMNodeBlocks.count(nBlockHeight)) {
        return mapLMNodeBlocks[nBlockHeight].GetBestPayee(payee);
    }

    return false;
}

// Is this lmnode scheduled to get paid soon?
// -- Only look ahead up to 8 blocks to allow for propagation of the latest 2 blocks of votes
bool CLMNodePayments::IsScheduled(CLMNode &mn, int nNotBlockHeight) {
    LOCK(cs_mapLMNodeBlocks);

    if (!pCurrentBlockIndex) return false;

    CScript mnpayee;
    mnpayee = GetScriptForDestination(mn.pubKeyCollateralAddress.GetID());

    CScript payee;
    for (int64_t h = pCurrentBlockIndex->nHeight; h <= pCurrentBlockIndex->nHeight + 8; h++) {
        if (h == nNotBlockHeight) continue;
        if (mapLMNodeBlocks.count(h) && mapLMNodeBlocks[h].GetBestPayee(payee) && mnpayee == payee) {
            return true;
        }
    }

    return false;
}

bool CLMNodePayments::AddPaymentVote(const CLMNodePaymentVote &vote) {
    LogPrint("lmnode-payments", "CLMNodePayments::AddPaymentVote\n");
    uint256 blockHash = uint256();
    if (!GetBlockHash(blockHash, vote.nBlockHeight - 101)) return false;

    if (HasVerifiedPaymentVote(vote.GetHash())) return false;

    LOCK2(cs_mapLMNodeBlocks, cs_mapLMNodePaymentVotes);

    mapLMNodePaymentVotes[vote.GetHash()] = vote;

    if (!mapLMNodeBlocks.count(vote.nBlockHeight)) {
        CLMNodeBlockPayees blockPayees(vote.nBlockHeight);
        mapLMNodeBlocks[vote.nBlockHeight] = blockPayees;
    }

    mapLMNodeBlocks[vote.nBlockHeight].AddPayee(vote);

    return true;
}

bool CLMNodePayments::HasVerifiedPaymentVote(uint256 hashIn) {
    LOCK(cs_mapLMNodePaymentVotes);
    std::map<uint256, CLMNodePaymentVote>::iterator it = mapLMNodePaymentVotes.find(hashIn);
    return it != mapLMNodePaymentVotes.end() && it->second.IsVerified();
}

void CLMNodeBlockPayees::AddPayee(const CLMNodePaymentVote &vote) {
    LOCK(cs_vecPayees);

    BOOST_FOREACH(CLMNodePayee & payee, vecPayees)
    {
        if (payee.GetPayee() == vote.payee) {
            payee.AddVoteHash(vote.GetHash());
            return;
        }
    }
    CLMNodePayee payeeNew(vote.payee, vote.GetHash());
    vecPayees.push_back(payeeNew);
}

bool CLMNodeBlockPayees::GetBestPayee(CScript &payeeRet) {
    LOCK(cs_vecPayees);
    LogPrint("mnpayments", "CLMNodeBlockPayees::GetBestPayee, vecPayees.size()=%s\n", vecPayees.size());
    if (!vecPayees.size()) {
        LogPrint("mnpayments", "CLMNodeBlockPayees::GetBestPayee -- ERROR: couldn't find any payee\n");
        return false;
    }

    int nVotes = -1;
    BOOST_FOREACH(CLMNodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() > nVotes) {
            payeeRet = payee.GetPayee();
            nVotes = payee.GetVoteCount();
        }
    }

    return (nVotes > -1);
}

bool CLMNodeBlockPayees::HasPayeeWithVotes(CScript payeeIn, int nVotesReq) {
    LOCK(cs_vecPayees);

    BOOST_FOREACH(CLMNodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= nVotesReq && payee.GetPayee() == payeeIn) {
            return true;
        }
    }

//    LogPrint("mnpayments", "CLMNodeBlockPayees::HasPayeeWithVotes -- ERROR: couldn't find any payee with %d+ votes\n", nVotesReq);
    return false;
}

bool CLMNodeBlockPayees::IsTransactionValid(const CTransaction &txNew) {
    LOCK(cs_vecPayees);

    int nMaxSignatures = 0;
    std::string strPayeesPossible = "";

    CAmount nLMNodePayment = GetLMNodePayment(nBlockHeight, txNew.GetValueOut());

    //require at least MNPAYMENTS_SIGNATURES_REQUIRED signatures

    BOOST_FOREACH(CLMNodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= nMaxSignatures) {
            nMaxSignatures = payee.GetVoteCount();
        }
    }

    // if we don't have at least MNPAYMENTS_SIGNATURES_REQUIRED signatures on a payee, approve whichever is the longest chain
    if (nMaxSignatures < MNPAYMENTS_SIGNATURES_REQUIRED) return true;

    bool hasValidPayee = false;

    BOOST_FOREACH(CLMNodePayee & payee, vecPayees)
    {
        if (payee.GetVoteCount() >= MNPAYMENTS_SIGNATURES_REQUIRED) {
            hasValidPayee = true;

            BOOST_FOREACH(CTxOut txout, txNew.vout) {
                if (payee.GetPayee() == txout.scriptPubKey && nLMNodePayment == txout.nValue) {
                    LogPrint("mnpayments", "CLMNodeBlockPayees::IsTransactionValid -- Found required payment\n");
                    return true;
                }
            }

            CTxDestination address1;
            ExtractDestination(payee.GetPayee(), address1);
            CBitcoinAddress address2(address1);

            if (strPayeesPossible == "") {
                strPayeesPossible = address2.ToString();
            } else {
                strPayeesPossible += "," + address2.ToString();
            }
        }
    }

    if (!hasValidPayee) return true;

    LogPrintf("CLMNodeBlockPayees::IsTransactionValid -- ERROR: Missing required payment, possible payees: '%s', amount: %f HPP\n", strPayeesPossible, (float) nLMNodePayment / COIN);
    return false;
}

std::string CLMNodeBlockPayees::GetRequiredPaymentsString() {
    LOCK(cs_vecPayees);

    std::string strRequiredPayments = "Unknown";

    BOOST_FOREACH(CLMNodePayee & payee, vecPayees)
    {
        CTxDestination address1;
        ExtractDestination(payee.GetPayee(), address1);
        CBitcoinAddress address2(address1);

        if (strRequiredPayments != "Unknown") {
            strRequiredPayments += ", " + address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.GetVoteCount());
        } else {
            strRequiredPayments = address2.ToString() + ":" + boost::lexical_cast<std::string>(payee.GetVoteCount());
        }
    }

    return strRequiredPayments;
}

std::string CLMNodePayments::GetRequiredPaymentsString(int nBlockHeight) {
    LOCK(cs_mapLMNodeBlocks);

    if (mapLMNodeBlocks.count(nBlockHeight)) {
        return mapLMNodeBlocks[nBlockHeight].GetRequiredPaymentsString();
    }

    return "Unknown";
}

bool CLMNodePayments::IsTransactionValid(const CTransaction &txNew, int nBlockHeight) {
    LOCK(cs_mapLMNodeBlocks);

    if (mapLMNodeBlocks.count(nBlockHeight)) {
        return mapLMNodeBlocks[nBlockHeight].IsTransactionValid(txNew);
    }

    return true;
}

void CLMNodePayments::CheckAndRemove() {
    if (!pCurrentBlockIndex) return;

    LOCK2(cs_mapLMNodeBlocks, cs_mapLMNodePaymentVotes);

    int nLimit = GetStorageLimit();

    std::map<uint256, CLMNodePaymentVote>::iterator it = mapLMNodePaymentVotes.begin();
    while (it != mapLMNodePaymentVotes.end()) {
        CLMNodePaymentVote vote = (*it).second;

        if (pCurrentBlockIndex->nHeight - vote.nBlockHeight > nLimit) {
            LogPrint("mnpayments", "CLMNodePayments::CheckAndRemove -- Removing old LMNode payment: nBlockHeight=%d\n", vote.nBlockHeight);
            mapLMNodePaymentVotes.erase(it++);
            mapLMNodeBlocks.erase(vote.nBlockHeight);
        } else {
            ++it;
        }
    }
    LogPrintf("CLMNodePayments::CheckAndRemove -- %s\n", ToString());
}

bool CLMNodePaymentVote::IsValid(CNode *pnode, int nValidationHeight, std::string &strError) {
    CLMNode *pmn = mnodeman.Find(vinLMNode);

    if (!pmn) {
        strError = strprintf("Unknown LMNode: prevout=%s", vinLMNode.prevout.ToStringShort());
        // Only ask if we are already synced and still have no idea about that LMNode
        if (lmnodeSync.IsLMNodeListSynced()) {
            mnodeman.AskForMN(pnode, vinLMNode);
        }

        return false;
    }

    int nMinRequiredProtocol;
    if (nBlockHeight >= nValidationHeight) {
        // new votes must comply SPORK_10_LMNODE_PAY_UPDATED_NODES rules
        nMinRequiredProtocol = mnpayments.GetMinLMNodePaymentsProto();
    } else {
        // allow non-updated lmnodes for old blocks
        nMinRequiredProtocol = MIN_LMNODE_PAYMENT_PROTO_VERSION_1;
    }

    if (pmn->nProtocolVersion < nMinRequiredProtocol) {
        strError = strprintf("LMNode protocol is too old: nProtocolVersion=%d, nMinRequiredProtocol=%d", pmn->nProtocolVersion, nMinRequiredProtocol);
        return false;
    }

    // Only lmnodes should try to check lmnode rank for old votes - they need to pick the right winner for future blocks.
    // Regular clients (miners included) need to verify lmnode rank for future block votes only.
    if (!fZNode && nBlockHeight < nValidationHeight) return true;

    int nRank = mnodeman.GetLMNodeRank(vinLMNode, nBlockHeight - 101, nMinRequiredProtocol, false);

    if (nRank == -1) {
        LogPrint("mnpayments", "CLMNodePaymentVote::IsValid -- Can't calculate rank for lmnode %s\n",
                 vinLMNode.prevout.ToStringShort());
        return false;
    }

    if (nRank > MNPAYMENTS_SIGNATURES_TOTAL) {
        // It's common to have lmnodes mistakenly think they are in the top 10
        // We don't want to print all of these messages in normal mode, debug mode should print though
        strError = strprintf("LMNode is not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL, nRank);
        // Only ban for new mnw which is out of bounds, for old mnw MN list itself might be way too much off
        if (nRank > MNPAYMENTS_SIGNATURES_TOTAL * 2 && nBlockHeight > nValidationHeight) {
            strError = strprintf("LMNode is not in the top %d (%d)", MNPAYMENTS_SIGNATURES_TOTAL * 2, nRank);
            LogPrintf("CLMNodePaymentVote::IsValid -- Error: %s\n", strError);
            Misbehaving(pnode->GetId(), 20);
        }
        // Still invalid however
        return false;
    }

    return true;
}

bool CLMNodePayments::ProcessBlock(int nBlockHeight) {

    // DETERMINE IF WE SHOULD BE VOTING FOR THE NEXT PAYEE

    if (fLiteMode || !fZNode) {
        return false;
    }

    // We have little chances to pick the right winner if winners list is out of sync
    // but we have no choice, so we'll try. However it doesn't make sense to even try to do so
    // if we have not enough data about lmnodes.
    if (!lmnodeSync.IsLMNodeListSynced()) {
        return false;
    }

    int nRank = mnodeman.GetLMNodeRank(activeLMNode.vin, nBlockHeight - 101, GetMinLMNodePaymentsProto(), false);

    if (nRank == -1) {
        LogPrint("mnpayments", "CLMNodePayments::ProcessBlock -- Unknown LMNode\n");
        return false;
    }

    if (nRank > MNPAYMENTS_SIGNATURES_TOTAL) {
        LogPrint("mnpayments", "CLMNodePayments::ProcessBlock -- LMNode not in the top %d (%d)\n", MNPAYMENTS_SIGNATURES_TOTAL, nRank);
        return false;
    }

    // LOCATE THE NEXT LMNODE WHICH SHOULD BE PAID

    LogPrintf("CLMNodePayments::ProcessBlock -- Start: nBlockHeight=%d, lmnode=%s\n", nBlockHeight, activeLMNode.vin.prevout.ToStringShort());

    // pay to the oldest MN that still had no payment but its input is old enough and it was active long enough
    int nCount = 0;
    CLMNode *pmn = mnodeman.GetNextLMNodeInQueueForPayment(nBlockHeight, true, nCount);

    if (pmn == NULL) {
        LogPrintf("CLMNodePayments::ProcessBlock -- ERROR: Failed to find lmnode to pay\n");
        return false;
    }

    LogPrintf("CLMNodePayments::ProcessBlock -- LMNode found by GetNextLMNodeInQueueForPayment(): %s\n", pmn->vin.prevout.ToStringShort());


    CScript payee = GetScriptForDestination(pmn->pubKeyCollateralAddress.GetID());

    CLMNodePaymentVote voteNew(activeLMNode.vin, nBlockHeight, payee);

    CTxDestination address1;
    ExtractDestination(payee, address1);
    CBitcoinAddress address2(address1);

    // SIGN MESSAGE TO NETWORK WITH OUR LMNODE KEYS

    if (voteNew.Sign()) {
        if (AddPaymentVote(voteNew)) {
            voteNew.Relay();
            return true;
        }
    }

    return false;
}

void CLMNodePaymentVote::Relay() {
    // do not relay until synced
    if (!lmnodeSync.IsWinnersListSynced()) {
        LogPrintf("CLMNodePaymentVote::Relay - lmnodeSync.IsWinnersListSynced() not sync\n");
        return;
    }
    CInv inv(MSG_LMNODE_PAYMENT_VOTE, GetHash());
    RelayInv(inv);
}

bool CLMNodePaymentVote::CheckSignature(const CPubKey &pubKeyLMNode, int nValidationHeight, int &nDos) {
    // do not ban by default
    nDos = 0;

    std::string strMessage = vinLMNode.prevout.ToStringShort() +
                             boost::lexical_cast<std::string>(nBlockHeight) +
                             ScriptToAsmStr(payee);

    std::string strError = "";
    if (!darkSendSigner.VerifyMessage(pubKeyLMNode, vchSig, strMessage, strError)) {
        // Only ban for future block vote when we are already synced.
        // Otherwise it could be the case when MN which signed this vote is using another key now
        // and we have no idea about the old one.
        if (lmnodeSync.IsLMNodeListSynced() && nBlockHeight > nValidationHeight) {
            nDos = 20;
        }
        return error("CLMNodePaymentVote::CheckSignature -- Got bad LMNode payment signature, lmnode=%s, error: %s", vinLMNode.prevout.ToStringShort().c_str(), strError);
    }

    return true;
}

std::string CLMNodePaymentVote::ToString() const {
    std::ostringstream info;

    info << vinLMNode.prevout.ToStringShort() <<
         ", " << nBlockHeight <<
         ", " << ScriptToAsmStr(payee) <<
         ", " << (int) vchSig.size();

    return info.str();
}

// Send only votes for future blocks, node should request every other missing payment block individually
void CLMNodePayments::Sync(CNode *pnode) {
    LOCK(cs_mapLMNodeBlocks);

    if (!pCurrentBlockIndex) return;

    int nInvCount = 0;

    for (int h = pCurrentBlockIndex->nHeight; h < pCurrentBlockIndex->nHeight + 20; h++) {
        if (mapLMNodeBlocks.count(h)) {
            BOOST_FOREACH(CLMNodePayee & payee, mapLMNodeBlocks[h].vecPayees)
            {
                std::vector <uint256> vecVoteHashes = payee.GetVoteHashes();
                BOOST_FOREACH(uint256 & hash, vecVoteHashes)
                {
                    if (!HasVerifiedPaymentVote(hash)) continue;
                    pnode->PushInventory(CInv(MSG_LMNODE_PAYMENT_VOTE, hash));
                    nInvCount++;
                }
            }
        }
    }

    LogPrintf("CLMNodePayments::Sync -- Sent %d votes to peer %d\n", nInvCount, pnode->id);
    pnode->PushMessage(NetMsgType::SYNCSTATUSCOUNT, LMNODE_SYNC_MNW, nInvCount);
}

// Request low data/unknown payment blocks in batches directly from some node instead of/after preliminary Sync.
void CLMNodePayments::RequestLowDataPaymentBlocks(CNode *pnode) {
    if (!pCurrentBlockIndex) return;

    LOCK2(cs_main, cs_mapLMNodeBlocks);

    std::vector <CInv> vToFetch;
    int nLimit = GetStorageLimit();

    const CBlockIndex *pindex = pCurrentBlockIndex;

    while (pCurrentBlockIndex->nHeight - pindex->nHeight < nLimit) {
        if (!mapLMNodeBlocks.count(pindex->nHeight)) {
            // We have no idea about this block height, let's ask
            vToFetch.push_back(CInv(MSG_LMNODE_PAYMENT_BLOCK, pindex->GetBlockHash()));
            // We should not violate GETDATA rules
            if (vToFetch.size() == MAX_INV_SZ) {
                LogPrintf("CLMNodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d blocks\n", pnode->id, MAX_INV_SZ);
                pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
                // Start filling new batch
                vToFetch.clear();
            }
        }
        if (!pindex->pprev) break;
        pindex = pindex->pprev;
    }

    std::map<int, CLMNodeBlockPayees>::iterator it = mapLMNodeBlocks.begin();

    while (it != mapLMNodeBlocks.end()) {
        int nTotalVotes = 0;
        bool fFound = false;
        BOOST_FOREACH(CLMNodePayee & payee, it->second.vecPayees)
        {
            if (payee.GetVoteCount() >= MNPAYMENTS_SIGNATURES_REQUIRED) {
                fFound = true;
                break;
            }
            nTotalVotes += payee.GetVoteCount();
        }
        // A clear winner (MNPAYMENTS_SIGNATURES_REQUIRED+ votes) was found
        // or no clear winner was found but there are at least avg number of votes
        if (fFound || nTotalVotes >= (MNPAYMENTS_SIGNATURES_TOTAL + MNPAYMENTS_SIGNATURES_REQUIRED) / 2) {
            // so just move to the next block
            ++it;
            continue;
        }
        // DEBUG
//        DBG (
//            // Let's see why this failed
//            BOOST_FOREACH(CLMNodePayee& payee, it->second.vecPayees) {
//                CTxDestination address1;
//                ExtractDestination(payee.GetPayee(), address1);
//                CBitcoinAddress address2(address1);
//                printf("payee %s votes %d\n", address2.ToString().c_str(), payee.GetVoteCount());
//            }
//            printf("block %d votes total %d\n", it->first, nTotalVotes);
//        )
        // END DEBUG
        // Low data block found, let's try to sync it
        uint256 hash;
        if (GetBlockHash(hash, it->first)) {
            vToFetch.push_back(CInv(MSG_LMNODE_PAYMENT_BLOCK, hash));
        }
        // We should not violate GETDATA rules
        if (vToFetch.size() == MAX_INV_SZ) {
            LogPrintf("CLMNodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d payment blocks\n", pnode->id, MAX_INV_SZ);
            pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
            // Start filling new batch
            vToFetch.clear();
        }
        ++it;
    }
    // Ask for the rest of it
    if (!vToFetch.empty()) {
        LogPrintf("CLMNodePayments::SyncLowDataPaymentBlocks -- asking peer %d for %d payment blocks\n", pnode->id, vToFetch.size());
        pnode->PushMessage(NetMsgType::GETDATA, vToFetch);
    }
}

std::string CLMNodePayments::ToString() const {
    std::ostringstream info;

    info << "Votes: " << (int) mapLMNodePaymentVotes.size() <<
         ", Blocks: " << (int) mapLMNodeBlocks.size();

    return info.str();
}

bool CLMNodePayments::IsEnoughData() {
    float nAverageVotes = (MNPAYMENTS_SIGNATURES_TOTAL + MNPAYMENTS_SIGNATURES_REQUIRED) / 2;
    int nStorageLimit = GetStorageLimit();
    return GetBlockCount() > nStorageLimit && GetVoteCount() > nStorageLimit * nAverageVotes;
}

int CLMNodePayments::GetStorageLimit() {
    return std::max(int(mnodeman.size() * nStorageCoeff), nMinBlocksToStore);
}

void CLMNodePayments::UpdatedBlockTip(const CBlockIndex *pindex) {
    pCurrentBlockIndex = pindex;
    LogPrint("mnpayments", "CLMNodePayments::UpdatedBlockTip -- pCurrentBlockIndex->nHeight=%d\n", pCurrentBlockIndex->nHeight);
    
    ProcessBlock(pindex->nHeight + 5);
}
