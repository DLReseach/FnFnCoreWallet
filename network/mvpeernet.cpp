// Copyright (c) 2017-2018 The Multiverse developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "mvpeernet.h"
#include "mvpeer.h"
#include <boost/bind.hpp>
#include <boost/any.hpp>

#define HANDSHAKE_TIMEOUT               (5)
#define RESPONSE_TX_TIMEOUT             (15)
#define RESPONSE_BLOCK_TIMEOUT          (120)
#define RESPONSE_DISTRIBUTE_TIMEOUT     (5)
#define RESPONSE_PUBLISH_TIMEOUT        (10)
#define NODE_ACTIVE_TIME                (3 * 60 * 60)

using namespace std;
using namespace walleve;
using namespace multiverse::network;
using boost::asio::ip::tcp;

//////////////////////////////
// CMvPeerNet

CMvPeerNet::CMvPeerNet()
: CPeerNet("peernet")
{   
    nMagicNum = 0;
    nVersion  = 0;
    nService  = 0;
    fEnclosed = false;
    pNetChannel = NULL;
    pDelegatedChannel = NULL;
}   

CMvPeerNet::~CMvPeerNet()
{   
}   

bool CMvPeerNet::WalleveHandleInitialize()
{
    if (!WalleveGetObject("netchannel",pNetChannel))
    {
        WalleveLog("Failed to request peer net datachannel\n");
        return false;
    }

    if (!WalleveGetObject("delegatedchannel",pDelegatedChannel))
    {
        WalleveLog("Failed to request peer net datachannel\n");
        return false;
    }

    return true;
}

void CMvPeerNet::WalleveHandleDeinitialize()
{
    setDNSeed.clear();
    pNetChannel = NULL;
    pDelegatedChannel = NULL;
}

bool CMvPeerNet::HandleEvent(CMvEventPeerInv& eventInv)
{
    CWalleveBufStream ssPayload;
    ssPayload << eventInv;
    return SendDataMessage(eventInv.nNonce,MVPROTO_CMD_INV,ssPayload);
}

bool CMvPeerNet::HandleEvent(CMvEventPeerGetData& eventGetData)
{
    CWalleveBufStream ssPayload;
    ssPayload << eventGetData;
    if (!SendDataMessage(eventGetData.nNonce,MVPROTO_CMD_GETDATA,ssPayload))
    {
        return false;
    }

    SetInvTimer(eventGetData.nNonce,eventGetData.data);
    return true;
}

bool CMvPeerNet::HandleEvent(CMvEventPeerGetBlocks& eventGetBlocks)
{
    CWalleveBufStream ssPayload;
    ssPayload << eventGetBlocks;
    return SendDataMessage(eventGetBlocks.nNonce,MVPROTO_CMD_GETBLOCKS,ssPayload);
}

bool CMvPeerNet::HandleEvent(CMvEventPeerTx& eventTx)
{
    CWalleveBufStream ssPayload;
    ssPayload << eventTx;
    return SendDataMessage(eventTx.nNonce,MVPROTO_CMD_TX,ssPayload);
}

bool CMvPeerNet::HandleEvent(CMvEventPeerBlock& eventBlock)
{
    CWalleveBufStream ssPayload;
    ssPayload << eventBlock;
    return SendDataMessage(eventBlock.nNonce,MVPROTO_CMD_BLOCK,ssPayload);
}

bool CMvPeerNet::HandleEvent(CMvEventPeerBulletin& eventBulletin)
{
    CWalleveBufStream ssPayload;
    ssPayload << eventBulletin;
    return SendDelegatedMessage(eventBulletin.nNonce,MVPROTO_CMD_BULLETIN,ssPayload);
}

bool CMvPeerNet::HandleEvent(CMvEventPeerGetDelegated& eventGetDelegated)
{
    CWalleveBufStream ssPayload;
    ssPayload << eventGetDelegated;
    if (!SendDelegatedMessage(eventGetDelegated.nNonce,MVPROTO_CMD_GETDELEGATED,ssPayload))
    {
        return false;
    }

    CWalleveBufStream ss;
    ss << eventGetDelegated.hashAnchor << eventGetDelegated.data.destDelegate;
    uint256 hash = crypto::CryptoHash(ss.GetData(),ss.GetSize());

    vector<CInv> vInv;
    vInv.push_back(CInv(eventGetDelegated.data.nInvType,hash));

    SetInvTimer(eventGetDelegated.nNonce,vInv);
    return true;
}

bool CMvPeerNet::HandleEvent(CMvEventPeerDistribute& eventDistribute)
{
    CWalleveBufStream ssPayload;
    ssPayload << eventDistribute;
    return SendDelegatedMessage(eventDistribute.nNonce,MVPROTO_CMD_DISTRIBUTE,ssPayload);
}

