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
#include <znc/Query.h>
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
        AddCommand("Clear", static_cast<CModCommand::ModCmdFunc>(&CPlaybackMod::ClearCommand), "<buffer(s)>", "Clears playback for given buffers.");
        AddCommand("Play", static_cast<CModCommand::ModCmdFunc>(&CPlaybackMod::PlayCommand), "<buffer(s)> [timestamp]", "Sends playback for given buffers.");
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

    virtual EModRet OnPrivBufferPlayLine(CClient& Client, CString& sLine)
    {
        if (!m_bPlay && Client.IsCapEnabled(PlaybackCap))
            return HALTCORE;
        return CONTINUE;
    }

    void ClearCommand(const CString& sLine)
    {
        // CLEAR <buffer(s)>
        const CString sArg = sLine.Token(1);
        if (sArg.empty() || !sLine.Token(2).empty())
            return;
        std::vector<CChan*> vChans = FindChans(GetNetwork(), sArg);
        for (std::vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it)
            (*it)->ClearBuffer();
        std::vector<CQuery*> vQueries = FindQueries(GetNetwork(), sArg);
        for (std::vector<CQuery*>::const_iterator it = vQueries.begin(); it != vQueries.end(); ++it)
            (*it)->ClearBuffer();
    }

    void PlayCommand(const CString& sLine)
    {
        // PLAY <buffer(s)> [timestamp]
        const CString sArg = sLine.Token(1);
        if (sArg.empty() || !sLine.Token(3).empty())
            return;
        double timestamp = sLine.Token(2).ToDouble();
        std::vector<CChan*> vChans = FindChans(GetNetwork(), sArg);
        for (std::vector<CChan*>::const_iterator it = vChans.begin(); it != vChans.end(); ++it) {
            CBuffer Lines = GetLines((*it)->GetBuffer(), timestamp);
            m_bPlay = true;
            (*it)->SendBuffer(GetClient(), Lines);
            m_bPlay = false;
        }
        std::vector<CQuery*> vQueries = FindQueries(GetNetwork(), sArg);
        for (std::vector<CQuery*>::const_iterator it = vQueries.begin(); it != vQueries.end(); ++it) {
            CBuffer Lines = GetLines((*it)->GetBuffer(), timestamp);
            m_bPlay = true;
            (*it)->SendBuffer(GetClient(), Lines);
            m_bPlay = false;
        }
    }

    virtual EModRet OnSendToClient(CString& sLine, CClient& Client)
    {
        if (Client.IsAttached() && Client.IsCapEnabled(PlaybackCap) && !sLine.Token(0).Equals("CAP")) {
            MCString mssTags = CUtils::GetMessageTags(sLine);
            if (mssTags.find("time") == mssTags.end()) {
                // CUtils::FormatServerTime() converts to UTC
                mssTags["time"] = CUtils::FormatServerTime(LocalTime());
                CUtils::SetMessageTags(sLine, mssTags);
            }
        }
        return CONTINUE;
    }

private:
    static timeval LocalTime()
    {
        timeval tv;
        if (gettimeofday(&tv, NULL) == -1) {
            tv.tv_sec = time(NULL);
            tv.tv_usec = 0;
        }
        return tv;
    }

    static timeval UniversalTime(timeval tv = LocalTime())
    {
        tm stm;
        memset(&stm, 0, sizeof(stm));
        const time_t secs = tv.tv_sec; // OpenBSD has tv_sec as int, so explicitly convert it to time_t to make gmtime_r() happy
        gmtime_r(&secs, &stm);
        const char* tz = getenv("TZ");
        setenv("TZ", "UTC", 1);
        tzset();
        tv.tv_sec = mktime(&stm);
        if (tz)
            setenv("TZ", tz, 1);
        else
            unsetenv("TZ");
        tzset();
        return tv;
    }

    static std::vector<CChan*> FindChans(CIRCNetwork* pNetwork, const CString& sArg)
    {
        std::vector<CChan*> vChans;
        if (pNetwork) {
            VCString vsArgs;
            sArg.Split(",", vsArgs, false);

            for(std::vector<CString>::iterator it = vsArgs.begin(); it != vsArgs.end(); ++it) {
                std::vector<CChan*> vFound = pNetwork->FindChans(*it);
                vChans.insert(vChans.end(), vFound.begin(), vFound.end());
            }
        }
        return vChans;
    }

    static std::vector<CQuery*> FindQueries(CIRCNetwork* pNetwork, const CString& sArg)
    {
        std::vector<CQuery*> vQueries;
        if (pNetwork) {
            VCString vsArgs;
            sArg.Split(",", vsArgs, false);

            for(std::vector<CString>::iterator it = vsArgs.begin(); it != vsArgs.end(); ++it) {
                std::vector<CQuery*> vFound = pNetwork->FindQueries(*it);
                vQueries.insert(vQueries.end(), vFound.begin(), vFound.end());
            }
        }
        return vQueries;
    }

    static CBuffer GetLines(const CBuffer& Buffer, double timestamp)
    {
        CBuffer Lines(Buffer.Size());
        for (size_t uIdx = 0; uIdx < Buffer.Size(); uIdx++) {
            const CBufLine& Line = Buffer.GetBufLine(uIdx);
            timeval tv = UniversalTime(Line.GetTime());
            if (timestamp < tv.tv_sec + tv.tv_usec / 1000000.0)
                Lines.AddLine(Line.GetFormat(), Line.GetText(), &tv);
        }
        return Lines;
    }

    bool m_bPlay;
};

GLOBALMODULEDEFS(CPlaybackMod, "An advanced playback module for ZNC")
