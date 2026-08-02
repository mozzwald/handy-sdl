//
// Handy/SDL - ComLynx over TCP
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#ifndef HANDY_SDL_COMLYNX_H
#define HANDY_SDL_COMLYNX_H

//
// Bridges the emulated Mikey UART (ComLynx) to a TCP socket, as a raw
// bidirectional byte pipe with no framing of any kind. Whatever the cart
// transmits appears on the socket and vice versa.
//
// Two modes:
//
//   listen:PORT          accept peers on PORT (default; this is what an
//                        external bridge such as a FujiNet BoIP channel
//                        connects to)
//   connect:HOST:PORT    dial out to a listening peer
//
// ComLynx is a multi-drop bus rather than a point to point link, so in
// listen mode every byte is delivered to all participants except the one
// that sent it: bytes from the cart go to all peers, and bytes from a peer
// go to the cart and to all the other peers.
//

// Parse a mode string as described above. Returns 1 on success, 0 on a
// malformed spec (a message is printed).
int  handy_sdl_comlynx_parse(const char *spec);

// Log every byte crossing the link in both directions. Useful when bringing
// up whatever sits on the other end.
void handy_sdl_comlynx_trace(int on);

// Re-arm the Tx callback and cable flag after the emulation object has been
// replaced, as happens when a new cartridge is loaded.
void handy_sdl_comlynx_reattach(void);

// Bring up the socket and attach to the emulated UART. Safe to call when no
// spec was given, in which case it does nothing. Returns 1 if enabled.
int  handy_sdl_comlynx_init(void);

// Pump the socket. Must be called from the emulation loop often enough to
// keep up with the UART: the Rx queue only holds 32 bytes and overruns are
// discarded silently, so this needs to run inside the instruction batch
// rather than once per frame.
void handy_sdl_comlynx_poll(void);

// Drop all peers and release the listening socket.
void handy_sdl_comlynx_close(void);

#endif
