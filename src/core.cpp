// Copyright (c) 2017-2018 The Multiverse developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "core.h"

using namespace std;                      
using namespace walleve; 
using namespace multiverse;

#define DEBUG(err, ...)         Debug((err),__FUNCTION__,__VA_ARGS__)

static const int64  MAX_CLOCK_DRIFT   = 10 * 60;

static const int  PROOF_OF_WORK_BITS_LIMIT   = 16;
static const int  PROOF_OF_WORK_BITS_INIT    = 20;
static const int  PROOF_OF_WORK_ADJUST_COUNT = 16; 
static const int  PROOF_OF_WORK_ADJUST_DEBOUNCE = 10; 
static const int  PROOF_OF_WORK_TARGET_SPACING = BLOCK_TARGET_SPACING + BLOCK_TARGET_SPACING / 2; 
///////////////////////////////
// CMvCoreProtocol

CMvCoreProtocol::CMvCoreProtocol()
{
    nProofOfWorkLimit = PROOF_OF_WORK_BITS_LIMIT;
    nProofOfWorkInit = PROOF_OF_WORK_BITS_INIT;
    nProofOfWorkUpperTarget = PROOF_OF_WORK_TARGET_SPACING + PROOF_OF_WORK_ADJUST_DEBOUNCE;
    nProofOfWorkLowerTarget = PROOF_OF_WORK_TARGET_SPACING - PROOF_OF_WORK_ADJUST_DEBOUNCE;
}

CMvCoreProtocol::~CMvCoreProtocol()
{
}

bool CMvCoreProtocol::WalleveHandleInitialize()
{
    CBlock block;
    GetGenesisBlock(block);
    hashGenesisBlock = block.GetHash();
    return true;
}

const MvErr CMvCoreProtocol::Debug(const MvErr& err,const char* pszFunc,const char *pszFormat,...)
{
    string strFormat(pszFunc);
    strFormat += string(", ") + string(MvErrString(err)) + string(" : ") + string(pszFormat);
    va_list ap;
    va_start(ap,pszFormat);
    WalleveVDebug(strFormat.c_str(),ap);
    va_end(ap);
    return err;
}

const uint256& CMvCoreProtocol::GetGenesisBlockHash()
{
    return hashGenesisBlock;
}

/*
PubKey : 575f2041770496489120bb102d9dd55f5e75b0c4aa528d5762b92b59acd6d939
Secret : bf0ebca9be985104e88b6a24e54cedbcdd1696a8984c8fdd7bc96917efb5a1ed

PubKey : 749b1fe6ad43c24bd8bff20a222ef74fdf0763a7efa0761619b99ec73985016c
Secret : 05f07d1e09b60b74538a1e021c98956a7c508fa35f6c3eba01c9ab1f6a871611

PubKey : 6236d780f9f743707d57b3feb19d21c8a867577f5e83c163774222bb7ef8d8cb
Secret : 7c6a6aba05cec77a998c19649ee1fa0e29c7b5246d0e3a6501ee1d4d81dd73ea
*/

void CMvCoreProtocol::GetGenesisBlock(CBlock& block)
{
    block.SetNull();

    block.nVersion   = 1;
    block.nType      = CBlock::BLOCK_GENESIS;
    block.nTimeStamp = 1515745156;
    block.hashPrev   = 0;
    
    CTransaction& tx = block.txMint;
    tx.nType   = CTransaction::TX_GENESIS;
    tx.sendTo  = CDestination(multiverse::crypto::CPubKey(uint256("575f2041770496489120bb102d9dd55f5e75b0c4aa528d5762b92b59acd6d939")));
    tx.nAmount = 745000000 * COIN; // 745000000 is initial number of token

    const char* pszGenesis = "The third machine age : Revolution";
    block.vchProof = vector<uint8>((const uint8*)pszGenesis, (const uint8*)pszGenesis + strlen(pszGenesis));
    
}

