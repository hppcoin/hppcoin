// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Copyright (c) 2016-2017 The Zcoin Core developers
// Copyright (c) 2017-2018 The Hppcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

static map<int, uint256> mapPoWHash;

std::string precomputedHash[20501] = { "d9b7e1b1c29026cb5c571ccf941a92b207f7dc3f1088e9d449f4d01e48979da9"
};

void buildMapPoWHash() {
    for (int i=0; i<1; i++) {
        mapPoWHash.insert(make_pair(i, uint256S(precomputedHash[i])));
    }
};