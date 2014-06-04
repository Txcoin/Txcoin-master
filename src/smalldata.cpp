// Copyright (c) 2014 The Txcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <boost/foreach.hpp>
#include <boost/tuple/tuple.hpp>

using namespace std;
using namespace boost;

#include "keystore.h"
#include "bignum.h"
#include "key.h"
#include "main.h"
#include "sync.h"
#include "util.h"
#include "smalldata.h"
#include "db.h"
#include "txdb.h"

bool fAdEnabled = false;

static unsigned char pchSmallDataHeader1[] = { 0xfa, 0xce, SMALLDATA_TYPE_PLAINTEXT, 0, 0} ;
static unsigned char pchSmallDataHeader2[] = { 0xfa, 0xce, SMALLDATA_TYPE_BROADCAST, 0, 0} ;
const unsigned char *GetSmallDataHeader(int type)
{
    switch (type)
    {
    case SMALLDATA_TYPE_PLAINTEXT:
        return pchSmallDataHeader1;
    case SMALLDATA_TYPE_BROADCAST:
        return pchSmallDataHeader2;
    default:
        break;
    }

    return NULL;
}

bool GetTxMessage(CTransaction &tx, std::string &msg, bool &isBroadcast)
{
    txnouttype whichType;
    BOOST_FOREACH(const CTxOut& txout, tx.vout) {
        if ( 0 != txout.nValue )
            continue;

        if (!::IsStandard(txout.scriptPubKey, whichType))
            continue;

        if (whichType == TX_NULL_DATA)
        {
            char start = 2;
            if ( txout.scriptPubKey[1] == 0x4c )
                start = 3;

            if ( txout.scriptPubKey[start] != 0xfa || txout.scriptPubKey[start + 1] != 0xce )
                return false;

            if ( txout.scriptPubKey[start + 2] == SMALLDATA_TYPE_PLAINTEXT )
                isBroadcast = false;
            else if ( txout.scriptPubKey[start + 2] == SMALLDATA_TYPE_BROADCAST )
                isBroadcast = true;
            else 
                return false;

            std::string str(txout.scriptPubKey.begin() + start + 4, txout.scriptPubKey.end());
            msg = str;
            return true;
        }
    }
    
    return false;
}


CAdTx::CAdTx()
{
}

CAdTx::CAdTx(const CTransaction& txIn) : CTransaction(txIn)
{
}

void CAdTx::GetText()
{
    if ( adText.empty() )
    {
        bool isBroadcast;
        GetTxMessage(*(CTransaction*)this, adText, isBroadcast);
    }
}

void CAdTx::GetFee()
{
    {
        CCoinsViewCache &view = *pcoinsTip;
        int64 nValueIn = 0;
        if (!this->IsCoinBase())
        {
            for (unsigned int i = 0; i < vin.size(); i++)
            {
                if (fTxIndex)
                {
                    CDiskTxPos postx;
                    if (pblocktree->ReadTxIndex(vin[i].prevout.hash, postx)) 
                    {
                        CTransaction txOut;
                        CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
                        CBlockHeader header;
                        try {
                            file >> header;
                            fseek(file, postx.nTxOffset, SEEK_CUR);
                            file >> txOut;
                        } catch (std::exception &e) {
                            printf("%s() : deserialize or I/O error\n", __PRETTY_FUNCTION__);
                            return;
                        }
                        if (txOut.GetHash() != vin[i].prevout.hash){
                            printf("%s() : txid mismatch\n", __PRETTY_FUNCTION__);
                            return;
                        }

                        nValueIn += txOut.vout[vin[i].prevout.n].nValue;
                        continue;
                    }
                }

                CCoins coins;
                if (view.GetCoins(vin[i].prevout.hash, coins))
                {
                    CBlockIndex *pindexSlow = FindBlockByHeight(coins.nHeight);
                    if (pindexSlow) {
                        CBlock block;
                        if (block.ReadFromDisk(pindexSlow)) {
                            BOOST_FOREACH(const CTransaction &tx, block.vtx) {
                                if (tx.GetHash() == vin[i].prevout.hash) {
                                    nValueIn += tx.vout[vin[i].prevout.n].nValue;
                                }
                            }
                        }
                    }
                }
            }
        }
        nFee = nValueIn - this->GetValueOut();
        if ( nFee < 0 )
            nFee = 0;
        //printf("CAdTx %s in=%"PRI64d" out=%"PRI64d" fee=%"PRI64d"\n", this->GetHash().GetHex().c_str(), nValueIn, this->GetValueOut(), nFee);
    }
}

