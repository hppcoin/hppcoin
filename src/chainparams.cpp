// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Copyright (c) 2016-2017 The Zcoin developers
// Copyright (c) 2017-2018 The Hppcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/merkle.h"
#include "consensus/consensus.h"

#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"
#include "libzerocoin/bitcoin_bignum/bignum.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

#include "chainparamsseeds.h"
#include "arith_uint256.h"


static CBlock CreateGenesisBlock(const char *pszTimestamp, const CScript &genesisOutputScript, uint32_t nTime, uint32_t nNonce,
                   uint32_t nBits, int32_t nVersion, const CAmount &genesisReward,
                   std::vector<unsigned char> extraNonce) {
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
//    CScriptNum csn = CScriptNum(4);
//    std::cout << "CScriptNum(4):" << csn.GetHex();
//    CBigNum cbn = CBigNum(4);
//    std::cout << "CBigNum(4):" << cbn.GetHex();
    txNew.vin[0].scriptSig = CScript() << 504365040 << CBigNum(4).getvch() << std::vector < unsigned char >
    ((const unsigned char *) pszTimestamp, (const unsigned char *) pszTimestamp + strlen(pszTimestamp)) << extraNonce;
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime = nTime;
    genesis.nBits = nBits;
    genesis.nNonce = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(txNew);
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount &genesisReward,
                   std::vector<unsigned char> extraNonce) {
const char *pszTimestamp = "Forbes NOV 15, 2017 Apollo 11 ... With AI And High-Performance Computing";
const CScript genesisOutputScript = CScript();
return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward,
                              extraNonce);
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = "main";
        consensus.nSubsidyHalvingInterval = 262500;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.nMinNFactor = 10;
        consensus.nMaxNFactor = 30;
        //change on 22 Nov to 1511365800
      consensus.nChainStartTime = 1511365800;
