#include <avr/interrupt.h>
#include <util/delay.h>
#include "avr_print.h"
#include "Wi07.h"
#include <stdio.h>
#include <string.h>
#include <util10.h>
#include <avr/io.h>
#include <avr/pgmspace.h>

volatile uint8_t Wi07Head = 0;
volatile uint8_t Wi07Tail = 0;
volatile uint8_t Wi07Buffer[WI07BUFFER];

//If we ran into a +IPD and can only receive..
int8_t forced_match;

/*void MatchIPDOrAT()
{
	const char * matchipd = "+IPD,";
	const char * matchother = "\r\n";
	int matchpl = 0;
	int matchplo = 0;
	int timeout = 0;

	while( timeout < WI07WAIT )
	{
		while( Wi07CanPop() || matchpl )
		{
			char c = Wi07Pop();
			if( c == matchother[matchplo] )
			{
				matchplo++;
				if( matchother[matchplo] == 0 )
				{
					//got a newline!
					return;
				}
			}
			else
				matchplo = 0;
			if( c == matchipd[matchpl] )
			{
				matchpl++;
				if( matchipd[matchpl] == 0 )
				{
					forced_match = 1;
				}
			}
			else
				matchpl = 0;
		}
		_delay_ms(1);
	}
}*/


static int8_t matchIPD()
{
	if( forced_match ) return 1;
	const char * matchipd = "+IPD,";
	char buff[6];
	int matchpl = 0;

	while( Wi07CanPop() || matchpl )
	{
		char c = Wi07Pop();
		if( c == matchipd[matchpl] )
		{
			matchpl++;
			if( matchipd[matchpl] == 0 )
			{
				forced_match = 1;
				return 1;
			}
		}
		else
			matchpl = 0;
	}
	return 0;
}

#if   defined (__AVR_ATmega32U2__) || defined( __AVR_ATmega8U2__)
#define LUDR UDR1
#elif   defined (__AVR_ATmega168__) || defined( __AVR_ATmega328__)
#define LUDR UDR0
#endif

#if   defined (__AVR_ATmega32U2__) || defined( __AVR_ATmega8U2__)
ISR( USART1_RX_vect )
#elif   defined (__AVR_ATmega168__) || defined( __AVR_ATmega328__)
ISR( USART_RX_vect  )
#endif
{
	Wi07Buffer[Wi07Head++] = LUDR;
}

uint8_t Wi07CanPop()
{
	return Wi07Head != Wi07Tail;
}

uint8_t Wi07Peek()
{
	uint8_t rc;
	while( Wi07Head == Wi07Tail );
	rc = Wi07Buffer[Wi07Tail];
	Wi07Buffer[Wi07Tail] = 0;
	return rc;
}

uint8_t Wi07Pop()
{
	uint8_t rc;
	while( Wi07Head == Wi07Tail );
	rc = Wi07Buffer[Wi07Tail];
	Wi07Buffer[Wi07Tail] = 0;
	Wi07Tail++;
	return rc;
}

int8_t Wi07Match( const char * matching, char * ret, int retmax )
{
	int retpl = 0;
	int matchpl = 0;
	int16_t timeout = 0;
	while( matching[matchpl] != 0 )
	{
		if( !Wi07CanPop() )
		{
			timeout++;
			if( timeout > WI07WAIT )
			{
				if( ret ) ret[retpl] = 0;
				 return -1;
			}
			_delay_ms(1);
			continue;
		}

		uint8_t c = Wi07Pop();
		if( matching[matchpl] == c ) matchpl++;
		else matchpl = 0;
		if( ret && retpl < retmax - 1 && matching[matchpl] != 0 ) ret[retpl++] = c;

	}
	if( ret ) ret[retpl] = 0;
	return 0;
}