MvErr CMvCoreProtocol::ValidateTransaction(const CTransaction& tx)
{
    // Basic checks that don't depend on any context
    if (tx.vInput.empty() && tx.nType != CTransaction::TX_GENESIS && tx.nType != CTransaction::TX_WORK && tx.nType != CTransaction::TX_STAKE)
    {
        return DEBUG(MV_ERR_TRANSACTION_INVALID,"tx vin is empty\n");
    }
    if (!tx.vInput.empty() && (tx.nType == CTransaction::TX_GENESIS || tx.nType == CTransaction::TX_WORK || tx.nType == CTransaction::TX_STAKE))
    {
        return DEBUG(MV_ERR_TRANSACTION_INVALID,"tx vin is not empty for genesis or work tx\n");
    }
    if (!tx.vchSig.empty() && tx.IsMintTx())
    {
        return DEBUG(MV_ERR_TRANSACTION_INVALID,"invalid signature\n");
    }
    if (tx.sendTo.IsNull())
    {
        return DEBUG(MV_ERR_TRANSACTION_OUTPUT_INVALID,"send to null address\n");
    }
    if (!MoneyRange(tx.nAmount))
    {
        return DEBUG(MV_ERR_TRANSACTION_OUTPUT_INVALID,"amount overflow %ld\n",tx.nAmount);
    }
    
    if (!MoneyRange(tx.nTxFee)
        || (tx.nType != CTransaction::TX_TOKEN && tx.nTxFee != 0)
        || (tx.nType == CTransaction::TX_TOKEN && tx.nTxFee < MIN_TX_FEE))
    {
        return DEBUG(MV_ERR_TRANSACTION_OUTPUT_INVALID,"txfee invalid %ld",tx.nTxFee);
    }

    set<CTxOutPoint> setInOutPoints;
    BOOST_FOREACH(const CTxIn& txin, tx.vInput)
    {
        if (txin.prevout.IsNull() || txin.prevout.n > 1)
        {
            return DEBUG(MV_ERR_TRANSACTION_INPUT_INVALID,"prevout invalid\n");
        }
        if (!setInOutPoints.insert(txin.prevout).second)
        {
            return DEBUG(MV_ERR_TRANSACTION_INPUT_INVALID,"duplicate inputs\n");
        }
    }

    if (GetSerializeSize(tx) > MAX_TX_SIZE)
    {
        return DEBUG(MV_ERR_TRANSACTION_OVERSIZE,"%u\n",GetSerializeSize(tx));
    }

    return MV_OK;
}

MvErr CMvCoreProtocol::ValidateBlock(const CBlock& block)
{
    // These are checks that are independent of context
    // Check timestamp
    if (block.GetBlockTime() > WalleveGetNetTime() + MAX_CLOCK_DRIFT)
    {
        return DEBUG(MV_ERR_BLOCK_TIMESTAMP_OUT_OF_RANGE,"%ld\n",block.GetBlockTime());
    }

    // Extended block should be not empty
    if (block.nType == CBlock::BLOCK_EXTENDED && block.vtx.empty())
    {
        return DEBUG(MV_ERR_BLOCK_TRANSACTIONS_INVALID,"empty extended block\n");
    }
  
    // validate vacant block 
    if (block.nType == CBlock::BLOCK_VACANT)
    {
        return ValidateVacantBlock(block);
    }

    // Validate mint tx
    if (!block.txMint.IsMintTx() || ValidateTransaction(block.txMint) != MV_OK)
    {
        return DEBUG(MV_ERR_BLOCK_TRANSACTIONS_INVALID,"invalid mint tx\n");
    }

    size_t nBlockSize = GetSerializeSize(block);
    if (nBlockSize > MAX_BLOCK_SIZE)
    {
        return DEBUG(MV_ERR_BLOCK_OVERSIZE,"size overflow size=%u vtx=%u\n",nBlockSize,block.vtx.size());
    }

    if (block.nType == CBlock::BLOCK_ORIGIN && !block.vtx.empty())
    {
        return DEBUG(MV_ERR_BLOCK_TRANSACTIONS_INVALID,"origin block vtx is not empty\n");
    }

    vector<uint256> vMerkleTree;
    if (block.hashMerkle != block.BuildMerkleTree(vMerkleTree))
    {
        return DEBUG(MV_ERR_BLOCK_TXHASH_MISMATCH,"tx merkeroot mismatched\n");
    }

    set<uint256> setTx;
    setTx.insert(vMerkleTree.begin(),vMerkleTree.begin() + block.vtx.size());
    if (setTx.size() != block.vtx.size())
    {
        return DEBUG(MV_ERR_BLOCK_DUPLICATED_TRANSACTION,"duplicate tx\n");
    }

    BOOST_FOREACH(const CTransaction& tx,block.vtx)
    {
        if (tx.IsMintTx() || ValidateTransaction(tx) != MV_OK)
        {
            return DEBUG(MV_ERR_BLOCK_TRANSACTIONS_INVALID,"invalid tx %s\n",tx.GetHash().GetHex().c_str());
        }
    }

    if (!CheckBlockSignature(block))
    {
        return DEBUG(MV_ERR_BLOCK_SIGNATURE_INVALID,"\n");
    }
    return MV_OK;
}

