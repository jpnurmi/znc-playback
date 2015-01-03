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
static const char* EchoMessageCap = "znc.in/echo-message";

class CPlaybackMod : public CModule
{
public:
    MODCONSTRUCTOR(CPlaybackMod)
    {
        m_play = false;
        AddHelpCommand();
        AddCommand("Clear", static_cast<CModCommand::ModCmdFunc>(&CPlaybackMod::ClearCommand), "<buffer(s)>", "Clears playback for given buffers.");
        AddCommand("Play", static_cast<CModCommand::ModCmdFunc>(&CPlaybackMod::PlayCommand), "<buffer(s)> [timestamp]", "Sends playback for given buffers.");
    }

    void OnClientCapLs(CClient* client, SCString& caps) override
    {
        caps.insert(PlaybackCap);
        caps.insert(EchoMessageCap);
    }

    bool IsClientCapSupported(CClient* client, const CString& cap, bool state) override
    {
        return cap.Equals(PlaybackCap) || cap.Equals(EchoMessageCap);
    }

    EModRet OnChanBufferStarting(CChan& chan, CClient& client) override
    {
        if (!m_play && client.IsCapEnabled(PlaybackCap))
            return HALTCORE;
        return CONTINUE;
    }

    EModRet OnChanBufferPlayLine(CChan& chan, CClient& client, CString& line) override
    {
        if (!m_play && client.IsCapEnabled(PlaybackCap))
            return HALTCORE;
        return CONTINUE;
    }

    EModRet OnChanBufferEnding(CChan& chan, CClient& client) override
    {
        if (!m_play && client.IsCapEnabled(PlaybackCap))
            return HALTCORE;
        return CONTINUE;
    }

    EModRet OnPrivBufferPlayLine(CClient& client, CString& line) override
    {
        if (!m_play && client.IsCapEnabled(PlaybackCap))
            return HALTCORE;
        return CONTINUE;
    }

    void ClearCommand(const CString& line)
    {
        // CLEAR <buffer(s)>
        const CString arg = line.Token(1);
        if (arg.empty() || !line.Token(2).empty())
            return;
        std::vector<CChan*> chans = FindChans(GetNetwork(), arg);
        for (CChan* chan : chans)
            chan->ClearBuffer();
        std::vector<CQuery*> queries = FindQueries(GetNetwork(), arg);
        for (CQuery* query : queries)
            query->ClearBuffer();
    }

    void PlayCommand(const CString& line)
    {
        // PLAY <buffer(s)> [timestamp]
        const CString arg = line.Token(1);
        if (arg.empty() || !line.Token(3).empty())
            return;
        double timestamp = line.Token(2).ToDouble();
        std::vector<CChan*> chans = FindChans(GetNetwork(), arg);
        for (CChan* chan : chans) {
            if (chan->IsOn() && !chan->IsDetached()) {
                CBuffer lines = GetLines(chan->GetBuffer(), timestamp);
                m_play = true;
                chan->SendBuffer(GetClient(), lines);
                m_play = false;
            }
        }
        std::vector<CQuery*> queries = FindQueries(GetNetwork(), arg);
        for (CQuery* query : queries) {
            CBuffer lines = GetLines(query->GetBuffer(), timestamp);
            m_play = true;
            query->SendBuffer(GetClient(), lines);
            m_play = false;
        }
    }

    EModRet OnSendToClient(CString& line, CClient& client) override
    {
        if (client.IsAttached() && client.IsCapEnabled(PlaybackCap) && !line.Token(0).Equals("CAP")) {
            MCString tags = CUtils::GetMessageTags(line);
            if (tags.find("time") == tags.end()) {
                // CUtils::FormatServerTime() converts to UTC
                tags["time"] = CUtils::FormatServerTime(LocalTime());
                CUtils::SetMessageTags(line, tags);
            }
        }
        return CONTINUE;
    }

    EModRet OnUserMsg(CString& target, CString& message) override
    {
        return EchoMessage("PRIVMSG " + target + " :" + message);
    }

    EModRet OnUserNotice(CString& target, CString& message) override
    {
        return EchoMessage("NOTICE " + target + " :" + message);
    }

    EModRet OnUserAction(CString& target, CString& message) override
    {
        return EchoMessage("PRIVMSG " + target + " :\001ACTION " + message + "\001");
    }

private:
    EModRet EchoMessage(CString message)
    {
        CClient* client = GetClient();
        if (client && client->IsCapEnabled(EchoMessageCap)) {
            message.insert(0, ":" + client->GetNickMask() + " ");
            if (client->HasServerTime()) {
                MCString tags = CUtils::GetMessageTags(message);
                tags["time"] = CUtils::FormatServerTime(LocalTime());
                CUtils::SetMessageTags(message, tags);
            }
            client->PutClient(message);
        }
        return CONTINUE;
    }

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

    static std::vector<CChan*> FindChans(CIRCNetwork* network, const CString& arg)
    {
        std::vector<CChan*> chans;
        if (network) {
            VCString vargs;
            arg.Split(",", vargs, false);

            for (const CString& name : vargs) {
                std::vector<CChan*> found = network->FindChans(name);
                chans.insert(chans.end(), found.begin(), found.end());
            }
        }
        return chans;
    }

    static std::vector<CQuery*> FindQueries(CIRCNetwork* network, const CString& arg)
    {
        std::vector<CQuery*> queries;
        if (network) {
            VCString vargs;
            arg.Split(",", vargs, false);

            for (const CString& name : vargs) {
                std::vector<CQuery*> found = network->FindQueries(name);
                queries.insert(queries.end(), found.begin(), found.end());
            }
        }
        return queries;
    }

    static CBuffer GetLines(const CBuffer& buffer, double timestamp)
    {
        CBuffer lines(buffer.Size());
        for (size_t i = 0; i < buffer.Size(); ++i) {
            const CBufLine& line = buffer.GetBufLine(i);
            timeval tv = UniversalTime(line.GetTime());
            if (timestamp < tv.tv_sec + tv.tv_usec / 1000000.0)
                lines.AddLine(line.GetFormat(), line.GetText(), &tv);
        }
        return lines;
    }

    bool m_play;
};

GLOBALMODULEDEFS(CPlaybackMod, "An advanced playback module for ZNC")
