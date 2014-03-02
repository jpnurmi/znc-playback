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

static const char* SmartPlaybackCap = "znc.in/smartplayback";

class CSmartPlaybackMod : public CModule
{
public:
    MODCONSTRUCTOR(CSmartPlaybackMod) { }

    virtual void OnClientCapLs(CClient* pClient, SCString& ssCaps)
    {
        ssCaps.insert(SmartPlaybackCap);
    }

    virtual bool IsClientCapSupported(CClient* pClient, const CString& sCap, bool bState)
    {
        return sCap.Equals(SmartPlaybackCap);
    }

    virtual EModRet OnChanBufferStarting(CChan& Chan, CClient& Client)
    {
        if (Client.IsCapEnabled(SmartPlaybackCap))
            return HALTCORE;
        return CONTINUE;
    }

    virtual EModRet OnChanBufferPlayLine(CChan& Chan, CClient& Client, CString& sLine)
    {
        if (Client.IsCapEnabled(SmartPlaybackCap))
            return HALTCORE;
        return CONTINUE;
    }

    virtual EModRet OnChanBufferEnding(CChan& Chan, CClient& Client)
    {
        if (Client.IsCapEnabled(SmartPlaybackCap))
            return HALTCORE;
        return CONTINUE;
    }

    virtual EModRet OnUserRaw(CString& sLine)
    {
        CClient* pClient = GetClient();
        CIRCNetwork* pNetwork = GetNetwork();
        if (pClient && pNetwork && sLine.Token(0).Equals("PLAYBACK")) {
            // PLAYBACK <#chan(s)> [timestamp]
            VCString vsArgs;
            sLine.Token(1).Split(",", vsArgs, false);

            std::vector<CChan*> vChans;
            for (VCString::const_iterator it = vsArgs.begin(); it != vsArgs.end(); ++it) {
                std::vector<CChan*> vFound = FindChans(pNetwork, *it);
                vChans.insert(vChans.end(), vFound.begin(), vFound.end());
            }

            if (!vChans.empty()) {
                time_t from = sLine.Token(2).ToLong();
                for (std::vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it)
                    SendBuffer(*it, pClient, from);
            }
            return HALTCORE;
        }
        return CONTINUE;
    }

    virtual EModRet OnSendToClient(CClient* pClient, CString& sLine)
    {
        if (pClient && pClient->IsAttached() && pClient->IsCapEnabled(SmartPlaybackCap)) {
            timeval tv;
            if (gettimeofday(&tv, NULL) == -1) {
                tv.tv_sec = time(NULL);
                tv.tv_usec = 0;
            }
            MCString mssTags = GetMessageTags(sLine);
            mssTags["time"] = FormatServerTime(tv);
            SetMessageTags(sLine, mssTags);
        }
        return CONTINUE;
    }

private:
    static void SendBuffer(CChan* pChan, CClient* pClient, time_t from = 0, time_t to = -1)
    {
        assert(pChan);
        assert(pClient);

        VCString vsLines;
        const CBuffer& Buffer = pChan->GetBuffer();
        for (size_t uIdx = 0; uIdx < Buffer.Size(); ++uIdx) {
            const CBufLine& BufLine = Buffer.GetBufLine(uIdx);
            time_t stamp = BufLine.GetTime().tv_sec;
            if (stamp >= from && (to < 0 || stamp <= to))
                vsLines.push_back(BufLine.GetLine(*pClient, MCString::EmptyMap));
        }

        if (!vsLines.empty()) {
            pClient->PutClient(":***!znc@znc.in PRIVMSG " + pChan->GetName() + " :Buffer Playback...");
            for (size_t uIdx = 0; uIdx < vsLines.size(); ++uIdx)
                pClient->PutClient(vsLines.at(uIdx));
            pClient->PutClient(":***!znc@znc.in PRIVMSG " + pChan->GetName() + " :Playback Complete.");
        }
    }

    static std::vector<CChan*> FindChans(const CIRCNetwork* pNetwork, const CString& sWild)
    {
        assert(pNetwork);

        std::vector<CChan*> vChans;
        const std::vector<CChan*>& vAllChans = pNetwork->GetChans();
        for (std::vector<CChan*>::const_iterator it = vAllChans.begin(); it != vAllChans.end(); ++it) {
            if ((*it)->GetName().WildCmp(sWild))
                vChans.push_back(*it);
        }
        return vChans;
    }

    static MCString GetMessageTags(const CString& sLine)
    {
        if (sLine.StartsWith("@")) {
            VCString vsTags;
            sLine.Token(0).TrimPrefix_n("@").Split(";", vsTags, false);

            MCString mssTags;
            for (VCString::const_iterator it = vsTags.begin(); it != vsTags.end(); ++it)
                mssTags[(*it).Token(0, false, "=")] = (*it).Token(1, false, "=");
            return mssTags;
        }
        return MCString::EmptyMap;
    }

    static void SetMessageTags(CString& sLine, const MCString& mssTags)
    {
        if (sLine.StartsWith("@"))
            sLine.LeftChomp(sLine.Token(0).length() + 1);

        if (!mssTags.empty()) {
            CString sTags = "@";
            for (MCString::const_iterator it = mssTags.begin(); it != mssTags.end(); ++it) {
                if (sTags.size() > 1)
                    sTags += ";";
                sTags += it->first + "=" + it->second;
            }
            sLine = sTags + " " + sLine;
        }
    }

    static CString FormatServerTime(const timeval& tv) {
        CString s_msec(tv.tv_usec / 1000);
        while (s_msec.length() < 3) {
            s_msec = "0" + s_msec;
        }
        // TODO support leap seconds properly
        // TODO support message-tags properly
        struct tm stm;
        memset(&stm, 0, sizeof(stm));
        const time_t secs = tv.tv_sec; // OpenBSD has tv_sec as int, so explicitly convert it to time_t to make gmtime_r() happy
        gmtime_r(&secs, &stm);
        char sTime[20] = {};
        strftime(sTime, sizeof(sTime), "%Y-%m-%dT%H:%M:%S", &stm);
        return CString(sTime) + "." + s_msec + "Z";
    }
};

GLOBALMODULEDEFS(CSmartPlaybackMod, "A smart playback module for ZNC")
