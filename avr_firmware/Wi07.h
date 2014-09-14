#ifndef _WI07_H
#define _WI07_H

#define WI07BUFFER 256
#define WI07WAIT 2500
#define IPWAIT 20



int8_t InitWi07( int client, const char * ssid, const char * passphrase );
int8_t Wi07ListenTCP( const char * port ); //Yick, I know terrible, but works out better for me to be a human-readable string.

//Only call this from your "main" thread, as this will callback your Receive callback
//at ANY TIME if new TCP data comes in.
int8_t Wi07TCPSend( uint8_t muxport, void * sendbuffer, uint16_t plen );

//Check
void Wi07CheckReceive();

//YOU Must provide this:
void Wi07ReceiveCallback( uint8_t muxport, uint16_t datalen );

//Generally for internal use:

//Does not add '\n''s.
void Wi07Command( const char * cmd );
void Wi07CommandPGM( const char * cmd );
void Wi07Data( uint8_t * dat, int len );  //for raw sending
void Wi07SC( uint8_t c );  //send a single byte of data.

void Wi07Term(); //Go into terminal mode, do not exit.

//Match incoming data with string, wait for taht string to come along, or WI07WAIT (in ms) it hit.
//If ret is nonzero, all response will be logged to ret.
int8_t Wi07Match( const char * matching, char * ret, int retmax );

uint8_t Wi07Pop();
uint8_t Wi07Peek();
uint8_t Wi07CanPop();

#endif

