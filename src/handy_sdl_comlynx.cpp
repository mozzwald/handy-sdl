//
// Handy/SDL - ComLynx over TCP
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __GCCWIN32__
	#include <winsock2.h>
	#include <ws2tcpip.h>
	#define CLOSESOCKET(s)	closesocket(s)
	#define SOCKWOULDBLOCK	(WSAGetLastError()==WSAEWOULDBLOCK)
	#define SOCKINPROGRESS	(WSAGetLastError()==WSAEWOULDBLOCK)
	typedef int socklen_t;
#else
	#include <unistd.h>
	#include <fcntl.h>
	#include <netdb.h>
	#include <sys/types.h>
	#include <sys/select.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <netinet/tcp.h>
	#define CLOSESOCKET(s)	close(s)
	#define SOCKWOULDBLOCK	(errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR)
	#define SOCKINPROGRESS	(errno==EINPROGRESS || errno==EINTR)
	#define INVALID_SOCKET	(-1)
	typedef int SOCKET;
#endif

// Note for Win32: winsock2.h above must precede SDL.h, which pulls in
// windows.h and would otherwise drag in the older winsock.h.
#include "SDL.h"

#include "handy_sdl_main.h"
#include "handy_sdl_comlynx.h"

#define COMLYNX_MAX_PEERS	4
#define COMLYNX_TXBUF		4096

// How long comlynx_start_connect() waits for the far end to complete the TCP
// handshake before giving up.
#define COMLYNX_CONNECT_TIMEOUT_MS	500

#define MODE_OFF		HANDY_COMLYNX_OFF
#define MODE_LISTEN		HANDY_COMLYNX_LISTEN
#define MODE_CONNECT	HANDY_COMLYNX_CONNECT

// What the link is doing right now.
static int		mode = MODE_OFF;

// What the GUI and the command line asked for. Kept separately so the fields
// stay filled in while the link is stopped. Defaults to OFF: the link must be
// asked for, never opened just because the emulator started.
static int		cfg_mode = MODE_OFF;
static char		peerhost[256] = "127.0.0.1";
static int		peerport = 8100;

// Set only by -comlynx. The stored settings also fill cfg_mode in, so that
// alone cannot mean "bring the link up" - otherwise merely having used
// ComLynx once would reopen the socket on every later run.
static int		cli_requested = 0;

// Throughput accounting, sampled once a second for the status display.
static int		rx_bytes = 0, tx_bytes = 0;
static int		rx_rate  = 0, tx_rate  = 0;
static Uint32	rate_mark = 0;

#ifdef __GCCWIN32__
static int		winsock_up = 0;
#endif

static SOCKET	listen_sock = INVALID_SOCKET;
static SOCKET	peers[COMLYNX_MAX_PEERS];
static int		peer_count = 0;

// Bytes the cart has transmitted, waiting to go out to the peers. Held per
// peer index so a slow peer cannot stall the others.
static unsigned char txbuf[COMLYNX_MAX_PEERS][COMLYNX_TXBUF];
static int		txlen[COMLYNX_MAX_PEERS];

static int		cable_asserted = 0;
static int		trace = 0;

// Mikey needs to see the trace flag too, so the UART register traffic can be
// logged alongside the wire bytes. Non-static for that reason.
int				gComLynxTrace = 0;
int				gComLynxRxGate = 1;


static void comlynx_set_nonblocking(SOCKET s)
{
#ifdef __GCCWIN32__
	u_long nb = 1;
	ioctlsocket(s, FIONBIO, &nb);
#else
	int fl = fcntl(s, F_GETFL, 0);
	if(fl!=-1) fcntl(s, F_SETFL, fl|O_NONBLOCK);
#endif
	// ComLynx traffic is small and latency sensitive, so don't let Nagle sit
	// on single bytes waiting for more.
	int one = 1;
	setsockopt(s, IPPROTO_TCP, TCP_NODELAY, (const char *)&one, sizeof(one));
}

