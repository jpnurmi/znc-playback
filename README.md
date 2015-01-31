An advanced playback module for ZNC
===================================

### Overview

The advanced playback module for ZNC makes it possible for IRC clients
to avoid undesired repetitive buffer playback. IRC clients may request
the module to send a partial buffer playback starting from and ending
to a certain point of time.

### Persistent buffers

The module has been designed to be used together with persistent buffers.
Either uncheck *Auto Clear Chan Buffer* and *Auto Clear Query Buffer*
(Your Settings -> Flags) via webadmin or disable `AutoClearChanBuffer` and
`AutoClearQueryBuffer` via \*controlpanel.

### Message tags

The module implementation is based on the IRC v3.2 message tags [1] and
server time [2] extensions. The module injects a server time tag (in
millisecond precision) to each message sent from ZNC to an IRC client
that has requested the `znc.in/playback` capability.

- [1] http://ircv3.org/specification/message-tags-3.2
- [2] http://ircv3.org/extensions/server-time-3.2

### Usage

In order for an IRC client to support advanced playback, it should request
the `znc.in/playback` capability and keep track of the latest server time
of received messages. The syntax for a buffer playback request is:

    /msg *playback PLAY <buffer(s)> [from] [to]

Where the first argument is a comma-separated list of channels (supports
wildcards), and optional last two arguments are number of seconds (floating
point for millisecond precision) elapsed since _January 1, 1970_.

When a client connects to ZNC, it may request the module to play all
buffers, starting from *0* (first connect) or the the timestamp of the
latest received message (consecutive reconnects).

    /msg *playback PLAY * 0

It is also possible to selectively clear playback buffers using the
following command syntax:

    /msg *playback CLEAR <buffer(s)>

If a client wants to list available buffers, it can use the following
command syntax:

    /msg *playback LIST [buffer(s)]

The module outputs a list of available/matching buffers, one per line,
each followed by the first and last timestamp for the respective buffer.

See also http://wiki.znc.in/Query_buffers and http://wiki.znc.in/Playback.

### Limits

The module also allows configuring client specific buffer limits for
identified clients, using the following command syntax:

    /msg *playback LIMIT <client> [limit]

A limit of less or equal to 0 clears the limit. Notice that the limit
cannot exceed the limit set in the ZNC configuration, but can be used
to limit the playback for eg. mobile clients.

### Client identifiers

ZNC supports passing a client identifier in the password:

    username@identifier/network:password

or in the username:

    username@identifier/network

### Contact

Got questions? Contact jpnurmi@gmail.com or *jpnurmi* on Freenode.
