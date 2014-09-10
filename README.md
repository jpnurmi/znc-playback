An advanced playback module for ZNC
===================================

### Overview

The advanced playback module for ZNC makes it possible for IRC clients
to avoid undesired repetitive buffer playback. IRC clients may request
the module to send a partial buffer playback starting from a certain
point of time.

### Persistent buffers

The module has been designed to be used together with persistent buffers.
Either uncheck *Auto Clear Chan Buffer* and *Auto Clear Query Buffer*
(Your Settings -> Flags) via webadmin, disable `AutoClearChanBuffer` and
`AutoClearQueryBuffer` via \*controlpanel, or answer no to the following
`znc --makeconf` questions:

    Would you like to clear channel buffers after replay? (yes/no): no
    Would you like to clear query buffers after replay? (yes/no): no

### Message tags

The module implementation is based on the IRC v3.2 message tags [1] and
server time [2] extensions. The module injects a server time tag to each
message sent from ZNC to an IRC client that has requested the
`znc.in/playback` capability.

- [1] http://ircv3.org/specification/message-tags-3.2
- [2] http://ircv3.org/extensions/server-time-3.2

### Usage

In order for an IRC client to support advanced playback, it should request
the `znc.in/playback` capability and keep track of the latest server time
of received messages. The syntax for a buffer playback request is:

    /msg *playback PLAY <#chan(s)> [<timestamp>]

Where the first argument is a comma-separated list of channels (supports
wildcards), and an optional second argument is number of seconds elapsed
since _January 1, 1970_.

When the IRC client connects to ZNC, it should request the module to send
a buffer playback for all channels, starting from *0* (first connect) or
the the timestamp of the latest received message (consecutive reconnects).

    /msg *playback PLAY * 0

It is also possible to selectively clear playback buffers using the
following command syntax:

    /msg *playback CLEAR <#chan(s)>

Where the command argument is a comma-separated list of channels (also
supports wildcards). 

See also http://wiki.znc.in/Query_buffers.

### Supported IRC clients

* [Textual](http://textualapp.com) IRC client for Mac OS X
* [Communi](https://github.com/communi/communi-desktop) for Mac OS X, Linux and Windows
* [Communi for SailfishOS](https://github.com/communi/communi-sailfish)
* WIP: [HexChat](http://hexchat.github.io) (https://github.com/hexchat/hexchat/issues/1109)

### Contact

Got questions? Contact jpnurmi@gmail.com or *jpnurmi* on Freenode.
