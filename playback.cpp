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

static const char* PlaybackCap = "znc.in/playback";

class CPlaybackMod : public CModule
{
public:
    MODCONSTRUCTOR(CPlaybackMod) { }

    virtual void OnClientCapLs(CClient* pClient, SCString& ssCaps)
    {
        ssCaps.insert(PlaybackCap);
    }

    virtual bool IsClientCapSupported(CClient* pClient, const CString& sCap, bool bState)
    {
        return sCap.Equals(PlaybackCap);
    }

    virtual EModRet OnChanBufferStarting(CChan& Chan, CClient& Client)
    {
        if (Client.IsCapEnabled(PlaybackCap))
            return HALTCORE;
        return CONTINUE;
    }

    virtual EModRet OnChanBufferPlayLine(CChan& Chan, CClient& Client, CString& sLine)
    {
        if (Client.IsCapEnabled(PlaybackCap))
            return HALTCORE;
        return CONTINUE;
    }

    virtual EModRet OnChanBufferEnding(CChan& Chan, CClient& Client)
    {
        if (Client.IsCapEnabled(PlaybackCap))
            return HALTCORE;
        return CONTINUE;
    }

    virtual void OnModCommand(const CString& sLine)
    {
        const CString sCommand = sLine.Token(0);
        if (sCommand.Equals("HELP")) {
            ShowHelp();
        } else if (sCommand.Equals("PLAY")) {
            // PLAY <#chan(s)> [timestamp]
            const CString sArg = sLine.Token(1);
            if (sArg.empty() || !sLine.Token(3).empty()) {
                PutModule("Usage: Play <#chan(s)> [timestamp]");
                return;
            }
            std::vector<CChan*> vChans = GetChans(GetNetwork(), sArg);
            time_t from = sLine.Token(2).ToLong();
            for (std::vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it)
                SendBuffer(*it, GetClient(), from);
        } else if (sCommand.Equals("CLEAR")) {
            // CLEAR <#chan(s)>
            const CString sArg = sLine.Token(1);
            if (sArg.empty() || !sLine.Token(2).empty()) {
                PutModule("Usage: Clear <#chan(s)>");
                return;
            }
            unsigned int uMatches = 0;
            std::vector<CChan*> vChans = GetChans(GetNetwork(), sArg);
            for (std::vector<CChan*>::iterator it = vChans.begin(); it != vChans.end(); ++it) {
                (*it)->ClearBuffer();
                ++uMatches;
            }
            PutModule("The playback buffer for [" + CString(uMatches) + "] channels matching [" + sLine.Token(1) + "] has been cleared.");
        }
    }

    // #494: Add module hooks for raw client and server messages
    // https://github.com/znc/znc/pull/494
    virtual EModRet OnSendToClient(CClient* pClient, CString& sLine)
    {
        if (pClient && pClient->IsAttached() && pClient->IsCapEnabled(PlaybackCap)) {
            MCString mssTags = GetMessageTags(sLine);
            if (mssTags.find("time") == mssTags.end()) {
                timeval tv;
                if (gettimeofday(&tv, NULL) == -1) {
                    tv.tv_sec = time(NULL);
                    tv.tv_usec = 0;
                }
                mssTags["time"] = FormatServerTime(tv);
                SetMessageTags(sLine, mssTags);
            }
        }
        return CONTINUE;
    }

private:
    void ShowHelp()
    {
        PutModule("Available commands:");
        CTable Table;
        Table.AddColumn("Command");
        Table.AddColumn("Description");
        Table.AddRow();
        Table.SetCell("Command", "Clear <#chan(s)>");
        Table.SetCell("Description", "Clear playback buffers for given channels.");
        Table.AddRow();
        Table.SetCell("Command", "Play <#chan(s)> [timestamp]");
        Table.SetCell("Description", "Send playback buffers for given channels.");
        PutModule(Table);
        PutModule("Command arguments:");
        Table.Clear();
        Table.AddColumn("Argument");
        Table.AddColumn("Description");
        Table.AddRow();
        Table.SetCell("Argument", "#chan(s)");
        Table.SetCell("Description", "A comma-separated list of channels (supports wildcards).");
        Table.AddRow();
        Table.SetCell("Argument", "timestamp");
        Table.SetCell("Description", "The number of seconds elapsed since January 1, 1970.");
        PutModule(Table);
    }

    std::vector<CChan*> GetChans(CIRCNetwork* pNetwork, const CString& sArg)
    {
        std::vector<CChan*> vChans;
        if (pNetwork) {
            VCString vsArgs;
            sArg.Split(",", vsArgs, false);

            for (VCString::const_iterator it = vsArgs.begin(); it != vsArgs.end(); ++it) {
                std::vector<CChan*> vFound = FindChans(pNetwork, *it);
                vChans.insert(vChans.end(), vFound.begin(), vFound.end());
            }
        }
        return vChans;
    }

    // #502: CChan::SendBuffer(): allow specifying a time range
    // https://github.com/znc/znc/pull/502
    static void SendBuffer(CChan* pChan, CClient* pClient, time_t from = 0, time_t to = -1)
    {
        if (pChan && pClient) {
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
    }

    // #500: Add CIRCNetwork::FindChans()
    // https://github.com/znc/znc/pull/500
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

    // #501: Add CUtils::Get/SetMessageTags()
    // https://github.com/znc/znc/pull/501
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

    // #501: Add CUtils::Get/SetMessageTags()
    // https://github.com/znc/znc/pull/501
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

    // #493: Promote server-time formatting to Utils
    // https://github.com/znc/znc/pull/493
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

GLOBALMODULEDEFS(CPlaybackMod, "An advanced playback module for ZNC")
