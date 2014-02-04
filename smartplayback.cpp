/*
 * Copyright (C) 2004-2014  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Modules.h>
#include <znc/Client.h>
#include <znc/Buffer.h>
#include <znc/Chan.h>
#include <znc/znc.h>
#include <sys/time.h>

class CSmartPlaybackMod : public CModule
{
public:
    MODCONSTRUCTOR(CSmartPlaybackMod) { }

    virtual bool OnLoad(const CString& sArgs, CString& sMessage)
    {
        m_mSince.clear();
        return true;
    }

    virtual void OnClientLogin()
    {
        m_mSince.erase(GetClient());
    }

    virtual void OnClientDisconnect()
    {
        m_mSince.erase(GetClient());
    }

    virtual void OnClientCapLs(CClient* pClient, SCString& ssCaps)
    {
        ssCaps.insert("znc.in/smartplayback");
    }

    virtual bool IsClientCapSupported(CClient* pClient, const CString& sCap, bool bState)
    {
        return sCap.StartsWith("znc.in/smartplayback");
    }

    virtual void OnClientCapRequest(CClient* pClient, const CString& sCap, bool bState)
    {
        if (sCap.StartsWith("znc.in/smartplayback/")) {
            if (bState)
                m_mSince[pClient] = sCap.TrimPrefix_n("znc.in/smartplayback/").ToLong();
            else
                m_mSince.erase(pClient);
        }
    }

    virtual EModRet OnChanBufferStarting(CChan& Chan, CClient& Client)
    {
        return HALTCORE;
    }

    virtual EModRet OnChanBufferPlayLine(CChan& Chan, CClient& Client, CString& sLine)
    {
        return HALTCORE;
    }

    virtual EModRet OnChanBufferEnding(CChan& Chan, CClient& Client)
    {
        VCString vsLines;
        CBuffer Buffer = Chan.GetBuffer();
        long lSince = GetSince(&Client);
        for (size_t uIdx = 0; uIdx < Buffer.Size(); ++uIdx) {
            CBufLine BufLine = Buffer.GetBufLine(uIdx);
            if (lSince == 0 || BufLine.GetTime().tv_sec > lSince)
                vsLines.push_back(BufLine.GetLine(Client, MCString::EmptyMap));
        }
        if (!vsLines.empty()) {
            Client.PutClient(":***!znc@znc.in PRIVMSG " + Chan.GetName() + " :Buffer Playback...");
            for (size_t uIdx = 0; uIdx < vsLines.size(); ++uIdx)
                Client.PutClient(vsLines.at(uIdx));
            Client.PutClient(":***!znc@znc.in PRIVMSG " + Chan.GetName() + " :Playback Complete.");
        }
        return HALTCORE;
    }

    virtual EModRet OnSendToClient(CClient* pClient, CString& sLine)
    {
        if (pClient && pClient->IsAttached() && pClient->HasServerTime()) {
            if (pClient->IsCapEnabled("znc.in/smartplayback") && sLine.StartsWith(":")) {
                timeval tv;
                if (gettimeofday(&tv, NULL) == -1) {
                    tv.tv_sec = time(NULL);
                    tv.tv_usec = 0;
                }
                sLine = CUtils::FormatServerTime(tv) + " " + sLine;
            }
        }
        return CONTINUE;
    }

private:
    long GetSince(CClient* pClient) const
    {
        long lSince = 0;
        std::map<CClient*, long>::const_iterator it = m_mSince.find(pClient);
        if (it != m_mSince.end())
            lSince = it->second;
        return lSince;
    }

    std::map<CClient*, long> m_mSince;
};

GLOBALMODULEDEFS(CSmartPlaybackMod, "A smart playback module for ZNC")
