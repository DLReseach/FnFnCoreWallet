// Copyright (c) 2017-2018 The Multiverse developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "worldline.h"
#include "mvdelegatecomm.h"

using namespace std;
using namespace walleve;
using namespace multiverse;

//////////////////////////////
// CWorldLine 

CWorldLine::CWorldLine()
{
    pCoreProtocol = NULL;
}

CWorldLine::~CWorldLine()
{
}

bool CWorldLine::WalleveHandleInitialize()
{
    if (!WalleveGetObject("coreprotocol",pCoreProtocol))
    {
        WalleveLog("Failed to request coreprotocol\n");
        return false;
    }

    return true;
}

void CWorldLine::WalleveHandleDeinitialize()
{
    pCoreProtocol = NULL;
}

bool CWorldLine::WalleveHandleInvoke()
{
    storage::CMvDBConfig dbConfig(StorageConfig()->strDBHost,StorageConfig()->nDBPort,
                                  StorageConfig()->strDBName,StorageConfig()->strDBUser,StorageConfig()->strDBPass);

    if (!cntrBlock.Initialize(dbConfig,StorageConfig()->nDBConn,WalleveConfig()->pathData,WalleveConfig()->fDebug))
    {
        WalleveLog("Failed to initalize container\n");
        return false;
    }

    if (!CheckContainer())
    {
        cntrBlock.Clear();
        WalleveLog("Block container is invalid,try rebuild from block storage\n");
        // Rebuild ... 
        if (!RebuildContainer())
        {
            cntrBlock.Clear(); 
            WalleveLog("Failed to rebuild Block container,reconstruct all\n");
        } 
    }

    if (cntrBlock.IsEmpty())
    {
        CBlock block;
        pCoreProtocol->GetGenesisBlock(block);
        if (!InsertGenesisBlock(block))
        {
            WalleveLog("Failed to create genesis block\n");
            return false;
        }
    }    
        
    return true;
}

void CWorldLine::WalleveHandleHalt()
{
    cntrBlock.Deinitialize();
}

void CWorldLine::GetForkStatus(map<uint256,CForkStatus>& mapForkStatus)
{
    mapForkStatus.clear();

    multimap<int,CBlockIndex*> mapForkIndex;
    cntrBlock.ListForkIndex(mapForkIndex);
    for (multimap<int,CBlockIndex*>::iterator it = mapForkIndex.begin();it != mapForkIndex.end();++it)
    {
        CBlockIndex* pIndex = (*it).second;
        int nForkHeight = (*it).first;
        uint256 hashFork = pIndex->GetOriginHash();
        uint256 hashParent = pIndex->GetParentHash();

        if (hashParent != 0)
        {
            mapForkStatus[hashParent].mapSubline.insert(make_pair(nForkHeight,hashFork));
        } 

        map<uint256,CForkStatus>::iterator mi = mapForkStatus.insert(make_pair(hashFork,CForkStatus(hashFork,hashParent,nForkHeight))).first;
        CForkStatus& status = (*mi).second;
        status.hashLastBlock = pIndex->GetBlockHash();
        status.nLastBlockTime = pIndex->GetBlockTime();
        status.nLastBlockHeight = pIndex->GetBlockHeight();
        status.nMoneySupply = pIndex->GetMoneySupply();
    }
}

bool CWorldLine::GetForkProfile(const uint256& hashFork,CProfile& profile)
{
    return cntrBlock.RetrieveProfile(hashFork,profile);
}

int  CWorldLine::GetBlockCount(const uint256& hashFork)
{   
    int nCount = 0;
    CBlockIndex* pIndex = NULL;
    if (cntrBlock.RetrieveFork(hashFork,&pIndex))
    {
        while (pIndex != NULL)
        {
            pIndex = pIndex->pPrev;
            ++nCount;                               
        }
    }
    return nCount;
}   

bool CWorldLine::GetBlockLocation(const uint256& hashBlock,uint256& hashFork,int& nHeight)
{
    CBlockIndex* pIndex = NULL;
    if (!cntrBlock.RetrieveIndex(hashBlock,&pIndex))
    {
        return false;
    }
    hashFork = pIndex->GetOriginHash();
    nHeight = pIndex->GetBlockHeight();
    return true;
}