int8_t InitWi07( int client, const char * ssid, const char * passphrase )
{
	uint8_t i;

#if   defined (__AVR_ATmega32U2__) || defined( __AVR_ATmega8U2__)
	DDRD |= _BV(3);
	DDRD &=~_BV(2);

	UCSR1A = _BV(U2X1); //2x speed	
	UCSR1B = _BV(RXCIE1) | _BV(RXEN1) | _BV(TXEN1);
	UCSR1C = _BV(UCSZ11) | _BV(UCSZ10);
	UBRR1 = 34; //57,600 with U2X1.  @ 16MHz
#elif   defined (__AVR_ATmega168__) || defined( __AVR_ATmega328__)
	DDRD |= _BV(1);
	DDRD &=~_BV(0);

	UCSR0A = _BV(U2X0); //2x speed	
	UCSR0B = _BV(RXCIE0) | _BV(RXEN0) | _BV(TXEN0);
	UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
	UBRR0 = 25;		//Calculate for 12 MHz
		//52; //57,600 with U2X1.  @ 20 MHz = 42

	//@??? MHz = 53
#else
#error no pins defined for yor arch.
#endif

//Attempt to flush the buffer (it failed)

	Wi07CommandPGM( PSTR("AT+RST\r\n") );

	if( Wi07Match( "ready\r\n", 0, 0 ) )
	{
		for( i = 0; i < 255; i++ )
			Wi07SC(0);

		sendstr( "Wi07: NR.\n" );
		return -1;
	}

	if( client )
	{
		sendstr( "Wi07 JOIN\n" );
		Wi07CommandPGM( PSTR("AT+CWJAP=\"") );
		Wi07Command( ssid );
		Wi07CommandPGM( PSTR("\",\"") );
		Wi07Command( passphrase );
		Wi07CommandPGM( PSTR("\"\r\n") );

		if( Wi07Match( "OK\r\n", 0, 0 ) )
		{
			sendstr( "Wi07 Could not join network.\n" )
			return -2;
		}

		char ip[18];
		//Wait for an IP. TODO XXX Improve the way this works.
		for( i = 0; i < IPWAIT; i++ )
		{	
			Wi07CommandPGM( PSTR("AT+CIFSR\r\n") );
			Wi07Match( "\n", ip, 18 );  //Pull off the echoed command.
			if( !Wi07Match( "\n", ip, 18 ) )
			{
				if( strlen( ip ) < 5 )
				{
					if( Wi07Match( "\n", ip, 18 ) ) continue;
				}
				if( strncmp( ip, "ERROR", 5 ) != 0 )
				{
					break;
				}
			}
			_delay_ms(500);
		}
		if( i == IPWAIT )
		{
			sendstr( "FAILED IP\n" );
			return -3;	
		}
		sendstr( "Got IP: ");
		sendstr( ip );
		sendstr( "\n" );
	}
	else
	{
		//TODO! (Encryption looks brok)
		sendstr( "Wi07 Creating...\n" );
		Wi07Command( "AT+CWSAP=\"" );
		Wi07Command( ssid );
		Wi07Command( "\",\"" );
		Wi07Command( passphrase );
		Wi07Command( "\",11,NONE;\r\n" );
	}

	sendstr( "Wi07 Ready.\n" );
	return 0;
}

int8_t Wi07ListenTCP( const char * port )
{
	_delay_ms(100); //I have no idea why this is needed.
	Wi07Command( "AT+CIPMUX=1;\r\n" );
	if( Wi07Match( "OK\r\n", 0, 0 ) )
		return -1;
	Wi07Command( "AT+CIPSERVER=1," );
	Wi07Command( port );
	Wi07Command( "\r\n" );
	return Wi07Match( "OK\r\n", 0, 0 );
}


void Wi07Term()
{
	while(1)
	{
		if( Wi07CanPop() )
		{
			sendchr( 0 );
			sendchr( Wi07Pop() );
		}
	}
}


//YOU Must provide this:
//void Wi07ReceiveCallback( uint8_t muxport, uint16_t datalen );


int8_t Wi07TCPSend( uint8_t muxport, void * sendbuffer, uint16_t plen );

void Wi07CheckReceive()
{
	int matchpl;
	char buff[7];

	if( !matchIPD() )
		return;

	forced_match = 0;

	//Just got +IPD,...
	//Now get:      #,#:
	Wi07Match( ",", buff, 6 );
	int8_t mux = StrTo16Uint( buff );
	Wi07Match( ":", buff, 6 );
	matchpl = StrTo16Uint( buff );
	Wi07ReceiveCallback( mux, matchpl );
	Wi07Match( "\r\n", 0, 0 );
}