MvErr CMvCoreProtocol::ValidateOrigin(const CBlock& block,const CProfile& parentProfile,CProfile& forkProfile)
{
    if (!forkProfile.Load(block.vchProof))
    {
        return DEBUG(MV_ERR_BLOCK_INVALID_FORK,"load profile error\n");
    }
    if (forkProfile.IsNull())
    {
        return DEBUG(MV_ERR_BLOCK_INVALID_FORK,"invalid profile");
    }
    if (parentProfile.IsPrivate())
    {
        if (!forkProfile.IsPrivate() || parentProfile.destOwner != forkProfile.destOwner)
        {
            return DEBUG(MV_ERR_BLOCK_INVALID_FORK,"permission denied");
        }
    }
    return MV_OK;
}

MvErr CMvCoreProtocol::VerifyBlock(const CBlock& block,CBlockIndex* pIndexPrev)
{
    (void)block;
    (void)pIndexPrev;
    return MV_OK;
}

MvErr CMvCoreProtocol::VerifyBlockTx(const CTransaction& tx,const CTxContxt& txContxt,CBlockIndex* pIndexPrev)
{
    (void)tx;
    (void)txContxt;
    (void)pIndexPrev;
    return MV_OK;
}

MvErr CMvCoreProtocol::VerifyTransaction(const CTransaction& tx,const vector<CTxOutput>& vPrevOutput,int nForkHeight)
{
    CDestination destIn = vPrevOutput[0].destTo;
    int64 nValueIn = 0;
    BOOST_FOREACH(const CTxOutput& output,vPrevOutput)
    {
        if (destIn != output.destTo)
        {
            return DEBUG(MV_ERR_TRANSACTION_INPUT_INVALID,"input destination mismatched\n");
        }
        if (output.nLockUntil != 0 && output.nLockUntil < nForkHeight)
        {
            return DEBUG(MV_ERR_TRANSACTION_INPUT_INVALID,"input is still locked\n");
        }
        nValueIn += output.nAmount;
    }
    if (!MoneyRange(nValueIn))
    {
        return DEBUG(MV_ERR_TRANSACTION_INPUT_INVALID,"valuein invalid %ld\n",nValueIn);
    }
    if (nValueIn < tx.nAmount + tx.nTxFee)
    {
        return DEBUG(MV_ERR_TRANSACTION_INPUT_INVALID,"valuein is not enough (%ld : %ld)\n",nValueIn,tx.nAmount + tx.nTxFee);
    }
    if (!destIn.VerifySignature(tx.GetSignatureHash(),tx.vchSig))
    {
        return DEBUG(MV_ERR_TRANSACTION_SIGNATURE_INVALID,"invalid signature\n");
    }
    return MV_OK;
}