static void comlynx_drop_peer(int idx)
{
	if(peers[idx]==INVALID_SOCKET) return;
	CLOSESOCKET(peers[idx]);
	peers[idx] = INVALID_SOCKET;
	txlen[idx] = 0;
	peer_count--;
	printf("ComLynx: peer disconnected (%d connected)\n", peer_count);
}

static void comlynx_add_peer(SOCKET s)
{
	for(int i=0;i<COMLYNX_MAX_PEERS;i++)
	{
		if(peers[i]==INVALID_SOCKET)
		{
			comlynx_set_nonblocking(s);
			peers[i] = s;
			txlen[i]  = 0;
			peer_count++;
			printf("ComLynx: peer connected (%d connected)\n", peer_count);
			return;
		}
	}
	// No room; refusing is better than silently pretending it joined the bus.
	printf("ComLynx: rejecting peer, %d already connected\n", COMLYNX_MAX_PEERS);
	CLOSESOCKET(s);
}

// Queue a byte towards every peer except "except" (-1 for all). ComLynx is a
// shared wire, so a byte from one participant is heard by all the others.
static void comlynx_queue(unsigned char byte, int except)
{
	for(int i=0;i<COMLYNX_MAX_PEERS;i++)
	{
		if(peers[i]==INVALID_SOCKET || i==except) continue;
		if(txlen[i] < COMLYNX_TXBUF)
			txbuf[i][txlen[i]++] = byte;
		else
			printf("ComLynx: transmit buffer full, dropping byte for peer %d\n", i);
	}
}

//
// Called by Mikey once per transmitted byte, from inside the emulation loop.
//
static void comlynx_tx_callback(int data, UOBJREF objref)
{
	(void)objref;

	// Mikey also fires this for a line break, which a raw byte pipe has no way
	// to represent, so drop those. Ordinary bytes now arrive carrying the
	// ninth bit in bit 8; TCP cannot express it either, so mask it off and
	// send the eight data bits. The receiving side re-derives it.
	if(data & UART_BREAK_CODE) return;
	data &= 0xff;

	// Say where the byte actually went. With no peer attached comlynx_queue()
	// has nobody to hand it to and it is simply dropped, which otherwise looks
	// identical in a log to a peer that received it and chose not to answer.
	if(trace)
	{
		if(peer_count)
			printf("ComLynx: cart -> net %02x (to %d peer%s)\n",
				data & 0xff, peer_count, peer_count==1?"":"s");
		else
			printf("ComLynx: cart -> net %02x DISCARDED, no peer connected\n", data & 0xff);
	}
	tx_bytes++;
	comlynx_queue((unsigned char)data, -1);
}

void handy_sdl_comlynx_trace(int on)
{
	trace = on;
	gComLynxTrace = on;
}

/*
	The Tx callback and the cable flag live on the CSystem object, so both are
	lost when the cartridge changes and a new one is built. Re-arm them.
*/
void handy_sdl_comlynx_reattach(void)
{
	if(mode==MODE_OFF || mpLynx==NULL) return;

	mpLynx->ComLynxTxCallback(comlynx_tx_callback, (UOBJREF)0);
	mpLynx->ComLynxCable(TRUE);
	cable_asserted = 1;
}

