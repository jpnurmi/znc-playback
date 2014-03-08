An advanced playback module for ZNC
===================================

### Overview

The advanced playback module for ZNC makes it possible for IRC clients
to avoid undesired repetitive buffer playback. IRC clients may request
the module to send a partial buffer playback starting from a certain
point of time.

### Persistent buffers

The module has been designed to be used together with persistent channel
buffers. Either uncheck *Auto Clear Chan Buffer* (Your Settings -> Flags)
via webadmin, set disable `AutoClearChanBuffer` via \*controlpanel, or
answer no to the following `znc --makeconf` question:

    Would you like to clear channel buffers after replay? (yes/no): no

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

### Supported IRC clients

As noted in the previous section, an automatic buffer playback does not
work out of the box, but requires special support from the IRC client
wishing to use the functionality. A proof of concept implementation exists
for the Textual IRC client for Mac OS X:
https://github.com/jpnurmi/Textual/commits/playback.

### Contact

Got questions? Contact jpnurmi@gmail.com or *jpnurmi* on Freenode.