bool CMvPeerNet::HandleEvent(CMvEventPeerPublish& eventPublish)
{
    CWalleveBufStream ssPayload;
    ssPayload << eventPublish;
    return SendDelegatedMessage(eventPublish.nNonce,MVPROTO_CMD_PUBLISH,ssPayload);
}

CPeer* CMvPeerNet::CreatePeer(CIOClient *pClient,uint64 nNonce,bool fInBound)
{
    uint32_t nTimerId = SetTimer(nNonce,HANDSHAKE_TIMEOUT);
    CMvPeer *pPeer = new CMvPeer(this,pClient,nNonce,fInBound,nMagicNum,nTimerId);
    if (pPeer == NULL)
    {   
        CancelTimer(nTimerId);
    }
    return pPeer;
}

void CMvPeerNet::DestroyPeer(CPeer* pPeer)
{
    CMvPeer *pMvPeer = static_cast<CMvPeer *>(pPeer);
    if (pMvPeer->IsHandshaked())
    {
        CMvEventPeerDeactive* pEventDeactive = new CMvEventPeerDeactive(pMvPeer->GetNonce());
        if (pEventDeactive != NULL)
        {
            pEventDeactive->data = CAddress(pMvPeer->nService,pMvPeer->GetRemote());

            CMvEventPeerDeactive* pEventDeactiveDelegated = new CMvEventPeerDeactive(*pEventDeactive);

            pNetChannel->PostEvent(pEventDeactive);

            if (pEventDeactiveDelegated != NULL)
            {
                pDelegatedChannel->PostEvent(pEventDeactiveDelegated);
            }
        }
    }
    CPeerNet::DestroyPeer(pPeer);
}

CPeerInfo* CMvPeerNet::GetPeerInfo(CPeer* pPeer,CPeerInfo* pInfo)
{
    pInfo = new CMvPeerInfo();
    if (pInfo != NULL)
    {
        CMvPeer *pMvPeer = static_cast<CMvPeer *>(pPeer);
        CMvPeerInfo *pMvInfo = static_cast<CMvPeerInfo *>(pInfo);
        CPeerNet::GetPeerInfo(pPeer,pInfo);
        pMvInfo->nVersion = pMvPeer->nVersion;
        pMvInfo->nService = pMvPeer->nService;
        pMvInfo->strSubVer = pMvPeer->strSubVer;
        pMvInfo->nStartingHeight = pMvPeer->nStartingHeight;
    }
    return pInfo;
}

bool CMvPeerNet::SendDataMessage(uint64 nNonce,int nCommand,CWalleveBufStream& ssPayload)
{
    CMvPeer *pMvPeer = static_cast<CMvPeer *>(GetPeer(nNonce));
    if (pMvPeer == NULL)
    {
        return false;
    }
    return pMvPeer->SendMessage(MVPROTO_CHN_DATA,nCommand,ssPayload);
}

bool CMvPeerNet::SendDelegatedMessage(uint64 nNonce,int nCommand,walleve::CWalleveBufStream& ssPayload)
{
    CMvPeer *pMvPeer = static_cast<CMvPeer *>(GetPeer(nNonce));
    if (pMvPeer == NULL)
    {
        return false;
    }
    return pMvPeer->SendMessage(MVPROTO_CHN_DELEGATE,nCommand,ssPayload);
}

void CMvPeerNet::SetInvTimer(uint64 nNonce,vector<CInv>& vInv)
{
    const int64 nTimeout[] = { 0, RESPONSE_TX_TIMEOUT, RESPONSE_BLOCK_TIMEOUT,
                                  RESPONSE_DISTRIBUTE_TIMEOUT, RESPONSE_PUBLISH_TIMEOUT};
    CMvPeer *pMvPeer = static_cast<CMvPeer *>(GetPeer(nNonce));
    if (pMvPeer != NULL)
    {
        int64 nElapse = 0;
        BOOST_FOREACH(CInv &inv,vInv)
        {
            if (inv.nType >= CInv::MSG_TX && inv.nType <= CInv::MSG_PUBLISH)
            {
                nElapse += nTimeout[inv.nType];
                uint32 nTimerId = SetTimer(nNonce,nElapse);
                CancelTimer(pMvPeer->Request(inv,nTimerId));
            }
        }
    }
}

void CMvPeerNet::ProcessAskFor(CPeer* pPeer)
{
    uint256 hashFork;
    CInv inv;
    CMvPeer *pMvPeer = static_cast<CMvPeer *>(pPeer);
    if (pMvPeer->FetchAskFor(hashFork,inv))
    {
        CMvEventPeerGetData* pEventGetData = new CMvEventPeerGetData(pMvPeer->GetNonce(),hashFork);
        if (pEventGetData != NULL)
        {
            pEventGetData->data.push_back(inv);
            pNetChannel->PostEvent(pEventGetData);
        }
    }
}