int handy_sdl_comlynx_parse(const char *spec)
{
	if(spec==NULL) return 0;

	if(!strncmp(spec, "listen:", 7))
	{
		peerport = atoi(spec+7);
		if(peerport<=0 || peerport>65535)
		{
			printf("ComLynx: bad port in \"%s\"\n", spec);
			return 0;
		}
		cfg_mode = MODE_LISTEN;
		cli_requested = 1;
		return 1;
	}

	if(!strncmp(spec, "connect:", 8))
	{
		const char *host = spec+8;
		const char *colon = strrchr(host, ':');
		if(colon==NULL || colon==host)
		{
			printf("ComLynx: expected connect:HOST:PORT, got \"%s\"\n", spec);
			return 0;
		}
		size_t hlen = (size_t)(colon-host);
		if(hlen >= sizeof(peerhost))
		{
			printf("ComLynx: host name too long in \"%s\"\n", spec);
			return 0;
		}
		memcpy(peerhost, host, hlen);
		peerhost[hlen] = '\0';
		peerport = atoi(colon+1);
		if(peerport<=0 || peerport>65535)
		{
			printf("ComLynx: bad port in \"%s\"\n", spec);
			return 0;
		}
		cfg_mode = MODE_CONNECT;
		cli_requested = 1;
		return 1;
	}

	printf("ComLynx: expected listen:PORT or connect:HOST:PORT, got \"%s\"\n", spec);
	return 0;
}

static int comlynx_start_listen(void)
{
	struct sockaddr_in addr;

	listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_sock==INVALID_SOCKET)
	{
		printf("ComLynx: could not create socket\n");
		return 0;
	}

	int one = 1;
	setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof(one));

	memset(&addr, 0, sizeof(addr));
	addr.sin_family      = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port        = htons((unsigned short)peerport);

	if(::bind(listen_sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		printf("ComLynx: could not bind port %d\n", peerport);
		CLOSESOCKET(listen_sock);
		listen_sock = INVALID_SOCKET;
		return 0;
	}
	if(listen(listen_sock, COMLYNX_MAX_PEERS) < 0)
	{
		printf("ComLynx: could not listen on port %d\n", peerport);
		CLOSESOCKET(listen_sock);
		listen_sock = INVALID_SOCKET;
		return 0;
	}

	comlynx_set_nonblocking(listen_sock);
	printf("ComLynx: listening on port %d\n", peerport);
	return 1;
}

static int comlynx_start_connect(void)
{
	struct addrinfo hints, *res = NULL;
	char portstr[16];

	memset(&hints, 0, sizeof(hints));
	hints.ai_family   = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(portstr, sizeof(portstr), "%d", peerport);

	if(getaddrinfo(peerhost, portstr, &hints, &res)!=0 || res==NULL)
	{
		printf("ComLynx: could not resolve %s\n", peerhost);
		return 0;
	}

	SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if(s==INVALID_SOCKET)
	{
		printf("ComLynx: could not create socket\n");
		freeaddrinfo(res);
		return 0;
	}

	// Connect non-blocking. A blocking connect() here stalls whatever called us
	// for the full TCP timeout when the far end is listening but not answering
	// - and at startup that is the emulator itself, video, audio and all, so a
	// missing bridge used to mean a frozen window and no sound.
	comlynx_set_nonblocking(s);

	if(::connect(s, res->ai_addr, (socklen_t)res->ai_addrlen) < 0)
	{
		if(!SOCKINPROGRESS)
		{
			printf("ComLynx: could not connect to %s:%d\n", peerhost, peerport);
			CLOSESOCKET(s);
			freeaddrinfo(res);
			return 0;
		}

		// Give the handshake a moment to finish, but never longer than this:
		// the link is a convenience, and waiting on it is not worth holding up
		// the emulator. A bridge that comes up later can be connected from the
		// GUI.
		fd_set wr, ex;
		struct timeval tv;

		FD_ZERO(&wr); FD_SET(s, &wr);
		FD_ZERO(&ex); FD_SET(s, &ex);
		tv.tv_sec  = 0;
		tv.tv_usec = COMLYNX_CONNECT_TIMEOUT_MS * 1000;

		int sel = select((int)s+1, NULL, &wr, &ex, &tv);
		if(sel <= 0)
		{
			printf("ComLynx: no answer from %s:%d within %dms\n",
			       peerhost, peerport, COMLYNX_CONNECT_TIMEOUT_MS);
			CLOSESOCKET(s);
			freeaddrinfo(res);
			return 0;
		}

		// Writable does not mean connected: a refused connection reports ready
		// too, with the reason parked in SO_ERROR.
		int err = 0;
		socklen_t errlen = sizeof(err);
		if(getsockopt(s, SOL_SOCKET, SO_ERROR, (char *)&err, &errlen) < 0 || err!=0)
		{
			printf("ComLynx: could not connect to %s:%d\n", peerhost, peerport);
			CLOSESOCKET(s);
			freeaddrinfo(res);
			return 0;
		}
	}
	freeaddrinfo(res);

	printf("ComLynx: connected to %s:%d\n", peerhost, peerport);
	comlynx_add_peer(s);
	return 1;
}