int64 CAdTx::GetFeeCur()
{
    int interval = nBestHeight - nHeight;
    if ( interval < 0 )
        interval = 0;

    interval = 10000 - interval;
    if ( interval < 0 )
        interval = 0;

    return nFee * interval / 10000;
}

CAdManager adManager;

//
// CAdManager
//
CAdManager::CAdManager()
{
    nLastHeight = 0;
}

void CAdManager::load()
{
    if (fImporting || fReindex || !fAdEnabled)
        return;

    if ( vAdList.size() == 0 )
    {
        CAdDB adb;
        if (!adb.Read(adManager))
        {
            printf("Invalid or missing ad.dat; recreating\n");
            save();
        }

        int i;
        for ( i = 0; i < vAdList.size(); ++ i )
            vAdList[i].GetText();

        if ( 0 == nLastHeight )
            nLastHeight = nBestHeight - 10000;
        if ( nLastHeight < 0 )
            nLastHeight = 1;

        printf("CAdManager::load() nLastHeight = %d, nBestHeight = %d\n", nLastHeight, nBestHeight);
        for ( i = nLastHeight; i <= nBestHeight; ++ i )
        {
            CBlockIndex* pblockindex = FindBlockByHeight(i);
            if (pblockindex) 
            {
                CBlock blockTmp;
                if (!blockTmp.ReadFromDisk(pblockindex))
                    continue;

                processBlock(&blockTmp, i);
            }
        }
        printf("CAdManager::load finish\n");
    }
}

void CAdManager::save()
{
    if (fAdEnabled)
    {
        CAdDB adb;
        adb.Write(adManager);
    }
}

bool CAdManager::getAdList(std::vector<CAdTx> &adTxList, int max)
{
    adTxList.clear();

    if (fAdEnabled)
    {
        int size = vAdList.size() > max ? max : vAdList.size();
        adTxList.insert(adTxList.end(), vAdList.begin(), vAdList.begin() + size);
    }
#if 0
    if ( adTxList.size() < 15 )
    {
        CAdTx adDef;
        adDef.nFee = 0;
        adDef.nHeight = 0;
        adDef.adText = "<a href=\"http://transactioncoin.org/\">Txcoin</a>";
        adTxList.push_back(adDef);
    }
    if ( adTxList.size() < 15 )
    {
        CAdTx adDef;
        adDef.nFee = 0;
        adDef.nHeight = 0;
        adDef.adText = "<a href=\"https://bitcointalk.org/index.php?topic=512149.0\">Txcoin ANN!</a>";
        //adDef.adText = "<a href=\"http://transactioncoinmining.com/\">TXC and BTC Merged Mining Pool</a><br/><font color=\"#008800\">PPLNS 1% Fee!</font>";
        adTxList.push_back(adDef);
    }
#endif
    return true;
}


void CAdManager::processBlock(CBlock* pblock, int height)
{
    if (!fAdEnabled)
        return;

    if ( 0 == height )
    {
        CBlockIndex* pblockindex = mapBlockIndex[pblock->GetHash()];
        if ( pblockindex )
            height = pblockindex->nHeight;
    }

    if ( 0 == height )
        return;

    //printf("CAdManager::processBlock block=%s height=%d\n", pblock->GetHash().GetHex().c_str(), height);
    for ( int i = 0; i < pblock->vtx.size(); i ++ )
    {
        if ( pblock->vtx[i].IsCoinBase() )
            continue;

        std::string msg;
        bool isBroadcast;
        if ( GetTxMessage(pblock->vtx[i], msg, isBroadcast) )
        {
            if ( isBroadcast )
            {
                CAdTx adTx(pblock->vtx[i]);
                adTx.hashBlock = pblock->GetHash();
                adTx.nHeight = height;
                adTx.adText = msg;
                adTx.GetFee();
                //printf("CAdManager ad tx %s height=%d\n", adTx.GetHash().GetHex().c_str(), height);
                addToList(adTx);
            }
        }
    }

    nLastHeight = height;
}