bool CWorldLine::GetBlockHash(const uint256& hashFork,int nHeight,uint256& hashBlock)
{
    CBlockIndex* pIndex = NULL;
    if (!cntrBlock.RetrieveFork(hashFork,&pIndex) || pIndex->GetBlockHeight() < nHeight)
    {
        return false;
    }
    while (pIndex != NULL && pIndex->GetBlockHeight() > nHeight)
    {
        pIndex = pIndex->pPrev;
    }
    while (pIndex != NULL && pIndex->GetBlockHeight() == nHeight && pIndex->IsExtended())
    {
        pIndex = pIndex->pPrev;
    }
    hashBlock = !pIndex ? 0 : pIndex->GetBlockHash();
    return (pIndex != NULL);
}

bool CWorldLine::GetBlockHash(const uint256& hashFork,int nHeight,vector<uint256>& vBlockHash)
{   
    CBlockIndex* pIndex = NULL;
    if (!cntrBlock.RetrieveFork(hashFork,&pIndex) || pIndex->GetBlockHeight() < nHeight)
    {       
        return false;                               
    }   
    while (pIndex != NULL && pIndex->GetBlockHeight() > nHeight)
    {
        pIndex = pIndex->pPrev;
    }
    while (pIndex != NULL && pIndex->GetBlockHeight() == nHeight)
    {
        vBlockHash.push_back(pIndex->GetBlockHash());
        pIndex = pIndex->pPrev;
    }
    std::reverse(vBlockHash.begin(),vBlockHash.end());
    return (!vBlockHash.empty());
}

bool CWorldLine::GetLastBlock(const uint256& hashFork,uint256& hashBlock,int& nHeight,int64& nTime)
{
    CBlockIndex* pIndex = NULL;
    if (!cntrBlock.RetrieveFork(hashFork,&pIndex))
    {
        return false;
    }
    hashBlock = pIndex->GetBlockHash();
    nHeight = pIndex->GetBlockHeight();
    nTime = pIndex->GetBlockTime();
    return true;
}

bool CWorldLine::GetLastBlockTime(const uint256& hashFork,int nDepth,vector<int64>& vTime)
{
    CBlockIndex* pIndex = NULL;
    if (!cntrBlock.RetrieveFork(hashFork,&pIndex))
    {
        return false;
    }

    vTime.clear();
    while ((nDepth--) > 0 && pIndex != NULL)
    {
        vTime.push_back(pIndex->GetBlockTime());
        pIndex = pIndex->pPrev;
    }
    return true;
}

bool CWorldLine::GetBlock(const uint256& hashBlock,CBlock& block)
{
    return cntrBlock.Retrieve(hashBlock,block);
}

bool CWorldLine::Exists(const uint256& hashBlock)
{
    return cntrBlock.Exists(hashBlock);
}

bool CWorldLine::GetTransaction(const uint256& txid,CTransaction& tx)
{
    return cntrBlock.RetrieveTx(txid,tx);
}

bool CWorldLine::ExistsTx(const uint256& txid)
{
    return cntrBlock.ExistsTx(txid);
}

bool CWorldLine::GetTxLocation(const uint256& txid,uint256& hashFork,int& nHeight)
{
    return cntrBlock.RetrieveTxLocation(txid,hashFork,nHeight);
}

bool CWorldLine::GetTxUnspent(const uint256& hashFork,const vector<CTxIn>& vInput,vector<CTxOutput>& vOutput)
{
    vOutput.resize(vInput.size());
    storage::CBlockView view;
    if (!cntrBlock.GetForkBlockView(hashFork,view))
    {
        return false;
    }
    
    for (std::size_t i = 0;i < vInput.size();i++)
    {
        view.RetrieveUnspent(vInput[i].prevout,vOutput[i]);
    }
    return true;
}

bool CWorldLine::FilterTx(CTxFilter& filter)
{
    return cntrBlock.FilterTx(filter);
}