/*
	Bring the link up with the given settings, replacing any existing one. The
	GUI uses this as its "Apply", so it has to be safe to call repeatedly.
*/
int handy_sdl_comlynx_start(int newmode, const char *host, int port)
{
	// Tear down whatever is running first; a half-open previous link would
	// otherwise leak its sockets.
	handy_sdl_comlynx_stop();

	if(newmode==MODE_OFF) return 0;

	if(port<=0 || port>65535)
	{
		printf("ComLynx: bad port %d\n", port);
		return 0;
	}

	cfg_mode = newmode;
	peerport = port;
	if(host && *host)
	{
		strncpy(peerhost, host, sizeof(peerhost)-1);
		peerhost[sizeof(peerhost)-1] = '\0';
	}

	for(int i=0;i<COMLYNX_MAX_PEERS;i++)
	{
		peers[i] = INVALID_SOCKET;
		txlen[i] = 0;
	}
	peer_count = 0;
	rx_bytes = tx_bytes = rx_rate = tx_rate = 0;
	rate_mark = SDL_GetTicks();

#ifdef __GCCWIN32__
	if(!winsock_up)
	{
		WSADATA wsa;
		if(WSAStartup(MAKEWORD(2,2), &wsa)!=0)
		{
			printf("ComLynx: could not initialise winsock\n");
			return 0;
		}
		winsock_up = 1;
	}
#endif

	mode = newmode;

	int ok = (mode==MODE_LISTEN) ? comlynx_start_listen() : comlynx_start_connect();
	if(!ok)
	{
		mode = MODE_OFF;
		return 0;
	}

	if(mpLynx)
	{
		mpLynx->ComLynxTxCallback(comlynx_tx_callback, (UOBJREF)0);

		// Report a cable for as long as the transport is up. Carts test the
		// NOEXP bit to decide whether a link exists at all, and an external
		// bridge is the equivalent of a permanently plugged in cable, so this
		// does not track whether a peer happens to be connected right now.
		mpLynx->ComLynxCable(TRUE);
		cable_asserted = 1;
	}

	return 1;
}

void handy_sdl_comlynx_stop(void)
{
	if(mode==MODE_OFF) return;

	if(cable_asserted && mpLynx!=NULL)
	{
		mpLynx->ComLynxTxCallback(NULL, (UOBJREF)0);
		mpLynx->ComLynxCable(FALSE);
		cable_asserted = 0;
	}

	for(int i=0;i<COMLYNX_MAX_PEERS;i++)
		if(peers[i]!=INVALID_SOCKET) comlynx_drop_peer(i);

	if(listen_sock!=INVALID_SOCKET)
	{
		CLOSESOCKET(listen_sock);
		listen_sock = INVALID_SOCKET;
	}

	mode = MODE_OFF;
	rx_rate = tx_rate = 0;
	printf("ComLynx: stopped\n");
}

int handy_sdl_comlynx_init(void)
{
	// Only -comlynx brings the link up here. Settings restored from file are
	// applied by handy_sdl_config_apply_comlynx(), which honours its own
	// autostart flag.
	if(!cli_requested || cfg_mode==MODE_OFF) return 0;
	return handy_sdl_comlynx_start(cfg_mode, peerhost, peerport);
}