void CAdManager::addToList(CAdTx &adTx)
{
    {
        LOCK(cs);
        int i = vAdList.size();

        while ( i > 0 )
        {
            if ( adTx.GetFeeCur() > vAdList[i - 1].GetFeeCur() )
                i = i - 1;
            else if ( adTx.GetHash() == vAdList[i - 1].GetHash() )
                return;
            else
                break;
        }
        vAdList.insert(vAdList.begin() + i, adTx);

        if ( vAdList.size() > 15 )
            vAdList.pop_back();
    }
}

//
// CAdDB
//
CAdDB::CAdDB()
{
    pathAddr = GetDataDir() / "ad.dat";
}

bool CAdDB::Write(const CAdManager& addr)
{
    // Generate random temporary filename
    unsigned short randv = 0;
    RAND_bytes((unsigned char *)&randv, sizeof(randv));
    std::string tmpfn = strprintf("ad.dat.%04x", randv);

    // serialize addresses, checksum data up to that point, then append csum
    CDataStream ssPeers(SER_DISK, CLIENT_VERSION);
    ssPeers << FLATDATA(pchMessageStart);
    ssPeers << addr;
    uint256 hash = Hash(ssPeers.begin(), ssPeers.end());
    ssPeers << hash;

    // open temp output file, and associate with CAutoFile
    boost::filesystem::path pathTmp = GetDataDir() / tmpfn;
    FILE *file = fopen(pathTmp.string().c_str(), "wb");
    CAutoFile fileout = CAutoFile(file, SER_DISK, CLIENT_VERSION);
    if (!fileout)
        return error("CAdManager::Write() : open failed");

    // Write and commit header, data
    try {
        fileout << ssPeers;
    }
    catch (std::exception &e) {
        return error("CAdManager::Write() : I/O error");
    }
    FileCommit(fileout);
    fileout.fclose();

    // replace existing peers.dat, if any, with new peers.dat.XXXX
    if (!RenameOver(pathTmp, pathAddr))
        return error("CAdManager::Write() : Rename-into-place failed");

    return true;
}

bool CAdDB::Read(CAdManager& addr)
{
    // open input file, and associate with CAutoFile
    FILE *file = fopen(pathAddr.string().c_str(), "rb");
    CAutoFile filein = CAutoFile(file, SER_DISK, CLIENT_VERSION);
    if (!filein)
        return error("CAdManager::Read() : open failed");

    // use file size to size memory buffer
    int fileSize = GetFilesize(filein);
    int dataSize = fileSize - sizeof(uint256);
    //Don't try to resize to a negative number if file is small
    if ( dataSize < 0 ) dataSize = 0;
    vector<unsigned char> vchData;
    vchData.resize(dataSize);
    uint256 hashIn;

    // read data and checksum from file
    try {
        filein.read((char *)&vchData[0], dataSize);
        filein >> hashIn;
    }
    catch (std::exception &e) {
        return error("CAdManager::Read() 2 : I/O error or stream data corrupted");
    }
    filein.fclose();

    CDataStream ssPeers(vchData, SER_DISK, CLIENT_VERSION);

    // verify stored checksum matches input data
    uint256 hashTmp = Hash(ssPeers.begin(), ssPeers.end());
    if (hashIn != hashTmp)
        return error("CAdManager::Read() : checksum mismatch; data corrupted");

    unsigned char pchMsgTmp[4];
    try {
        // de-serialize file header (pchMessageStart magic number) and
        ssPeers >> FLATDATA(pchMsgTmp);

        // verify the network matches ours
        if (memcmp(pchMsgTmp, pchMessageStart, sizeof(pchMsgTmp)))
            return error("CAdManager::Read() : invalid network magic number");

        // de-serialize address data into one CAddrMan object
        ssPeers >> addr;
    }
    catch (std::exception &e) {
        return error("CAdManager::Read() : I/O error or stream data corrupted");
    }

    return true;
}