MvErr CWorldLine::AddNewBlock(const CBlock& block,CWorldLineUpdate& update)
{
    uint256 hash = block.GetHash();
    MvErr err = MV_OK;

    if (cntrBlock.Exists(hash))
    {
        WalleveLog("AddNewBlock Already Exists : %s \n",hash.ToString().c_str());
        return MV_ERR_ALREADY_HAVE;
    }
    
    err = pCoreProtocol->ValidateBlock(block);
    if (err != MV_OK)
    {
        WalleveLog("AddNewBlock Validate Block Error(%s) : %s \n",MvErrString(err),hash.ToString().c_str());
        return err;
    }
   
    CBlockIndex* pIndexPrev;
    if (!cntrBlock.RetrieveIndex(block.hashPrev,&pIndexPrev))
    {
        WalleveLog("AddNewBlock Retrieve Prev Index Error: %s \n",block.hashPrev.ToString().c_str());
        return MV_ERR_SYS_STORAGE_ERROR;
    }

    storage::CBlockView view;
    if (!cntrBlock.GetBlockView(block.hashPrev,view,!block.IsOrigin()))
    {
        WalleveLog("AddNewBlock Get Block View Error: %s \n",block.hashPrev.ToString().c_str());
        return MV_ERR_SYS_STORAGE_ERROR;
    }
    
    err = pCoreProtocol->VerifyBlock(block,pIndexPrev);
    if (err != MV_OK)
    {
        WalleveLog("AddNewBlock Verify Block Error(%s) : %s \n",MvErrString(err),hash.ToString().c_str());
        return err;
    } 

    if (!block.IsVacant())
    {
        view.AddTx(block.txMint.GetHash(),block.txMint);
    }

    CBlockEx blockex(block);
    vector<CTxContxt>& vTxContxt = blockex.vTxContxt;

    vTxContxt.reserve(block.vtx.size());

    BOOST_FOREACH(const CTransaction& tx,block.vtx)
    {
        uint256 txid = tx.GetHash();
        CTxContxt txContxt;
        err = GetTxContxt(view,tx,txContxt);
        if (err != MV_OK)
        {
            WalleveLog("AddNewBlock Get txContxt Error(%s) : %s \n",MvErrString(err),txid.ToString().c_str());
            return err;
        }
        err = pCoreProtocol->VerifyBlockTx(tx,txContxt,pIndexPrev);
        if (err != MV_OK)
        {
            WalleveLog("AddNewBlock Verify BlockTx Error(%s) : %s \n",MvErrString(err),txid.ToString().c_str());
            return err;
        }
        vTxContxt.push_back(txContxt);
        view.AddTx(txid,tx,txContxt.destIn,txContxt.GetValueIn());
    }

    CBlockIndex* pIndexNew;
    if (!cntrBlock.AddNew(hash,blockex,&pIndexNew))
    {
        WalleveLog("AddNewBlock Storage AddNew Error : %s \n",hash.ToString().c_str());
        return MV_ERR_SYS_STORAGE_ERROR;
    }

    WalleveLog("AddNew Block : %s \n",hash.ToString().c_str());
    WalleveLog("    %s\n",pIndexNew->ToString().c_str());

    CBlockIndex* pIndexFork = NULL;
    if (cntrBlock.RetrieveFork(pIndexNew->GetOriginHash(),&pIndexFork)
        && (pIndexFork->nChainTrust > pIndexNew->nChainTrust
            || (pIndexFork->nChainTrust == pIndexNew->nChainTrust && !pIndexNew->IsEquivalent(pIndexFork))))
    {
        return MV_OK;
    }

    if (!cntrBlock.CommitBlockView(view,pIndexNew))
    {
        WalleveLog("AddNewBlock Storage Commit BlockView Error : %s \n",hash.ToString().c_str());
        return MV_ERR_SYS_STORAGE_ERROR;
    }
   
    update = CWorldLineUpdate(pIndexNew);
    view.GetTxUpdated(update.setTxUpdate);
    if (!GetBlockChanges(pIndexNew,pIndexFork,update.vBlockAddNew,update.vBlockRemove))
    {
        WalleveLog("AddNewBlock Storage GetBlockChanges Error : %s \n",hash.ToString().c_str());
        return MV_ERR_SYS_STORAGE_ERROR;
    } 
    return MV_OK;
}