void CMvPeerNet::BuildHello(CPeer *pPeer,CWalleveBufStream& ssPayload)
{
    uint64 nNonce = pPeer->GetNonce();
    int64 nTime = WalleveGetNetTime();
    int nHeight = pNetChannel->GetPrimaryChainHeight();
    ssPayload << nVersion << nService << nTime << nNonce << subVersion << nHeight; 
}

void CMvPeerNet::HandlePeerWriten(CPeer *pPeer)
{
    ProcessAskFor(pPeer);
}

bool CMvPeerNet::HandlePeerHandshaked(CPeer *pPeer,uint32 nTimerId)
{
    CMvPeer *pMvPeer = static_cast<CMvPeer *>(pPeer);
    CancelTimer(nTimerId);
    if (!CheckPeerVersion(pMvPeer->nVersion,pMvPeer->nService,pMvPeer->strSubVer))
    {
        return false;
    }
    if (!pMvPeer->IsInBound())
    {
        tcp::endpoint ep = pMvPeer->GetRemote();
        if (GetPeer(pMvPeer->nNonceFrom) != NULL)
        {
            RemoveNode(ep);
            return false;
        }

        string strName = GetNodeName(ep);
        if (strName == "dnseed")
        {
            setDNSeed.insert(ep);
        }

        SetNodeData(ep,boost::any(pMvPeer->nService));
    }

    WalleveUpdateNetTime(pMvPeer->GetRemote().address(),pMvPeer->nTimeDelta);

    CMvEventPeerActive* pEventActive = new CMvEventPeerActive(pMvPeer->GetNonce());
    if (pEventActive == NULL)
    {
        return false;
    }

    pEventActive->data = CAddress(pMvPeer->nService,pMvPeer->GetRemote());
    CMvEventPeerActive* pEventActiveDelegated = new CMvEventPeerActive(*pEventActive);

    pNetChannel->PostEvent(pEventActive);
    if (pEventActiveDelegated != NULL)
    {
        pDelegatedChannel->PostEvent(pEventActiveDelegated);
    }

    if (!fEnclosed)
    {
        pMvPeer->SendMessage(MVPROTO_CHN_NETWORK,MVPROTO_CMD_GETADDRESS);
    }
    return true;
}