bool CMvCoreProtocol::GetProofOfWorkTarget(CBlockIndex* pIndexPrev,int nAlgo,int& nBits,int64& nReward)
{
    if (nAlgo <= 0 || nAlgo >= CM_MAX || !pIndexPrev->IsPrimary())
    {
        return false;
    }
    nReward = GetProofOfWorkReward(pIndexPrev);

    CBlockIndex* pIndex = pIndexPrev;
    while ((!pIndex->IsProofOfWork() || pIndex->nProofAlgo != nAlgo) && pIndex->pPrev != NULL)
    {
        pIndex = pIndex->pPrev;
    }
    
    // first 
    if (!pIndex->IsProofOfWork())
    {
        nBits = nProofOfWorkInit;
        return true; 
    }

    nBits = pIndex->nProofBits;
    int64 nSpacing = 0;
    int64 nWeight = 0;
    int nWIndex = PROOF_OF_WORK_ADJUST_COUNT - 1; 
    while (pIndex->IsProofOfWork())
    {
        nSpacing += (pIndex->GetBlockTime() - pIndex->pPrev->GetBlockTime()) << nWIndex;
        nWeight += (1ULL) << nWIndex; 
        if (!nWIndex--)
        {
            break;
        }
        pIndex = pIndex->pPrev;
        while ((!pIndex->IsProofOfWork() || pIndex->nProofAlgo != nAlgo) && pIndex->pPrev != NULL)
        {
            pIndex = pIndex->pPrev;
        }
    }
    nSpacing /= nWeight;
    if (nSpacing > nProofOfWorkUpperTarget && nBits > nProofOfWorkLimit)
    {
        nBits--;
    }
    else if (nSpacing < nProofOfWorkLowerTarget)
    {
        nBits++;
    }
    return true;    
}

int CMvCoreProtocol::GetProofOfWorkRunTimeBits(int nBits,int64 nTime,int64 nPrevTime)
{
    if (nTime - nPrevTime < BLOCK_TARGET_SPACING)
    {
        return (nBits + 1);
    }
    
    nBits -= (nTime - nPrevTime - BLOCK_TARGET_SPACING) / PROOF_OF_WORK_DECAY_STEP;
    if (nBits < nProofOfWorkLimit)
    {
        nBits = nProofOfWorkLimit;
    }
    return nBits;
}

int64 CMvCoreProtocol::GetDelegatedProofOfStakeReward(CBlockIndex* pIndexPrev,size_t nWeight)
{
    return (15 * COIN);    
}

void CMvCoreProtocol::GetDelegatedBallot(const uint256& nAgreement,size_t nWeight,
                                         const map<CDestination,size_t>& mapBallot,vector<CDestination>& vBallot)
{
    vBallot.clear();
    int nSelected = 0;
    for (const unsigned char* p = nAgreement.begin();p != nAgreement.end();++p)
    {
        nSelected ^= *p;
    }
    size_t nWeightWork = ((DELEGATE_THRESH - nWeight) * (DELEGATE_THRESH - nWeight) * (DELEGATE_THRESH - nWeight))
                         / (DELEGATE_THRESH * DELEGATE_THRESH);
    if (nSelected >= nWeightWork * 256 / (nWeightWork + nWeight))
    {
        size_t nTrust = nWeight;
        for (const unsigned char* p = nAgreement.begin();p != nAgreement.end() && nTrust != 0;++p)
        {
            nSelected += *p;
            size_t n = nSelected % nWeight;
            for (map<CDestination,size_t>::const_iterator it = mapBallot.begin();it != mapBallot.end();++it)
            {
                if (n < (*it).second)
                {
                    vBallot.push_back((*it).first);
                    break;
                }
                n -= (*it).second;
            }
            nTrust >>= 1;
        }
    }
}

int64 CMvCoreProtocol::GetProofOfWorkReward(CBlockIndex* pIndexPrev)
{
    (void)pIndexPrev;
    return (15 * COIN);    
}

bool CMvCoreProtocol::CheckBlockSignature(const CBlock& block)
{
    (void)block;
    return true;
}

MvErr CMvCoreProtocol::ValidateVacantBlock(const CBlock& block)
{
    if (block.hashMerkle != 0 || !block.txMint.IsNull() || !block.vtx.empty())
    {
        return DEBUG(MV_ERR_BLOCK_TRANSACTIONS_INVALID,"vacant block tx is not empty.");
    }
    
    if (!block.vchProof.empty() || !block.vchSig.empty())
    {
        return DEBUG(MV_ERR_BLOCK_SIGNATURE_INVALID,"vacant block proof or signature is not empty.");
    }

    return MV_OK;
}

///////////////////////////////
// CMvTestNetCoreProtocol

CMvTestNetCoreProtocol::CMvTestNetCoreProtocol()
{
}

void CMvTestNetCoreProtocol::GetGenesisBlock(CBlock& block)
{
    (void)block;
}