MvErr CWorldLine::AddNewOrigin(const CBlock& block,CWorldLineUpdate& update)
{
    uint256 hash = block.GetHash();
    MvErr err = MV_OK;

    if (cntrBlock.Exists(hash))
    {
        WalleveLog("AddNewOrigin Already Exists : %s \n",hash.ToString().c_str());
        return MV_ERR_ALREADY_HAVE;
    }
    
    err = pCoreProtocol->ValidateBlock(block);
    if (err != MV_OK)
    {
        WalleveLog("AddNewOrigin Validate Block Error(%s) : %s \n",MvErrString(err),hash.ToString().c_str());
        return err;
    }
   
    CBlockIndex* pIndexPrev;
    if (!cntrBlock.RetrieveIndex(block.hashPrev,&pIndexPrev))
    {
        WalleveLog("AddNewOrigin Retrieve Prev Index Error: %s \n",block.hashPrev.ToString().c_str());
        return MV_ERR_SYS_STORAGE_ERROR;
    }

    CProfile parent;
    if (!cntrBlock.RetrieveProfile(pIndexPrev->GetOriginHash(),parent))
    {
        WalleveLog("AddNewOrigin Retrieve parent profile Error: %s \n",block.hashPrev.ToString().c_str());
        return MV_ERR_SYS_STORAGE_ERROR;
    }
    CProfile profile;
    err = pCoreProtocol->ValidateOrigin(block,parent,profile);
    if (err != MV_OK)
    {
        WalleveLog("AddNewOrigin Validate Origin Error(%s): %s \n",MvErrString(err),hash.ToString().c_str());
        return err;
    }
    
    storage::CBlockView view;

    if (profile.IsIsolated())
    {
        if (!cntrBlock.GetBlockView(view))
        {
            WalleveLog("AddNewOrigin Get Block View Error: %s \n",block.hashPrev.ToString().c_str());
            return MV_ERR_SYS_STORAGE_ERROR;
        }
    }
    else
    {
        if (!cntrBlock.GetBlockView(block.hashPrev,view,false))
        {
            WalleveLog("AddNewOrigin Get Block View Error: %s \n",block.hashPrev.ToString().c_str());
            return MV_ERR_SYS_STORAGE_ERROR;
        }
    }
    
    if (block.txMint.nAmount != 0)
    {
        view.AddTx(block.txMint.GetHash(),block.txMint);
    }

    CBlockIndex* pIndexNew;
    CBlockEx blockex(block);

    if (!cntrBlock.AddNew(hash,blockex,&pIndexNew))
    {
        WalleveLog("AddNewOrigin Storage AddNew Error : %s \n",hash.ToString().c_str());
        return MV_ERR_SYS_STORAGE_ERROR;
    }

    WalleveLog("AddNew Origin Block : %s \n",hash.ToString().c_str());
    WalleveLog("    %s\n",pIndexNew->ToString().c_str());

    if (!cntrBlock.CommitBlockView(view,pIndexNew))
    {
        WalleveLog("AddNewOrigin Storage Commit BlockView Error : %s \n",hash.ToString().c_str());
        return MV_ERR_SYS_STORAGE_ERROR;
    }
   
    update = CWorldLineUpdate(pIndexNew);
    view.GetTxUpdated(update.setTxUpdate);
    update.vBlockAddNew.push_back(blockex);

    return MV_OK;
}

bool CWorldLine::GetProofOfWorkTarget(const uint256& hashPrev,int nAlgo,int& nBits,int64& nReward)
{
    CBlockIndex* pIndexPrev;
    if (!cntrBlock.RetrieveIndex(hashPrev,&pIndexPrev))
    {
        WalleveLog("GetProofOfWorkTarget : Retrieve Prev Index Error: %s \n",hashPrev.ToString().c_str());
        return false;
    }
    if (!pIndexPrev->IsPrimary())
    {
        WalleveLog("GetProofOfWorkTarget : Previous is not primary: %s \n",hashPrev.ToString().c_str());
        return false;
    }
    if (!pCoreProtocol->GetProofOfWorkTarget(pIndexPrev,nAlgo,nBits,nReward))
    {
        WalleveLog("GetProofOfWorkTarget : Unknown proof-of-work algo: %s \n",hashPrev.ToString().c_str());
        return false;
    }
    return true;
}

bool CWorldLine::GetDelegatedProofOfStakeReward(const uint256& hashPrev,size_t nWeight,int64& nReward)
{
    CBlockIndex* pIndexPrev;
    if (!cntrBlock.RetrieveIndex(hashPrev,&pIndexPrev))
    {
        WalleveLog("GetDelegatedProofOfStakeReward : Retrieve Prev Index Error: %s \n",hashPrev.ToString().c_str());
        return false;
    }
/*
    if (!pIndexPrev->IsPrimary())
    {
        WalleveLog("GetDelegatedProofOfStakeReward : Previous is not primary: %s \n",hashPrev.ToString().c_str());
        return false;
    }
*/    
    nReward = pCoreProtocol->GetDelegatedProofOfStakeReward(pIndexPrev,nWeight);
    return true;
}

bool CWorldLine::GetBlockLocator(const uint256& hashFork,CBlockLocator& locator)
{
    return cntrBlock.GetForkBlockLocator(hashFork,locator);
}

bool CWorldLine::GetBlockInv(const uint256& hashFork,const CBlockLocator& locator,vector<uint256>& vBlockHash,size_t nMaxCount)
{
    return cntrBlock.GetForkBlockInv(hashFork,locator,vBlockHash,nMaxCount);
}

