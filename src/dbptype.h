// Copyright (c) 2017-2018 The Multiverse developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef MULTIVERSE_DBP_TYPE_H
#define MULTIVERSE_DBP_TYPE_H

#include <boost/any.hpp>

namespace multiverse
{

class CMvDbpContent
{
public:
};

class CMvDbpRequest : public CMvDbpContent
{
public:
};

class CMvDbpRespond : public CMvDbpContent
{
public:
};

class CMvDbpConnect : public CMvDbpRequest
{
public:
    bool isReconnect;
    std::string session;
    int32 version;
    std::string client;
};

class CMvDbpSub : public CMvDbpRequest
{
public:
    std::string id;
    std::string name;
};

class CMvDbpUnSub : public CMvDbpRequest
{
public:
    std::string id;
};

class CMvDbpNoSub : public CMvDbpRespond
{
public:
    std::string id;
    std::string error;
};

class CMvDbpReady : public CMvDbpRespond
{
public:
    std::string id;
};

class CMvDbpTxIn
{
public:
    std::vector<uint8> hash;
    uint32 n;
};

class CMvDbpDestination
{
public:
    enum PREFIX
    {
        PREFIX_NULL = 0,
        PREFIX_PUBKEY = 1,
        PREFIX_TEMPLATE = 2,
        PREFIX_MAX = 3
    };

public:
    uint32 prefix;
    std::vector<uint8> data;
    uint32 size; //设置为33
};

class CMvDbpTransaction
{
public:
    uint32 nVersion;               //版本号,目前交易版本为 0x0001
    uint32 nType;                  //类型, 区分公钥地址交易、模板地址交易、即时业务交易和跨分支交易
    uint32 nLockUntil;             //交易冻结至高度为 nLockUntil 区块
    std::vector<uint8> hashAnchor; //交易有效起始区块 HASH
    std::vector<CMvDbpTxIn> vInput;
    CMvDbpDestination cDestination; // 输出地址
    int64 nAmount;                  //输出金额
    int64 nTxFee;                   //网络交易费
    std::vector<uint8> vchData;     //输出参数(模板地址参数、跨分支交易共轭交易)
    std::vector<uint8> vchSig;      //交易签名
    std::vector<uint8> hash;
};

class CMvDbpBlock
{
public:
    uint32 nVersion;
    uint32 nType;                       // 类型,区分创世纪块、主链区块、业务区块和业务子区块
    uint32 nTimeStamp;                  //时间戳，采用UTC秒单位
    std::vector<uint8> hashPrev;        //前一区块的hash
    std::vector<uint8> hashMerkle;      //Merkle tree的根
    std::vector<uint8> vchProof;        //用于校验共识合法性数据
    CMvDbpTransaction txMint;           // 出块奖励交易
    std::vector<CMvDbpTransaction> vtx; //区块打包的所有交易
    std::vector<uint8> vchSig;          //区块签名
    uint32 nHeight;                     // 当前区块高度
    std::vector<uint8> hash;            //当前区块的hash
};

class CMvDbpAdded : public CMvDbpRespond
{
public:
    std::string name;
    std::string id;
    boost::any anyAddedObj; // busniess object (block,tx...)
};

class CMvDbpMethod : public CMvDbpRequest
{
public:
    enum Method
    {
        GET_BLOCKS,
        GET_TX,
        SEND_TX
    };

    // param name => param value
    typedef std::map<std::string, std::string> ParamMap;

public:
    Method method;
    std::string id;
    ParamMap params;
};

class CMvDbpSendTxRet
{
public:
    std::string hash;
    std::string result;
    std::string reason;
};

class CMvDbpMethodResult : public CMvDbpRespond
{
public:
    std::string id;
    std::string error;
    std::vector<boost::any> anyResultObjs; // blocks,tx,send_tx_ret
};

class CMvDbpError : public CMvDbpRespond
{
public:
};

class CMvDbpConnected : public CMvDbpRespond
{
public:
    std::string session;
};

class CMvDbpPing : public CMvDbpRequest, public CMvDbpRespond
{
public:
    std::string id;
};

class CMvDbpPong : public CMvDbpRequest, public CMvDbpRespond
{
public:
    std::string id;
};

class CMvDbpFailed : public CMvDbpRespond
{
public:
    std::string reason;  // failed reason
    std::string session; // for delete session map
    std::vector<int32> versions;
};

class CMvDbpBroken
{
public:
    bool fEventStream;
};
} // namespace multiverse
#endif //MULTIVERSE_DBP_TYPE_H
