 unsigned long int scrypt_scratpad_size_current_block =
                                ((1 << (GetNfactor(pblock->nTime) + 1)) * 128) + 63;
                        char *scratchpad = (char *) malloc(scrypt_scratpad_size_current_block * sizeof(char));
             scrypt_N_1_1_256_sp_generic(BEGIN(block->nVersion), BEGIN(thash), scratchpad,
                                                    GetNfactor(block->nTime));


Peershares Genesis Block Found:
genesis hash=00000bf4b644742a5af78bbf04c82e78ca3a799aa8baa195651b9e7374b907cc
merkle root=1aae79b2fbd29a35d7357757e217d3f93a34c1c6d7e3ecf75500345f336250f0
CBlock(hash=00000bf4b644742a5af7, ver=1, hashPrevBlock=00000000000000000000, hashMerkleRoot=1aae79b2fb, nTime=1510070473, nBits=1e0fffff, nNonce=479623, vtx=1, vchBlockSig=)
  Coinbase(hash=1aae79b2fb, nTime=1510070473, ver=1, vin.size=1, vout.size=1, nLockTime=0)
    CTxIn(COutPoint(0000000000, -1), coinbase 04ffff001d020f274c5754686520477561726469616e20323031372f31312f303620576861742068617070656e73207768656e207468652073757065722d7269636820777269746520746865207461782072756c65733f2054686579206661696c)
    CTxOut(empty)
  vMerkleTree: 1aae79b2fb
End Peershares Genesis Block
00000bf4b644742a5af78bbf04c82e78ca3a799aa8baa195651b9e7374b907cc
000000d78e35e381ca738ceb855b9faf528f0970d994ce4eb4560b56cbe2f6c4
1aae79b2fbd29a35d7357757e217d3f93a34c1c6d7e3ecf75500345f336250f0




 genesis = CreateGenesisBlock(1510044919, 0, 0x1e0ffff0, 2, 0 * COIN, extraNonce);

        uint32_t myNonce=1;
        
        int nHeight = 0;
 
        printf("Start generating genesis\n");
        do
        {    genesis = CreateGenesisBlock(1510044919, myNonce, 0x1e0ffff0, 2, 0 * COIN, extraNonce);
            myNonce++;
              printf("Nonce =  for generating genesis %u\n",myNonce);
        }
     while (
        //    UintToArith256(genesis.GetHash()) > bnTarget
            !CheckProofOfWorkGen(genesis.GetPoWHash(nHeight), genesis.nBits) 
            );
        printf("HPCCOIIN Genesis Block Found:\n");
        printf("genesis hash=%s\n", genesis.GetHash().ToString().c_str());
        printf("merkle root=%s\n", genesis.hashMerkleRoot.ToString().c_str());
        printf("nonce = %u",myNonce);
     
        printf("End Peershares Genesis Block\n");

        //// debug print
        printf("%s\n", genesis.GetHash().ToString().c_str());
        printf("%s\n", genesis.hashMerkleRoot.ToString().c_str());