bool CWorldLine::GetBlockDelegateEnrolled(const uint256& hashBlock,map<CDestination,size_t>& mapWeight,
                                                                   map<CDestination,vector<unsigned char> >& mapEnrollData)
{
    mapWeight.clear();
    mapEnrollData.clear();

    CBlockIndex* pIndex;
    if (!cntrBlock.RetrieveIndex(hashBlock,&pIndex))
    {
        WalleveLog("GetBlockDelegateEnrolled : Retrieve block Index Error: %s \n",hashBlock.ToString().c_str());
        return false;
    }
    int64 nDelegateWeightRatio = (pIndex->GetMoneySupply() + DELEGATE_THRESH - 1) / DELEGATE_THRESH;

    if (pIndex->GetBlockHeight() < MV_CONSENSUS_ENROLL_INTERVAL)
    {
        return true;
    }
    for (int i = 0;i < MV_CONSENSUS_ENROLL_INTERVAL;i++)
    {
        pIndex = pIndex->pPrev;
    }
    
    map<CDestination,int64> mapDelegate;
    if (!cntrBlock.RetrieveDelegate(hashBlock,nDelegateWeightRatio,mapDelegate))
    {
        WalleveLog("GetBlockDelegateEnrolled : Retrieve Delegate Error: %s \n",hashBlock.ToString().c_str());
        return false;
    }

    map<CDestination,vector<unsigned char> > mapEnrollDataAll;
    if (!cntrBlock.RetrieveEnroll(pIndex->GetBlockHash(),hashBlock,mapEnrollDataAll))
    {
        WalleveLog("GetBlockDelegateEnrolled : Retrieve Enroll Error: %s \n",hashBlock.ToString().c_str());
        return false;
    }

    for (map<CDestination,int64>::iterator it = mapDelegate.begin();it != mapDelegate.end();++it)
    {
        const CDestination& dest = (*it).first;
        map<CDestination,vector<unsigned char> >::iterator mi = mapEnrollDataAll.find(dest);
        if (mi != mapEnrollDataAll.end())
        {
            mapWeight.insert(make_pair(dest,size_t((*it).second / nDelegateWeightRatio)));
            mapEnrollData.insert((*mi));
        }
    }

    return true;
}

bool CWorldLine::CheckContainer()
{
    if (cntrBlock.IsEmpty())
    {
        return true;
    }
    if (!cntrBlock.Exists(pCoreProtocol->GetGenesisBlockHash()))
    {
        return false;
    }
    return true;
}

bool CWorldLine::RebuildContainer()
{
    return false;
}

bool CWorldLine::InsertGenesisBlock(CBlock& block)
{
    CBlockIndex* pIndexNew = NULL;
    CBlockEx blockex(block);
    if (!cntrBlock.AddNew(block.GetHash(),blockex,&pIndexNew))
    {
        return false;
    }
    
    storage::CBlockView view;
    cntrBlock.GetBlockView(view);
    view.AddTx(block.txMint.GetHash(),block.txMint);
    if (!cntrBlock.CommitBlockView(view,pIndexNew))
    {
        return false;
    }
    return true;
}

MvErr CWorldLine::GetTxContxt(storage::CBlockView& view,const CTransaction& tx,CTxContxt& txContxt)
{
    txContxt.SetNull();
    BOOST_FOREACH(const CTxIn& txin,tx.vInput)
    {
        CTxOutput output;
        if (!view.RetrieveUnspent(txin.prevout,output))
        {
            return MV_ERR_MISSING_PREV;
        }
        if (txContxt.destIn.IsNull())
        {
            txContxt.destIn = output.destTo;
        }
        else if (txContxt.destIn != output.destTo)
        {
            return MV_ERR_TRANSACTION_INVALID;
        }
        txContxt.vInputValue.push_back(make_pair(output.nAmount,output.nLockUntil));
    }
    return MV_OK; 
}

bool CWorldLine::GetBlockChanges(const CBlockIndex* pIndexNew,const CBlockIndex* pIndexFork,
                                 vector<CBlockEx>& vBlockAddNew,vector<CBlockEx>& vBlockRemove)
{
    while (pIndexNew != pIndexFork)
    {
        int64 nLastBlockTime = pIndexFork ? pIndexFork->GetBlockTime() : -1;
        if (pIndexNew->GetBlockTime() >= nLastBlockTime)
        {
            CBlockEx block;
            if (!cntrBlock.Retrieve(pIndexNew,block))
            {
                return false;
            }
            vBlockAddNew.push_back(block);
            pIndexNew = pIndexNew->pPrev;
        }
        else
        {
            CBlockEx block;
            if (!cntrBlock.Retrieve(pIndexFork,block))
            {
                return false;
            }
            vBlockRemove.push_back(block);
            pIndexFork = pIndexFork->pPrev;
        }
    }
    return true;
}

