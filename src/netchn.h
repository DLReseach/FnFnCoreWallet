// Copyright (c) 2017-2018 The Multiverse developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef  MULTIVERSE_NETCHN_H
#define  MULTIVERSE_NETCHN_H

#include "mvpeernet.h"
#include "mvbase.h"
#include "schedule.h"

namespace multiverse
{

class CNetChannelPeer
{
    class CNetChannelPeerFork
    {
    public:
        CNetChannelPeerFork() : fSynchronized(true) {}
        enum { NETCHANNEL_KNOWNINV_EXPIREDTIME = 10 * 60,NETCHANNEL_KNOWNINV_MAXCOUNT = 1024 * 8 };
        void AddKnownTx(const std::vector<uint256>& vTxHash);
        bool IsKnownTx(const uint256& txid) const {  return (!!setKnownTx.get<0>().count(txid)); }
    protected:
        void ClearExpiredTx(std::size_t nReserved);
    public:
        bool fSynchronized;
        CPeerKnownTxSet setKnownTx;
    };
public:
    CNetChannelPeer() {}
    CNetChannelPeer(uint64 nServiceIn,const uint256& hashPrimary)
    : nService(nServiceIn)
    {
        mapSubscribedFork.insert(std::make_pair(hashPrimary,CNetChannelPeerFork()));
    }
    bool IsSynchronized(const uint256& hashFork) const;
    bool SetSyncStatus(const uint256& hashFork,bool fSync,bool& fInverted);
    void AddKnownTx(const uint256& hashFork,const std::vector<uint256>& vTxHash);
    bool IsSubscribed(const uint256& hashFork) const { return (!!mapSubscribedFork.count(hashFork)); }
    void MakeTxInv(const uint256& hashFork,const std::vector<uint256>& vTxPool,
                                           std::vector<network::CInv>& vInv,std::size_t nMaxCount);
public:
    uint64 nService;
    std::map<uint256,CNetChannelPeerFork> mapSubscribedFork;
};

class CNetChannel : public network::IMvNetChannel
{
public:
    CNetChannel();
    ~CNetChannel();
    int GetPrimaryChainHeight() override;
    void BroadcastBlockInv(const uint256& hashFork,const uint256& hashBlock,const std::set<uint64>& setKnownPeer=std::set<uint64>()) override;
    void BroadcastTxInv(const uint256& hashFork) override;
protected:
    enum {MAX_GETBLOCKS_COUNT = 128};
    enum {MAX_PEER_SCHED_COUNT = 8};

    bool WalleveHandleInitialize() override;
    void WalleveHandleDeinitialize() override;
    bool WalleveHandleInvoke() override;
    void WalleveHandleHalt() override;

    bool HandleEvent(network::CMvEventPeerActive& eventActive) override;
    bool HandleEvent(network::CMvEventPeerDeactive& eventDeactive) override;
    bool HandleEvent(network::CMvEventPeerInv& eventInv) override;
    bool HandleEvent(network::CMvEventPeerGetData& eventGetData) override;
    bool HandleEvent(network::CMvEventPeerGetBlocks& eventGetBlocks) override;
    bool HandleEvent(network::CMvEventPeerTx& eventTx) override;
    bool HandleEvent(network::CMvEventPeerBlock& eventBlock) override;

    CSchedule& GetSchedule(const uint256& hashFork);
    void NotifyPeerUpdate(uint64 nNonce,bool fActive,const network::CAddress& addrPeer);
    void DispatchGetBlocksEvent(uint64 nNonce,const uint256& hashFork);
    void DispatchAwardEvent(uint64 nNonce,walleve::CEndpointManager::Bonus bonus);
    void DispatchMisbehaveEvent(uint64 nNonce,walleve::CEndpointManager::CloseReason reason);
    void SchedulePeerInv(uint64 nNonce,const uint256& hashFork,CSchedule& sched);
    bool GetMissingPrevTx(CTransaction& tx,std::set<uint256>& setMissingPrevTx);
    void AddNewBlock(const uint256& hashFork,const uint256& hash,CSchedule& sched,
                     std::set<uint64>& setSchedPeer,std::set<uint64>& setMisbehavePeer);
    void AddNewTx(const uint256& hashFork,const uint256& txid,CSchedule& sched,
                     std::set<uint64>& setSchedPeer,std::set<uint64>& setMisbehavePeer);
    void PostAddNew(const uint256& hashFork,CSchedule& sched,
                    std::set<uint64>& setSchedPeer,std::set<uint64>& setMisbehavePeer);
    void SetPeerSyncStatus(uint64 nNonce,const uint256& hashFork,bool fSync);
protected:
    network::CMvPeerNet* pPeerNet;
    ICoreProtocol* pCoreProtocol;
    IWorldLine* pWorldLine;
    ITxPool* pTxPool;
    IDispatcher* pDispatcher;
    IService *pService;
    mutable boost::shared_mutex rwNetPeer; 
    std::map<uint256,CSchedule> mapSched; 
    std::map<uint64,CNetChannelPeer> mapPeer;
};

} // namespace multiverse

#endif //MULTIVERSE_NETCHN_H

