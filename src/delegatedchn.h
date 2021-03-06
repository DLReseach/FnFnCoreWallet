// Copyright (c) 2017-2018 The Multiverse developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef  MULTIVERSE_DELEGATEDCHN_H
#define  MULTIVERSE_DELEGATEDCHN_H

#include "mvpeernet.h"
#include "mvbase.h"

namespace multiverse
{

class CDelegatedDataIdent
{
public:
    CDelegatedDataIdent(const uint256& hashAnchorIn = 0, uint32 nInvTypeIn = 0,
                        const CDestination& destDelegatedIn = CDestination())
    : hashAnchor(hashAnchorIn), nInvType(nInvTypeIn), destDelegated(destDelegatedIn)
    {
    }
    friend inline bool operator<(const CDelegatedDataIdent& a,const CDelegatedDataIdent& b)
    {
        return (a.hashAnchor < b.hashAnchor 
                || (a.hashAnchor == b.hashAnchor && a.nInvType < b.nInvType)
                || (a.hashAnchor == b.hashAnchor && a.nInvType == b.nInvType 
                    && a.destDelegated < b.destDelegated));
    }
    friend inline bool operator==(const CDelegatedDataIdent& a,const CDelegatedDataIdent& b)
    {
        return (a.hashAnchor == b.hashAnchor && a.nInvType == b.nInvType && a.destDelegated == b.destDelegated);
    }
public:
    uint256 hashAnchor;
    uint32 nInvType;
    CDestination destDelegated;
};

class CDelegatedChannelPeer : public walleve::CDataPeer<CDelegatedDataIdent>
{
public:
    CDelegatedChannelPeer(uint64 nNonceIn)
    : CDataPeer<CDelegatedDataIdent>(nNonceIn), hashAnchor(0), bmDistribute(0), bmPublish(0)
    {
    }
    void Renew(const uint256& hashAnchorIn,const network::CMvEventPeerDelegatedBulletin& bulletin)
    {
        hashAnchor = hashAnchorIn;
        bmDistribute = bulletin.bmDistribute;
        bmPublish = bulletin.bmPublish;
        mapDistributeBitmap.clear();

        for (std::size_t i = 0;i < bulletin.vBitmap.size();++i)
        {
            mapDistributeBitmap.insert(std::make_pair(bulletin.vBitmap[i].hashAnchor,bulletin.vBitmap[i].bitmap));
        }
    }
    void Update(const uint256& hashAnchorIn,const network::CMvEventPeerDelegatedBulletin& bulletin)
    {
        if (hashAnchor == hashAnchorIn)
        {
            bmDistribute |= bulletin.bmDistribute;
            bmPublish |= bulletin.bmPublish;
        }
        else
        {
            std::map<uint256,uint64>::iterator it = mapDistributeBitmap.find(hashAnchorIn);
            if (it != mapDistributeBitmap.end())
            {
                (*it).second |= bulletin.bmDistribute;
            }
        }
        for (std::size_t i = 0;i < bulletin.vBitmap.size();++i)
        {
            std::map<uint256,uint64>::iterator it = mapDistributeBitmap.find(bulletin.vBitmap[i].hashAnchor);
            if (it != mapDistributeBitmap.end())
            {
                (*it).second |= bulletin.vBitmap[i].bitmap;
            }
        }
    }
    bool HaveUnknown(const uint256& hashAnchorIn,const network::CMvEventPeerDelegatedBulletin& bulletin)
    {
        if (hashAnchorIn == hashAnchor && (bulletin.bmDistribute | bmDistribute) == bmDistribute
                                       &&  (bulletin.bmPublish | bmPublish) == bmPublish)
        {
            std::size_t i = 0;
            while (i < bulletin.vBitmap.size())
            {
                uint64 bitmap = mapDistributeBitmap[bulletin.vBitmap[i].hashAnchor];
                if ((bulletin.vBitmap[i].bitmap | bitmap) != bitmap)
                {
                    break;
                }
                ++i;
            }
            return (i < bulletin.vBitmap.size());
        }
        return true;
    }
public:
    uint256 hashAnchor;
    uint64 bmDistribute;
    uint64 bmPublish;
    std::map<uint256,uint64> mapDistributeBitmap;
};

class CDelegatedChannelChainData
{
public:
    uint64 GetBitmap(const std::map<CDestination,std::vector<unsigned char> >& mapData) const
    {
        uint64 bitmap = 0;
        for (std::size_t i = 0; i < vEnrolled.size(); i++)
        {
            bitmap |= mapData.count(vEnrolled[i]) << i;
        }
        return bitmap;
    }
    void GetKnownData(const uint64 bitmap,std::set<CDestination>& setDestination) const
    {
        for (std::size_t i = 0; i < vEnrolled.size(); i++)
        {
            if ((bitmap & (1 << i)))
            {
                setDestination.insert(vEnrolled[i]);
            }
        } 
    }
public:
    std::map<CDestination,std::vector<unsigned char> > mapDistributeData;
    std::map<CDestination,std::vector<unsigned char> > mapPublishData;
    std::vector<CDestination> vEnrolled;
};

class CDelegatedChannelChain
{
public:
    CDelegatedChannelChain() { Clear(); }
    std::list<uint256>& GetHashList() { return listBlockHash; }
    void Clear();
    void Update(int nStartHeight,
                const std::vector<std::pair<uint256,std::map<CDestination,size_t> > >& vEnrolledWeight,
                const std::map<CDestination,std::vector<unsigned char> >& mapDistributeData,
                const std::map<CDestination,std::vector<unsigned char> >& mapPublishData);
    uint64 GetDistributeBitmap(const uint256& hashAnchor);
    uint64 GetPublishBitmap(const uint256& hashAnchor);
    void GetDistribute(const uint256& hashAnchor, uint64 bmDistribute,std::set<CDestination>& setDestination);
    void GetPublish(const uint256& hashAnchor, uint64 bmPublish,std::set<CDestination>& setDestination);
    void AskForDistribute(const uint256& hashAnchor, uint64 bmDistribute, std::set<CDestination>& setDestination);
    void AskForPublish(const uint256& hashAnchor, uint64 bmDistribute, std::set<CDestination>& setDestination);
    bool GetDistributeData(const uint256& hashAnchor, const CDestination& dest, std::vector<unsigned char>& vchData);
    bool GetPublishData(const uint256& hashAnchor, const CDestination& dest, std::vector<unsigned char>& vchData);
    bool IsOutOfDistributeRange(const uint256& hashAnchor) const;
    bool IsOutOfPublishRange(const uint256& hashAnchor) const;
    bool InsertDistributeData(const uint256& hashAnchor, const CDestination& dest, const std::vector<unsigned char>& vchData);
    bool InsertPublishData(const uint256& hashAnchor, const CDestination& dest, const std::vector<unsigned char>& vchData);
public:
    std::map<uint256,CDelegatedChannelChainData> mapChainData;
    std::list<uint256> listBlockHash;
    int nLastBlockHeight;
};

class CDelegatedChannel : public network::IMvDelegatedChannel
{
public:
    CDelegatedChannel();
    ~CDelegatedChannel();
    void PrimaryUpdate(int nStartHeight,
                       const std::vector<std::pair<uint256,std::map<CDestination,size_t> > >& vEnrolledWeight,
                       const std::map<CDestination,std::vector<unsigned char> >& mapDistributeData,
                       const std::map<CDestination,std::vector<unsigned char> >& mapPublishData) override;
protected:
    bool WalleveHandleInitialize() override;
    void WalleveHandleDeinitialize() override;
    bool WalleveHandleInvoke() override;
    void WalleveHandleHalt() override;

