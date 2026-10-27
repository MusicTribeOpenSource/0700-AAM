# ComProtocol

`PktMgr` is the framed packet protocol (header, type, payload and CRC) used between the Kick application
and the device hardware over USB. It is a small C++ class with no external dependencies.

`juce/PktMgr` registers the released packet implementation as a JUCE module.
KICK's `modules/serial_port` supplies the POSIX transport and a bounded streaming
decoder for fragmented/coalesced packets. Tests compare its wire output against
`CPktMgr` and exercise a native pseudo-terminal. Physical board I/O is unverified.

## License

GNU Affero General Public License v3.0 or later, see [LICENSE](LICENSE).
Copyright (c) MUSIC Tribe.