//   consensus.nChainStartTime = 1510834951;
        consensus.BIP34Height = 227931;
        consensus.BIP34Hash = uint256S("0x000000000000024b89b42a942fe0d9fea3bb44ab7bd1b19115dd6a759c0808b8");
        consensus.powLimit = uint256S("00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        
        consensus.nPowTargetTimespan = 30 * 60; // 30 minutes between retargets
        consensus.nPowTargetSpacing = 4 * 60; // 4 minute blocks
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1916; // 95% of 2016
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1510044919; 
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1570044919; 

        // Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1462060800; // May 1st, 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1493596800; // May 1st, 2017
// Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1479168000; // November 15th, 2016.
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1510704000; // November 15th, 2017.

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000708f98bf623f02e");

        // lmnode params
        consensus.nLMNodePaymentsStartBlock = HF_LMNODE_PAYMENT_START; // not true, but it's ok as long as it's less then nLMNodePaymentsIncreaseBlock
        // consensus.nLMNodePaymentsIncreaseBlock = 680000; // actual historical value // not used for now, probably later
        // consensus.nLMNodePaymentsIncreasePeriod = 576*30; // 17280 - actual historical value // not used for now, probably later
        // consensus.nSuperblockStartBlock = 614820;
        // consensus.nBudgetPaymentsStartBlock = 328008; // actual historical value
        // consensus.nBudgetPaymentsCycleBlocks = 16616; // ~(60*24*30)/2.6, actual number of blocks per month is 200700 / 12 = 16725
        // consensus.nBudgetPaymentsWindowBlocks = 100;
        nMaxTipAge = 6 * 60 * 60; // ~144 blocks behind -> 2 x fork detection time, was 24 * 60 * 60 in bitcoin

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 60*60; // fulfilled requests expire in 1 hour
        strSporkPubKey = "04549ac134f694c0243f503e8c8a9a986f5de6610049c40b07816809b0d1d06a21b07be27b9bb555931773f62ba6cf35a25fd52f694d4e1106ccd237a7bb899fdd";
        strLMNodePaymentsPubKey = "04549ac134f694c0243f503e8c8a9a986f5de6610049c40b07816809b0d1d06a21b07be27b9bb555931773f62ba6cf35a25fd52f694d4e1106ccd237a7bb899fdd";

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
       `  * a large 32-bit integer with any alignment.
         */
        //btzc: update hppcoin pchMessage
        pchMessageStart[0] = 0xe3;
        pchMessageStart[1] = 0xd9;
        pchMessageStart[2] = 0xfe;
        pchMessageStart[3] = 0xf1;
        nDefaultPort = 28878;
        nPruneAfterHeight = 100000;
        /**
         * btzc: hppcoin init genesis block
         * nBits = 0x1e0ffff0
         * nTime = 1414776286
         * nNonce = 142392
         * genesisReward = 0 * COIN
         * nVersion = 2
         * extraNonce
         */
        std::vector<unsigned char> extraNonce(4);
        extraNonce[0] = 0x82;
        extraNonce[1] = 0x3f;
        extraNonce[2] = 0x00;
        extraNonce[3] = 0x00;
         genesis = CreateGenesisBlock(1511365800, 180034036, 0x1e0ffff0, 2, 0 * COIN, extraNonce);
        consensus.hashGenesisBlock = genesis.GetHash();
        //  std::cout << "hppcoin  genesisBlock hash: " << consensus.hashGenesisBlock.ToString() << std::endl;
//  std::cout << "hppcoin  hashMerkleRoot hash: " << genesis.hashMerkleRoot.ToString() << std::endl;
//btzc: update main hppcoin hashGenesisBlock and hashMerkleRoot
assert(consensus.hashGenesisBlock ==
uint256S("0x6978fc5b6ca00dd3a2d6d56ed2c38246b2b58405052dc0b7eb860b9daff8f01a"));
assert(genesis.hashMerkleRoot ==
uint256S("0x168d6932a421c7c0d0ece16d00aab1a4690c80f8845e3d17641b335ad5e406a4"));

        vSeeds.push_back(CDNSSeedData("s1.hppcoin.org", "s1.hppcoin.org", false));
        vSeeds.push_back(CDNSSeedData("s2.hppcoin.org", "s2.hppcoin.org", false));
        vSeeds.push_back(CDNSSeedData("denver.hppcoin.org", "denver.hppcoin.org", false));
        vSeeds.push_back(CDNSSeedData("paris.hppcoin.org", "paris.hppcoin.org", false));
        vSeeds.push_back(CDNSSeedData("nice.hppcoin.org", "nice.hppcoin.org", false));
        vSeeds.push_back(CDNSSeedData("china.hppcoin.org", "china.hppcoin.org", false));
        vSeeds.push_back(CDNSSeedData("italy.hppcoin.org", "italy.hppcoin.org", false));
        vSeeds.push_back(CDNSSeedData("germany.hppcoin.org", "germany.hppcoin.org", false));
        vSeeds.push_back(CDNSSeedData("africa.hppcoin.org", "africa.hppcoin.org", false));
        // Note that of those with the service bits flag, most only support a subset of possible options
         base58Prefixes[PUBKEY_ADDRESS] = std::vector < unsigned char > (1, 100);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector < unsigned char > (1, 7);
        base58Prefixes[SECRET_KEY] = std::vector < unsigned char > (1, 210);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x88)(0xB2)(0x1E).convert_to_container < std::vector < unsigned char > > ();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x88)(0xAD)(0xE4).convert_to_container < std::vector < unsigned char > > ();
        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));


       fMiningRequiresPeers = true;
fDefaultConsistencyChecks = false;
fRequireStandard = true;
fMineBlocksOnDemand = false;
fTestnetToBeDeprecatedFieldRPC = false;
        checkpointData = (CCheckpointData) {
//change on 22 Nov to 1511365800 and right hash
                boost::assign::map_list_of
                (0, uint256S("0x6978fc5b6ca00dd3a2d6d56ed2c38246b2b58405052dc0b7eb860b9daff8f01a"))
                        ,
1511365800, // * UNIX timestamp of last checkpoint block
1,    // * total number of transactions between genesis and last checkpoint
//   (the tx=... number in the SetBestChain debug.log lines)
1200.0     // * estimated number of transactions per day after checkpoint
        };
    }
};


static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = "test";
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.nMajorityEnforceBlockUpgrade = 51;
        consensus.nMajorityRejectBlockOutdated = 75;
        consensus.nMajorityWindow = 100;
        consensus.nMinNFactor = 10;
        consensus.nMaxNFactor = 30;
//change on 22 Nov to 1511365800
      consensus.nChainStartTime = 1511365800;
//   consensus.nChainStartTime = 1510834951;
        consensus.BIP34Height = 21111;
        consensus.BIP34Hash = uint256S("0x0000000023b3a96d3484e5abb3755c413e7d41500f8e2a5c3f0dd01299cd8ef8");
        consensus.powLimit = uint256S("00ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 120; // 60 minutes between retargets
        consensus.nPowTargetSpacing =  30; // 30 sec blocks
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 1512; // 75% for testchains
        consensus.nMinerConfirmationWindow = 2016; // nPowTargetTimespan / nPowTargetSpacing
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1511365800; 
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1570044919; 
// Deployment of BIP68, BIP112, and BIP113.
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1456790400; // March 1st, 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1493596800; // May 1st, 2017
// Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1462060800; // May 1st 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 1493596800; // May 1st 2017

       // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x0000000000000000000000000000000000000000000000000708f98bf623f02e");
        // LMNode params testnet
        consensus.nLMNodePaymentsStartBlock = 1500; // not true, but it's ok as long as it's less then n
        //consensus.nLMNodePaymentsIncreaseBlock = 360; // not used for now, probably later
        //consensus.nLMNodePaymentsIncreasePeriod = 650; // not used for now, probably later
        //consensus.nSuperblockStartBlock = 61000;
        //consensus.nBudgetPaymentsStartBlock = 60000;
        //consensus.nBudgetPaymentsCycleBlocks = 50;
        //consensus.nBudgetPaymentsWindowBlocks = 10;
        nMaxTipAge = 0x7fffffff; // allow mining on top of old blocks for testnet

        nPoolMaxTransactions = 3;
        nFulfilledRequestExpireTime = 5*60; // fulfilled requests expire in 5 minutes
        strSporkPubKey = "046f78dcf911fbd61910136f7f0f8d90578f68d0b3ac973b5040fb7afb501b5939f39b108b0569dca71488f5bbf498d92e4d1194f6f941307ffd95f75e76869f0e";
        strLMNodePaymentsPubKey = "046f78dcf911fbd61910136f7f0f8d90578f68d0b3ac973b5040fb7afb501b5939f39b108b0569dca71488f5bbf498d92e4d1194f6f941307ffd95f75e76869f0e";

        pchMessageStart[0] = 0xcf;
        pchMessageStart[1] = 0xfc;
        pchMessageStart[2] = 0xbe;
        pchMessageStart[3] = 0xea;
        nDefaultPort = 8878;
        nPruneAfterHeight = 1000;
        /**
          * btzc: testnet params
          * nTime: 1414776313
          * nNonce: 1620571
          */
        std::vector<unsigned char> extraNonce(4);
        extraNonce[0] = 0x08;
        extraNonce[1] = 0x00;
        extraNonce[2] = 0x00;
        extraNonce[3] = 0x00;
        genesis = CreateGenesisBlock(1511365800, 170000001, 0x1e0ffff0, 2, 0 * COIN, extraNonce);
        consensus.hashGenesisBlock = genesis.GetHash();
//    std::cout << "hppcoin testnet genesisBlock hash: " << consensus.hashGenesisBlock.ToString() << std::endl;
//   std::cout << "hppcoin testnet hashMerkleRoot hash: " << genesis.hashMerkleRoot.ToString() << std::endl;
//btzc: update testnet hppcoin hashGenesisBlock and hashMerkleRoot
assert(consensus.hashGenesisBlock ==
uint256S("0xfb5653b3eb920612cf48c9c0262645679a70177ed2feb67078fef74d753e6843"));
assert(genesis.hashMerkleRoot ==
uint256S("0x4430cfe9a87008b82c46ea9d10fb2f7c13be20f7b3c3597e425c613006d8f269"));
        vFixedSeeds.clear();
        vSeeds.clear();
// nodes with support for servicebits filtering should be at the top
// hppcoin test seeds
        vSeeds.push_back(CDNSSeedData("73.229.27.208", "73.229.27.208", false));
        vSeeds.push_back(CDNSSeedData("37.59.46.162", "37.59.46.162", false));
        vSeeds.push_back(CDNSSeedData("37.59.10.238", "37.59.10.238", false));
//        vSeeds.push_back(CDNSSeedData("testnetbitcoin.jonasschnelli.ch", "testnet-seed.bitcoin.jonasschnelli.ch", true));
//        vSeeds.push_back(CDNSSeedData("petertodd.org", "seed.tbtc.petertodd.org", true));
//        vSeeds.push_back(CDNSSeedData("bluematt.me", "testnet-seed.bluematt.me"));
//        vSeeds.push_back(CDNSSeedData("bitcoin.schildbach.de", "testnet-seed.bitcoin.schildbach.de"));
        base58Prefixes[PUBKEY_ADDRESS] = std::vector < unsigned char > (1, 40);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector < unsigned char > (1, 178);
        base58Prefixes[SECRET_KEY] = std::vector < unsigned char > (1, 185);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container < std::vector < unsigned char > > ();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container < std::vector < unsigned char > > ();
        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));
fMiningRequiresPeers = true;
fDefaultConsistencyChecks = false;
fRequireStandard = false;
fMineBlocksOnDemand = false;
fTestnetToBeDeprecatedFieldRPC = true;
        checkpointData = (CCheckpointData) {
                boost::assign::map_list_of
                        (0, uint256S("0xfb5653b3eb920612cf48c9c0262645679a70177ed2feb67078fef74d753e6843")),
1511365800,
0,
100.0
        };
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.BIP34Height = -1; // BIP34 has not necessarily activated on regtest
        consensus.BIP34Hash = uint256();
        consensus.powLimit = uint256S("7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        consensus.nPowTargetTimespan = 120; // 2 minutes between retargets
        consensus.nPowTargetSpacing = 30; // 30 sec blocks
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 108; // 75% for testchains
        consensus.nMinerConfirmationWindow = 144; // Faster than normal for regtest (144 instead of 2016)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 999999999999ULL;

       // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");
        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 18444;
        nPruneAfterHeight = 1000;
/**
          * btzc: testnet params
          * nTime: 1414776313
          * nNonce: 1620571
          */
        std::vector<unsigned char> extraNonce(4);
        extraNonce[0] = 0x08;
        extraNonce[1] = 0x00;
        extraNonce[2] = 0x00;
        extraNonce[3] = 0x00;
        genesis = CreateGenesisBlock(1510044919, 230003491, 0x1e0ffff0, 2, 0 * COIN, extraNonce);
        consensus.hashGenesisBlock = genesis.GetHash();
//btzc: update regtest hppcoin hashGenesisBlock and hashMerkleRoot
//   std::cout << "hppcoin regtest genesisBlock hash: " << consensus.hashGenesisBlock.ToString() << std::endl;
//  std::cout << "hppcoin regtest hashMerkleRoot hash: " << genesis.hashMerkleRoot.ToString() << std::endl;
// btzc: update testnet hppcoin hashGenesisBlock and hashMerkleRoot
assert(consensus.hashGenesisBlock ==
uint256S("0x422e79465e5efa0d2b31bfb671589eabcbdda1328fbe238ec8444a708cc3f715"));
assert(genesis.hashMerkleRoot ==
uint256S("0x4430cfe9a87008b82c46ea9d10fb2f7c13be20f7b3c3597e425c613006d8f269"));
        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.
fMiningRequiresPeers = false;
fDefaultConsistencyChecks = true;
fRequireStandard = false;
fMineBlocksOnDemand = true;
fTestnetToBeDeprecatedFieldRPC = false;
        checkpointData = (CCheckpointData) {
                boost::assign::map_list_of
                        (0, uint256S("0x2c52ad6520cd9d2ca7c088e37aee024dee410a4acaa712527d872307043723be")),
0,
0,
0
        };
        base58Prefixes[PUBKEY_ADDRESS] = std::vector < unsigned char > (1, 40);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector < unsigned char > (1, 178);
        base58Prefixes[SECRET_KEY] = std::vector < unsigned char > (1, 239);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container < std::vector < unsigned char > > ();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container < std::vector < unsigned char > > ();
    }

    void UpdateBIP9Parameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout) {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
    }
};

static CRegTestParams regTestParams;

static CChainParams *pCurrentParams = 0;

const CChainParams &Params() {
    assert(pCurrentParams);
    return *pCurrentParams;
}

CChainParams &Params(const std::string &chain) {
    if (chain == CBaseChainParams::MAIN)
        return mainParams;
    else if (chain == CBaseChainParams::TESTNET)
        return testNetParams;
    else if (chain == CBaseChainParams::REGTEST)
        return regTestParams;
    else
        throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string &network) {
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

void UpdateRegtestBIP9Parameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout) {
    regTestParams.UpdateBIP9Parameters(d, nStartTime, nTimeout);
}
 