    bool HandleEvent(network::CMvEventPeerActive& eventActive) override;
    bool HandleEvent(network::CMvEventPeerDeactive& eventDeactive) override;
    bool HandleEvent(network::CMvEventPeerBulletin& eventBulletin) override;
    bool HandleEvent(network::CMvEventPeerGetDelegated& eventGetDelegated) override;
    bool HandleEvent(network::CMvEventPeerDistribute& eventDistribute) override;
    bool HandleEvent(network::CMvEventPeerPublish& eventPublish) override;

    void BroadcastBulletin();
    bool DispatchGetDelegated();
    void AddPeerKnownDistrubute(uint64 nNonce, const uint256& hashAnchor, uint64 bmDistrubute);
    void AddPeerKnownPublish(uint64 nNonce, const uint256& hashAnchor, uint64 bmPublish);
    void DispatchMisbehaveEvent(uint64 nNonce, walleve::CEndpointManager::CloseReason reason);
    std::shared_ptr<CDelegatedChannelPeer> GetPeer(uint64 nNonce)
    {
        return std::static_pointer_cast<CDelegatedChannelPeer>(schedPeer.GetPeer(nNonce));
    }
protected:
    network::CMvPeerNet* pPeerNet;
    ICoreProtocol* pCoreProtocol;
    IWorldLine* pWorldLine;
    IDispatcher* pDispatcher;
    mutable boost::shared_mutex rwPeer; 
    walleve::CDataScheduler<CDelegatedDataIdent> schedPeer;
    CDelegatedChannelChain dataChain;
};

} // namespace multiverse

#endif //MULTIVERSE_DELEGATEDCHN_H

