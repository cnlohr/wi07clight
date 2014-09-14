//Copyright 2012 <>< Charles Lohr, under the MIT-x11 or NewBSD license.  You choose.

//Err this is a total spaghetti code file.  My interest is in the libraries, not this.

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "avr_print.h"
#include <stdio.h>
#include <avr/pgmspace.h>

#include <string.h>
#include <avr/eeprom.h> 
#include <util10.h>
#include "Wi07.h"

#include "wifi_network.h"

int player_timeout = 300;

uint8_t  EEMEM my_server_name_eeprom[16]; 
char my_server_name[16];

char sendbuffer[350];
uint16_t sendpl = 0;

#define NOOP asm volatile("nop" ::)

uint8_t CanSend( uint8_t playerno ) //DUMBCRAFT
{
	return sendpl < 244; //No pending messages?
}

void SendStart( uint8_t playerno ) //Called by dumbcraft
{
	if( !CanSend( playerno ) )
	{
		sendchr(0);
		sendstr( "!!!! NO ABLE SEND\n");
	}
//	mark_for_send = 0;
	sendpl = 0;
}

void extSbyte( uint8_t byte )  //Called by dumbcraft.
{
//	sendchr(0);sendchr('*');
	sendbuffer[sendpl++] = byte;
}

void EndSend( )  //DUMBCRAFT
{
	uint8_t i;

	if( sendpl == 0 ) return;

}


uint16_t totaldatalen;
uint16_t readsofar;

uint8_t Rbyte()  //DUMBCRAFT
{
	if( readsofar++ > totaldatalen ) return 0;
	return Wi07Pop();
}

uint8_t CanRead()  //DUMBCRAFT
{
	return readsofar < totaldatalen;
}

void SetServerName( const char * stname )
{
	memcpy( my_server_name, stname, strlen( stname ) + 1 );
	eeprom_write_block( stname, my_server_name_eeprom, strlen( stname ) + 1 );
}


/////////////////////////////////////////////Wi07 section/////////////////////////////////////










uint8_t frametime = 0;

ISR(TIMER0_OVF_vect )
{
	frametime++;
}

static void setup_clock( void )
{
	CLKPR = 0x80;
	CLKPR = 0x00;

#if   defined (__AVR_ATmega32U2__) || defined( __AVR_ATmega8U2__)
	//Only if we're on an xu2
	USBCON = (1<<USBE)|(1<<FRZCLK); //USB "Freeze" (And enable)
	REGCR = 0; //enable regulator.
	PLLCSR = (1<<PLLE)|(1<<PLLP0); //PLL Config
        while (!(PLLCSR & (1<<PLOCK)));	// wait for PLL lock
	USBCON = 1<<USBE; //Unfreeze USB
        UDCON = 0;// enable attach resistor (go on bus)
        UDIEN = (1<<EORSTE)|(1<<SOFE);
#endif

}

ISR(BADISR_vect)
{
	sendstr( "BAD ISR\n" );
}

void Wi07ReceiveCallback( uint8_t muxport, uint16_t datalen )
{
	int i;
	if( player_timeout >= 50 )
	{
		sendpl = 0;
		//new connection.
	}
	player_timeout = 0;

	sendstr( "RCCB\n" );
	sendhex4( datalen );
	sendchr ('\n' );
	readsofar = 0;
	totaldatalen = datalen;

	//Start popping data off.
	if( datalen < 1 ) return;
	int bright = Rbyte();
	OCR0A = bright;
	sendchr( '!' );
	sendhex2( bright );
	sendchr ('\n' );
		
	for( i = 0; i < totaldatalen-1; i++ )
	{
		sendchr( 0 );
		sendhex2( Rbyte() );
	}
	sendchr( '\n' );
}



int main( void )
{
	uint8_t i;
	int8_t rr;
	uint8_t delayctr;
	uint8_t marker;

	//Input the interrupt.
	DDRD &= ~_BV(2);
	cli();
	setup_spi();
	sendstr( "HELLO\n" );
	setup_clock();

	eeprom_read_block( &my_server_name[0], &my_server_name_eeprom[0], 16 );
	if( my_server_name[0] == 0 || my_server_name[0] == (char)0xff )
		SetServerName( "Test" );

	//~100Hz Timer0
	//Set COM0A1

	TCCR0A = _BV(COM0A1) | _BV(COM0A0) | _BV(WGM01) | _BV(WGM00); //CTC on OCR1A
	TCCR0B = _BV(CS01);
	DDRD |= _BV(6); // turn on OCR0A

	sei();

retry:
	rr = InitWi07( 1, AP_BASE, AP_PASS );
	if( rr ) goto retry;
	sendchr(0);
	sendhex2(rr );
	rr = Wi07ListenTCP("55555");
	sendchr(0);
	sendhex2(rr );
	if( rr ) goto retry;

	sendstr("Wifi Online\n");


	while(1)
	{

		if( sendpl )
		{
			sendstr( "M4S\n" );
			sendhex4( sendpl );
			sendchr( '\n' );
			if( Wi07TCPSend( 0, sendbuffer, sendpl ) == 0 )
			{
				sendstr( "SENDDONE\n" );
				sendpl = 0;
			}
		}
		Wi07CheckReceive();

	}



	return 0;
} 
















