An advanced playback module for ZNC
===================================

### Overview

This ZNC module makes it possible for IRC clients to avoid undesired
repetitive buffer playback. IRC clients may request the module to send
a partial buffer playback starting from a certain point of time.

### Persistent buffers

The module has been designed to be used together with persistent channel
buffers. Either set `AutoClearChanBuffer = false` in your ZNC config file,
or answer *no* to the following `znc --makeconf` question:

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

    /msg *playback * 0

### Work in progress

The module is still a work in progress. For the time being, the module
depends on the following unmerged ZNC pull request:

- Add module hooks for raw client and server messages (#494)
  - https://github.com/znc/znc/pull/494

Furthermore, as noted in previous section, this module does not work out
of the box, but requires special support from the IRC client wishing to
use the functionality. A proof of concept implementation exists for the
Textual IRC client for Mac OS X:
https://github.com/jpnurmi/Textual/commits/smartplayback.

### Contact

Got questions? Contact jpnurmi@gmail.com or *jpnurmi* on Freenode.
