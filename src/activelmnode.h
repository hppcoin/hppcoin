// Copyright (c) 2014-2017 The Dash Core developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef ACTIVELMNODE_H
#define ACTIVELMNODE_H

#include "net.h"
#include "key.h"
#include "wallet/wallet.h"

class CActiveLMNode;

static const int ACTIVE_LMNODE_INITIAL          = 0; // initial state
static const int ACTIVE_LMNODE_SYNC_IN_PROCESS  = 1;
static const int ACTIVE_LMNODE_INPUT_TOO_NEW    = 2;
static const int ACTIVE_LMNODE_NOT_CAPABLE      = 3;
static const int ACTIVE_LMNODE_STARTED          = 4;

extern CActiveLMNode activeLMNode;

// Responsible for activating the LMNode and pinging the network
class CActiveLMNode
{
public:
    enum lmnode_type_enum_t {
        LMNODE_UNKNOWN = 0,
        LMNODE_REMOTE  = 1,
        LMNODE_LOCAL   = 2
    };

private:
    // critical section to protect the inner data structures
    mutable CCriticalSection cs;

    lmnode_type_enum_t eType;

    bool fPingerEnabled;

    /// Ping LMNode
    bool SendLMNodePing();

public:
    // Keys for the active LMNode
    CPubKey pubKeyLMNode;
    CKey keyLMNode;

    // Initialized while registering LMNode
    CTxIn vin;
    CService service;

    int nState; // should be one of ACTIVE_LMNODE_XXXX
    std::string strNotCapableReason;

    CActiveLMNode()
        : eType(LMNODE_UNKNOWN),
          fPingerEnabled(false),
          pubKeyLMNode(),
          keyLMNode(),
          vin(),
          service(),
          nState(ACTIVE_LMNODE_INITIAL)
    {}

    /// Manage state of active LMNode
    void ManageState();

    std::string GetStateString() const;
    std::string GetStatus() const;
    std::string GetTypeString() const;

private:
    void ManageStateInitial();
    void ManageStateRemote();
    void ManageStateLocal();
};

#endif