bool CMvPeerNet::HandlePeerRecvMessage(CPeer *pPeer,int nChannel,int nCommand,CWalleveBufStream& ssPayload)
{
    CMvPeer *pMvPeer = static_cast<CMvPeer *>(pPeer);
    if (nChannel == MVPROTO_CHN_NETWORK)
    {
        switch (nCommand)
        {
        case MVPROTO_CMD_GETADDRESS:
            {
                vector<CNodeAvail> vNode;
                RetrieveGoodNode(vNode,NODE_ACTIVE_TIME,500);
                vector<CAddress> vAddr;
                BOOST_FOREACH(const CNodeAvail& node,vNode)
                {
                    if (node.data.type() == typeid(uint64) && IsRoutable(node.ep.address()))
                    {
                        uint64 nService = boost::any_cast<uint64>(node.data);
                        vAddr.push_back(CAddress(nService,node.ep));
                    }
                }
                CWalleveBufStream ss;
                ss << vAddr;
                return pMvPeer->SendMessage(MVPROTO_CHN_NETWORK,MVPROTO_CMD_ADDRESS,ss);
            }
            break;
        case MVPROTO_CMD_ADDRESS:
            if (!fEnclosed)
            {
                vector<CAddress> vAddr;
                ssPayload >> vAddr;
                if (vAddr.size() > 500)
                {
                    return false;
                }
                BOOST_FOREACH(CAddress& addr,vAddr)
                {
                    tcp::endpoint ep;
                    addr.ssEndpoint.GetEndpoint(ep);          
                    if ((addr.nService & NODE_NETWORK) == NODE_NETWORK 
                         && IsRoutable(ep.address()) && !setDNSeed.count(ep))
                    {   
                        AddNewNode(ep,ep.address().to_string(),boost::any(addr.nService));
                    }
                }
                if (setDNSeed.count(pMvPeer->GetRemote()))
                {
                    RemoveNode(pMvPeer->GetRemote());
                }
                return true;
            }
            break;
        case MVPROTO_CMD_PING:
            return pMvPeer->SendMessage(MVPROTO_CHN_NETWORK,MVPROTO_CMD_PONG,ssPayload);
            break;
        case MVPROTO_CMD_PONG:
            return true;
            break;
        default:
            break;
        }
    }
    else if (nChannel == MVPROTO_CHN_DATA)
    {
        uint256 hashFork;
        ssPayload >> hashFork;
        switch (nCommand)
        {
        case MVPROTO_CMD_GETBLOCKS:
            {
                CMvEventPeerGetBlocks* pEvent = new CMvEventPeerGetBlocks(pMvPeer->GetNonce(),hashFork);
                if (pEvent != NULL)
                {
                    ssPayload >> pEvent->data;
                    pNetChannel->PostEvent(pEvent);
                    return true;
                } 
            }
            break;
        case MVPROTO_CMD_GETDATA:
            {
                vector<CInv> vInv;
                ssPayload >> vInv;
                pMvPeer->AskFor(hashFork,vInv);
                ProcessAskFor(pPeer);
                return true;
            }
            break;
        case MVPROTO_CMD_INV:
            {
                CMvEventPeerInv* pEvent = new CMvEventPeerInv(pMvPeer->GetNonce(),hashFork);
                if (pEvent != NULL)
                {
                    ssPayload >> pEvent->data;
                    pNetChannel->PostEvent(pEvent);
                    return true;
                } 
            }
            break;
        case MVPROTO_CMD_TX:
            {
                CMvEventPeerTx* pEvent = new CMvEventPeerTx(pMvPeer->GetNonce(),hashFork);
                if (pEvent != NULL)
                {
                    ssPayload >> pEvent->data;
                    CInv inv(CInv::MSG_TX,pEvent->data.GetHash());
                    CancelTimer(pMvPeer->Responded(inv));
                    pNetChannel->PostEvent(pEvent);
                    return true;
                }
            }
            break;
        case MVPROTO_CMD_BLOCK:
            {
                CMvEventPeerBlock* pEvent = new CMvEventPeerBlock(pMvPeer->GetNonce(),hashFork);
                if (pEvent != NULL)
                {           
                    ssPayload >> pEvent->data;
                    CInv inv(CInv::MSG_BLOCK,pEvent->data.GetHash());
                    CancelTimer(pMvPeer->Responded(inv));
                    pNetChannel->PostEvent(pEvent);
                    return true;
                }
            }
            break;
        default:
            break;
        }
    }
    else if (nChannel == MVPROTO_CHN_DELEGATE)
    {
        uint256 hashAnchor;
        ssPayload >> hashAnchor;
        switch (nCommand)
        {
        case MVPROTO_CMD_BULLETIN:
            {
                CMvEventPeerBulletin* pEvent = new CMvEventPeerBulletin(pMvPeer->GetNonce(),hashAnchor);
                if (pEvent != NULL)
                {           
                    ssPayload >> pEvent->data;
                    pDelegatedChannel->PostEvent(pEvent);
                    return true;
                }
            }
            break;
        case MVPROTO_CMD_GETDELEGATED:
            {
                CMvEventPeerGetDelegated* pEvent = new CMvEventPeerGetDelegated(pMvPeer->GetNonce(),hashAnchor);
                if (pEvent != NULL)
                {           
                    ssPayload >> pEvent->data;
                    pDelegatedChannel->PostEvent(pEvent);
                    return true;
                }
            }
            break;
        case MVPROTO_CMD_DISTRIBUTE:
            {
                CMvEventPeerDistribute* pEvent = new CMvEventPeerDistribute(pMvPeer->GetNonce(),hashAnchor);
                if (pEvent != NULL)
                {           
                    ssPayload >> pEvent->data;

                    CWalleveBufStream ss;
                    ss << hashAnchor << (pEvent->data.destDelegate);
                    uint256 hash = crypto::CryptoHash(ss.GetData(),ss.GetSize());
                    CInv inv(CInv::MSG_DISTRIBUTE,hash);
                    CancelTimer(pMvPeer->Responded(inv));

                    pDelegatedChannel->PostEvent(pEvent);

                    return true;
                }
            }
            break;
        case MVPROTO_CMD_PUBLISH:
            {
                CMvEventPeerPublish* pEvent = new CMvEventPeerPublish(pMvPeer->GetNonce(),hashAnchor);
                if (pEvent != NULL)
                {           
                    ssPayload >> pEvent->data;

                    CWalleveBufStream ss;
                    ss << hashAnchor << (pEvent->data.destDelegate);
                    uint256 hash = crypto::CryptoHash(ss.GetData(),ss.GetSize());
                    CInv inv(CInv::MSG_PUBLISH,hash);
                    CancelTimer(pMvPeer->Responded(inv));

                    pDelegatedChannel->PostEvent(pEvent);
                    return true;
                }
            }
            break;
        default:
            break;
        }
    }
    return false;
}
