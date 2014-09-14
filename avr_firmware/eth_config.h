//Copyright 2012 Charles Lohr under the MIT/x11, newBSD, LGPL or GPL licenses.  You choose.

#ifndef _ETH_CONFIG_H
#define _ETH_CONFIG_H

//Dummy file (we're not using my TCP/IP stack)

//Do this to disable printf's and save space
#define MUTE_PRINTF

#ifndef ASSEMBLY

extern unsigned char MyIP[4];
extern unsigned char MyMask[4];
extern unsigned char MyMAC[6];

#endif

#endif

