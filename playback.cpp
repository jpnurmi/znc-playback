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
#include <znc/Utils.h>
#include <znc/Chan.h>
#include <znc/znc.h>
#include <sys/time.h>

static const char* PlaybackCap = "znc.in/playback";

class CPlaybackMod : public CModule
{
public:
    MODCONSTRUCTOR(CPlaybackMod)
    {
        m_bPlay = false;
        AddHelpCommand();
        AddCommand("Clear", static_cast<CModCommand::ModCmdFunc>(&CPlaybackMod::ClearCommand), "<#chan(s)>", "Clears playback buffers for given channels.");
        AddCommand("Play", static_cast<CModCommand::ModCmdFunc>(&CPlaybackMod::PlayCommand), "<#chan(s)> [timestamp]", "Sends playback buffers for given channels.");
        AddCommand("Time", static_cast<CModCommand::ModCmdFunc>(&CPlaybackMod::TimeCommand), "", "Tells the current server timestamp.");
    }

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
        if (!m_bPlay && Client.IsCapEnabled(PlaybackCap))
            return HALTCORE;
        return CONTINUE;
    }

    virtual EModRet OnChanBufferPlayLine(CChan& Chan, CClient& Client, CString& sLine)
    {
        if (!m_bPlay && Client.IsCapEnabled(PlaybackCap))
            return HALTCORE;
        return CONTINUE;
    }

    virtual EModRet OnChanBufferEnding(CChan& Chan, CClient& Client)
    {
        if (!m_bPlay && Client.IsCapEnabled(PlaybackCap))
            return HALTCORE;
        return CONTINUE;
    }

    void ClearCommand(const CString& sLine)
    {
        // CLEAR <#chan(s)>
        const CString sArg = sLine.Token(1);
        if (sArg.empty() || !sLine.Token(2).empty()) {
            PutModule("Usage: Clear <#chan(s)>");
            return;
        }
        unsigned int uMatches = 0;
        std::vector<CChan*> vChans = FindChans(GetNetwork(), sArg);
        for (std::vector<CChan*>::iterator it = vChans.begin(); it != vChans.end(); ++it) {
            (*it)->ClearBuffer();
            ++uMatches;
        }
        PutModule("The playback buffer for [" + CString(uMatches) + "] channels matching [" + sLine.Token(1) + "] has been cleared.");
    }

    void PlayCommand(const CString& sLine)
    {
        // PLAY <#chan(s)> [timestamp]
        const CString sArg = sLine.Token(1);
        if (sArg.empty() || !sLine.Token(3).empty()) {
            PutModule("Usage: Play <#chan(s)> [timestamp]");
            return;
        }
        double from = sLine.Token(2).ToDouble();
        std::vector<CChan*> vChans = FindChans(GetNetwork(), sArg);
        for (std::vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it) {
            const CBuffer& Buffer = (*it)->GetBuffer();
            CBuffer Lines(Buffer.Size());
            for (size_t uIdx = 0; uIdx < Buffer.Size(); uIdx++) {
                const CBufLine& Line = Buffer.GetBufLine(uIdx);
                timeval tv = Line.GetTime();
                if (from >= 0 && from < tv.tv_sec + tv.tv_usec / 1000000.0)
                    Lines.AddLine(Line.GetFormat(), Line.GetText(), &tv);
            }
            m_bPlay = true;
            (*it)->SendBuffer(GetClient(), Lines);
            m_bPlay = false;
        }
    }

    void TimeCommand(const CString& sLine)
    {
        timeval tv = CurrentTime();
        double timestamp = tv.tv_sec + tv.tv_usec / 1000000.0;
        PutModule("The current timestamp is [" + CString(timestamp) + "]");
    }

    virtual EModRet OnSendToClient(CString& sLine, CClient& Client)
    {
        if (Client.IsAttached() && Client.IsCapEnabled(PlaybackCap)) {
            MCString mssTags = CUtils::GetMessageTags(sLine);
            if (mssTags.find("time") == mssTags.end()) {
                mssTags["time"] = CUtils::FormatServerTime(CurrentTime());
                CUtils::SetMessageTags(sLine, mssTags);
            }
        }
        return CONTINUE;
    }

private:
    static timeval CurrentTime()
    {
        timeval tv;
        if (gettimeofday(&tv, NULL) == -1) {
            tv.tv_sec = time(NULL);
            tv.tv_usec = 0;
        }
        return tv;
    }

    static std::vector<CChan*> FindChans(CIRCNetwork* pNetwork, const CString& sArg)
    {
        std::vector<CChan*> vChans;
        if (pNetwork) {
            VCString vsArgs;
            sArg.Split(",", vsArgs, false);

            for (VCString::const_iterator it = vsArgs.begin(); it != vsArgs.end(); ++it) {
                std::vector<CChan*> vFound = pNetwork->FindChans(*it);
                vChans.insert(vChans.end(), vFound.begin(), vFound.end());
            }
        }
        return vChans;
    }

    bool m_bPlay;
};

GLOBALMODULEDEFS(CPlaybackMod, "An advanced playback module for ZNC")