void Wi07CarefulSend( uint8_t cc )
{
	int retrysend = 0;
	int retryct;
	uint8_t c;
/*
	sendchr( 0 );
	sendchr( cc );
	sendchr( '(' );
	sendhex2( cc );
	sendchr( ')' );
*/
retrysendpt:
/*	sendchr( 0 );
	sendchr( '!' );*/
	retryct = 0;
	Wi07SC( cc );
retry:
	c = Wi07Peek();
/*	sendchr( 0 );
	sendchr( '~' );
	sendhex2( c );
	sendchr( 0 );*/
	if( c == cc )
	{
		Wi07Pop();
		return;
	}
	//OTHERWISE we have an unexpected character.
	if( c == '+' ) //+ipd?
	{
		Wi07CheckReceive();
		if( retryct++ < 20 )
			goto retry;
		else if( retrysend++ < 20 )
			goto retrysendpt;
	}
	else
	{
	sendchr(0);
	sendchr('^' );
		Wi07Pop();
	sendchr( 0 );
	sendchr( '~' );
	sendhex2( c );
	sendchr( 0 );

		if( retryct++ < 20 )
			goto retry;
		else if( retrysend++ < 20 )
			goto retrysendpt;
	}
}

void CarefulSendBuffer( uint8_t * buff, int len )
{
	int i;
	for( i = 0; i < len; i++ )
	{
		Wi07CarefulSend( buff[i] );
	}
}

void CarefulSendCmd( uint8_t * buff )
{
	int i;
	while( *buff )
	{
		Wi07CarefulSend( *(buff++) );
	}
}

int8_t Wi07TCPSend( uint8_t muxport, void * sendbuffer, uint16_t plen )
{
	char strs[20];

	int i;
	if( matchIPD() )
	{
		Wi07CheckReceive();
	}

	CarefulSendCmd( "AT+CIPSEND=" );
	Uint8To10Str( strs, muxport );
	CarefulSendCmd( strs );
	Wi07CarefulSend( ',' );
	Uint32To10Str( strs, plen );
	CarefulSendCmd( strs );
	CarefulSendCmd( "\r" ); //no \n???

	//Wait for a "busy"
	//or a ">"
	//or a "+"
	int bmm = 0;
	while(1)
	{
		const char * busymatch  = "busy";
		sendchr( 0 );
		sendchr( 'W' );
		char c = Wi07Peek();
		sendchr( 0 );
		sendhex2( c );
		sendchr( '*' );

		if( c == busymatch[bmm] )
		{
			bmm++;
			if( busymatch[bmm] == 0 )
				return -1;
		}
		else
		{
			bmm = 0;
		}

		if( c == '>' ) { Wi07Pop(); goto sendgood; }
		if( c == '+' ) matchIPD();
		else	Wi07Pop();
	}

sendgood:
	CarefulSendBuffer( sendbuffer, plen );

	return 0;
}


/*
int8_t Wi07TCPSend( uint8_t muxport, uint16_t plen )
{
	char strs[20];

	//Clean out the buffer
	if( matchIPD() != 0 )
	{
		sendchr(0);
		sendstr("TCPOversend\n");
		return -1;
	}

//	sendchr(0);sendchr('!');
	Wi07Command( "AT+CIPSEND=" );
	Uint8To10Str( strs, muxport );
//	sendstr( strs );
	Wi07Command( strs );
	Wi07SC( ',' );
	Uint32To10Str( strs, plen );
//	sendchr(0);
//	sendchr(',');
//	sendstr( strs);
	Wi07Command( strs );
//	sendchr( 0 );
	Wi07Command( "\r\n" );

	MatchIPDOrAT();

	return 0;
}

/*
uint8_t Wi07Receive( )
{
	int matchpl;
	char buff[7];
	if( matchIPD() == 0 )
		return 0;

	forced_match = 0;

	//Just got +IPD,...
	//Now get:      #,#:
	Wi07Match( ",", buff, 6 );
	int8_t mux = StrTo16Uint( buff );
	Wi07Match( ":", buff, 6 );
	matchpl = StrTo16Uint( buff );
	return matchpl;
}
*/


void Wi07SC( uint8_t c )
{
//	sendchr( 0 );
//	sendchr( c );
#if   defined (__AVR_ATmega32U2__) || defined( __AVR_ATmega8U2__)
	while( ! (UCSR1A & _BV(UDRE1)) );
#elif   defined (__AVR_ATmega168__) || defined( __AVR_ATmega328__)
	while( ! (UCSR0A & _BV(UDRE0)) );
#endif
	LUDR = c;
}

void Wi07Data( uint8_t * dat, int len )
{
	int i;
	for( i = 0; i < len; i++ )
	{
		Wi07SC( dat[i] );
	}
}

void Wi07CommandPGM( const char * cmd )
{
	uint8_t c;
	while( c = pgm_read_byte(cmd++) )
	{
		Wi07SC( c );
	}
}

void Wi07Command( const char * cmd )
{
	uint8_t c;
	while( c = *(cmd++) )
	{
		Wi07SC( c );
	}
}