void handy_sdl_comlynx_get_config(int *m, char *host, int hostlen, int *port)
{
	if(m)    *m = cfg_mode;
	if(port) *port = peerport;
	if(host && hostlen>0)
	{
		strncpy(host, peerhost, (size_t)hostlen-1);
		host[hostlen-1] = '\0';
	}
}

int handy_sdl_comlynx_cli_requested(void)
{
	return cli_requested;
}

void handy_sdl_comlynx_set_config(int m, const char *host, int port)
{
	cfg_mode = m;
	if(port>0 && port<=65535) peerport = port;
	if(host && *host)
	{
		strncpy(peerhost, host, sizeof(peerhost)-1);
		peerhost[sizeof(peerhost)-1] = '\0';
	}
}

void handy_sdl_comlynx_status(int *active, int *peers_out, int *rx, int *tx)
{
	if(active)    *active = (mode!=MODE_OFF);
	if(peers_out) *peers_out = peer_count;
	if(rx)        *rx = rx_rate;
	if(tx)        *tx = tx_rate;
}

int handy_sdl_comlynx_get_trace(void)
{
	return trace;
}

void handy_sdl_comlynx_poll(void)
{
	if(mode==MODE_OFF) return;

	// Roll the byte counters into a per-second rate for the status display.
	{
		Uint32 now = SDL_GetTicks();
		Uint32 elapsed = now - rate_mark;
		if(elapsed >= 1000)
		{
			rx_rate = (int)((float)rx_bytes * 1000.0f / (float)elapsed);
			tx_rate = (int)((float)tx_bytes * 1000.0f / (float)elapsed);
			rx_bytes = tx_bytes = 0;
			rate_mark = now;
		}
	}

	// Accept any newly arrived peers.
	if(listen_sock!=INVALID_SOCKET)
	{
		for(;;)
		{
			SOCKET s = accept(listen_sock, NULL, NULL);
			if(s==INVALID_SOCKET) break;
			comlynx_add_peer(s);
		}
	}

	// Flush queued transmit data.
	for(int i=0;i<COMLYNX_MAX_PEERS;i++)
	{
		if(peers[i]==INVALID_SOCKET || txlen[i]==0) continue;

		int sent = (int)send(peers[i], (const char *)txbuf[i], (size_t)txlen[i], 0);
		if(sent > 0)
		{
			if(sent < txlen[i])
				memmove(txbuf[i], txbuf[i]+sent, (size_t)(txlen[i]-sent));
			txlen[i] -= sent;
		}
		else if(sent < 0 && !SOCKWOULDBLOCK)
		{
			comlynx_drop_peer(i);
		}
	}

	// Receive. The Rx queue is only 32 bytes deep and ComLynxRxData() throws
	// away anything that does not fit, so never read more than it can take.
	for(int i=0;i<COMLYNX_MAX_PEERS;i++)
	{
		if(peers[i]==INVALID_SOCKET) continue;

		int space = mpLynx->ComLynxRxSpace();
		if(space<=0) break;

		unsigned char buf[UART_MAX_RX_QUEUE];
		if(space > (int)sizeof(buf)) space = (int)sizeof(buf);

		int got = (int)recv(peers[i], (char *)buf, (size_t)space, 0);
		if(got > 0)
		{
			for(int b=0;b<got;b++)
			{
				if(trace) printf("ComLynx: net -> cart %02x\n", buf[b]);
				rx_bytes++;
				mpLynx->ComLynxRxData(buf[b]);
				// Keep the rest of the bus in sync: everyone hears everyone.
				comlynx_queue(buf[b], i);
			}
		}
		else if(got==0)
		{
			comlynx_drop_peer(i);
		}
		else if(!SOCKWOULDBLOCK)
		{
			comlynx_drop_peer(i);
		}
	}
}

void handy_sdl_comlynx_close(void)
{
	handy_sdl_comlynx_stop();

#ifdef __GCCWIN32__
	if(winsock_up) { WSACleanup(); winsock_up = 0; }
#endif
}
