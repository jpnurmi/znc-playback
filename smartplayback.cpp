/*
 * Copyright (C) 2004-2014  See the AUTHORS file for details.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <znc/Modules.h>
#include <znc/IRCNetwork.h>
#include <znc/Client.h>
#include <znc/Buffer.h>
#include <znc/Chan.h>
#include <znc/znc.h>
#include <sys/time.h>

class CSmartPlaybackMod : public CModule
{
public:
    MODCONSTRUCTOR(CSmartPlaybackMod) { }

    virtual void OnClientCapLs(CClient* pClient, SCString& ssCaps)
    {
        ssCaps.insert("znc.in/smartplayback");
    }

    virtual bool IsClientCapSupported(CClient* pClient, const CString& sCap, bool bState)
    {
        return sCap.StartsWith("znc.in/smartplayback");
    }

    virtual EModRet OnUserRaw(CString& sLine)
    {
        CClient* pClient = GetClient();
        CIRCNetwork* pNetwork = GetNetwork();
        if (pClient && pNetwork && sLine.Token(0).Equals("PLAYBACK")) {
            // PLAYBACK <chan(s)> [timestamp]
            SCString ssArgs;
            sLine.Token(1).Split(",", ssArgs, false);

            std::vector<CChan*> vChans;
            for (SCString::const_iterator it = ssArgs.begin(); it != ssArgs.end(); ++it) {
                std::vector<CChan*> vFound = FindChans(*it);
                vChans.insert(vChans.end(), vFound.begin(), vFound.end());
            }

            if (!vChans.empty()) {
                time_t iSince = sLine.Token(2).ToLong();
                for (std::vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it)
                    PutBuffer(pClient, (*it), iSince);
            }
            return HALTCORE;
        }
        return CONTINUE;
    }

    virtual EModRet OnChanBufferStarting(CChan& Chan, CClient& Client)
    {
        if (Client.IsCapEnabled("znc.in/smartplayback"))
            return HALTCORE;
        return CONTINUE;
    }

    virtual EModRet OnChanBufferPlayLine(CChan& Chan, CClient& Client, CString& sLine)
    {
        if (Client.IsCapEnabled("znc.in/smartplayback"))
            return HALTCORE;
        return CONTINUE;
    }

    virtual EModRet OnChanBufferEnding(CChan& Chan, CClient& Client)
    {
        if (Client.IsCapEnabled("znc.in/smartplayback"))
            return HALTCORE;
        return CONTINUE;
    }

    virtual EModRet OnSendToClient(CClient* pClient, CString& sLine)
    {
        if (pClient && pClient->IsAttached() && pClient->HasServerTime()) {
            // TODO: proper handling for message tags (CUtils?)
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
    void PutBuffer(CClient* pClient, const CChan* pChan, time_t iSince = 0)
    {
        assert(pClient);
        assert(pChan);

        VCString vsLines;
        const CBuffer& Buffer = pChan->GetBuffer();
        for (size_t uIdx = 0; uIdx < Buffer.Size(); ++uIdx) {
            const CBufLine& BufLine = Buffer.GetBufLine(uIdx);
            if (iSince <= 0 || BufLine.GetTime().tv_sec > iSince)
                vsLines.push_back(BufLine.GetLine(*pClient, MCString::EmptyMap));
        }

        if (!vsLines.empty()) {
            pClient->PutClient(":***!znc@znc.in PRIVMSG " + pChan->GetName() + " :Buffer Playback...");
            for (size_t uIdx = 0; uIdx < vsLines.size(); ++uIdx)
                pClient->PutClient(vsLines.at(uIdx));
            pClient->PutClient(":***!znc@znc.in PRIVMSG " + pChan->GetName() + " :Playback Complete.");
        }
    }

    std::vector<CChan*> FindChans(const CString& sWild)
    {
        std::vector<CChan*> vFound;
        const CIRCNetwork *pNetwork = GetNetwork();
        if (pNetwork) {
            const std::vector<CChan*>& vChans = pNetwork->GetChans();
            for (std::vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it) {
                if ((*it)->GetName().WildCmp(sWild))
                    vFound.push_back(*it);
            }
        }
        return vFound;
    }
};

GLOBALMODULEDEFS(CSmartPlaybackMod, "A smart playback module for ZNC")
