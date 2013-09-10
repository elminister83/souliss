/**************************************************************************
	Souliss - Plinio
    Copyright (C) 2012  Veseo

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
	
	Developed by Dario Di Maio, based on SoftModem by arms22
	
***************************************************************************/
/*!
    \file 
    \ingroup

*/
/**************************************************************************/

/**************************************************************************

	Plinio is and AVR built-in Binary Frequency Shift Keying (BFSK) for bus
	based communication with automatic carrier sense and collision detection, 
	developed over SoftModem library.
	
	The SoftModem project relased as Creative Commons and is hosted at
	http://code.google.com/p/arms22/
	
***************************************************************************/

#include <Arduino.h>
#include <avr/interrupt.h>
#include <pins_arduino.h>
#include "plinio.h"

volatile uint8_t *_txPortReg;
uint8_t _txPortMask, _lastTCNT,
	 _lastDiff, _recvStat, _recvBits, _recvBufferHead,
	 _recvBufferTail, _recvBuffer[BFSK_MODEM_MAX_RX_BUFF],
	 _lowCount, _highCount;

uint8_t bus_ca=0;				// Bus Collision Avoidance Index
uint8_t bus_ca_tx=0;			// Bus Collision Avoidance TX Counter
unsigned long wait_time=0;		// Bus Collision Avoidance Timer



/**************************************************************************/
/*!
	Init function
*/
/**************************************************************************/
void plinio_init()
{
	// Set I/O
	pinMode(BFSK_MODEM_RX_PIN1, INPUT);
	digitalWrite(BFSK_MODEM_RX_PIN1, LOW);
	pinMode(BFSK_MODEM_RX_PIN2, INPUT);
	digitalWrite(BFSK_MODEM_RX_PIN2, LOW);
	pinMode(BFSK_MODEM_TX_PIN, OUTPUT);
	digitalWrite(BFSK_MODEM_TX_PIN, LOW);

	_txPortReg = portOutputRegister(digitalPinToPort(BFSK_MODEM_TX_PIN));
	_txPortMask = digitalPinToBitMask(BFSK_MODEM_TX_PIN);

	_recvStat = 0xff;
	_recvBufferHead = _recvBufferTail = 0;

	_lastTCNT = TCNT2;
	_lastDiff = _lowCount = _highCount = 0;

	// Start the analog comparator
	TCCR2A = 0;
	TCCR2B = TIMER_CLOCK_SELECT;
	ACSR   = _BV(ACIE) | _BV(ACIS1);
}

/**************************************************************************/
/*!
	Release the timer and the analog comparator
*/
/**************************************************************************/
void plinio_end()
{
	ACSR   &= ~(_BV(ACIE));
	TIMSK2 &= ~(_BV(OCIE2A));
}

/**************************************************************************/
/*!
	Demodulate the incoming bit at each cross of the modulated signal, 
	measure the time difference between two bits in the signal to identify
	if is a '1' or '0'.
*/
/**************************************************************************/
void plinio_demodulate()
{
	uint8_t t = TCNT2;	// Store the counter value
	uint8_t diff;
	
	// Get the difference in cycles between two demodulation
	if(TIFR2 & _BV(TOV2)){	
		// In TIFR2 register, TOV2 bit is set when the TCNT2 overflow
		TIFR2 |= _BV(TOV2);
		diff = (255 - _lastTCNT) + t + 1;		// Recover the overflow
	}
	else{
		diff = t - _lastTCNT;
	}
	
	// At this point in diff there are the number of cycles between two
	// cross in the analog comparator. Noise may cause a cross just before
	// or after the real cross of the modulated signal, so check if the
	// diff value is resonable or not.
	
	// If diff is too small, the cross was driven by the noise just after
	// the starting of a new period of the modulated wave, exit without
	// reset the _lastTCNT, the next time maybe the right one.
	if(diff < (uint8_t)(TCNT_HIGH_TH_L))				
		return;
	
	_lastTCNT = t;
	
	// If diff is too big, the real cross was missed due to noise. We shall
	// start with a new period of the wave, _lastTCNT may be reseted.
	if(diff > (uint8_t)(TCNT_LOW_TH_H))
		return;
	
	_lastDiff = (diff >> 1) + (diff >> 2) + (_lastDiff >> 2);
	
	// Match the wave period with the period of a low bit
	if(_lastDiff >= (uint8_t)(TCNT_LOW_TH_L)){
		_lowCount += _lastDiff;		// Increase the counter of periods for a low bit
		
		// If the number of periods for a low bit is near the expected one, we had a low bit.
		// The start-bit is always a low bit
		if((_recvStat == 0xff) && (_lowCount >= (uint8_t)(TCNT_BIT_PERIOD * 0.5))){ 
			_recvStat  = FSK_START_BIT;
			_highCount = 0;
			_recvBits  = 0;
			
			// Set the comparison OCR2A register to wave period of a bit, enable the interrupt
			// When the bit wave will end, we will have an interrupt
			OCR2A      = t + (uint8_t)(TCNT_BIT_PERIOD) - _lowCount; 
			TIFR2     |= _BV(OCF2A);
			TIMSK2    |= _BV(OCIE2A);
		}
	}
	// Match the wave period with the period of a high bit 
	else if(_lastDiff <= (uint8_t)(TCNT_HIGH_TH_H)){
		_highCount += _lastDiff;	// Increase the counter of periods for a high bit
		
		// If the number of periods for a low bit is near the expected one, we had a high bit.
		// The start-bit cannot be an high bit, discard
		if((_recvStat == 0xff) && (_highCount >= (uint8_t)(TCNT_BIT_PERIOD))){
			_lowCount = _highCount = 0;
		}
	}
}

/**************************************************************************/
/*!
	Set an interrupt for each cross of the modulated signal over the defined
	threshold. Activate the demodulation method.
*/
/**************************************************************************/
ISR(ANALOG_COMP_vect)
{
	// Increase the collision detection bus index
	plinio_busy();
	
	// Demodulate the incoming FSK
	plinio_demodulate();
}

/**************************************************************************/
/*!
	If the timer were elapsed, count the '1' and '0' that was demodulated
*/
/**************************************************************************/
void plinio_recv()
{
	// We jump in due to interrupt on OCR2A, so a bit wave was completed,
	// now _highCount and _lowCount contains the number of period that
	// was into the bit.

	uint8_t high;
	if(_highCount > _lowCount){
 		if(_highCount >= (uint8_t)TCNT_BIT_PERIOD)
 			_highCount -= (uint8_t)TCNT_BIT_PERIOD;
		else
			_highCount = 0;
			
		// If was the high frequency carrier, set the most significant bit
		// for this temporary variable
		high = 0x80;
	}
	else{
 		if(_lowCount >= (uint8_t)TCNT_BIT_PERIOD)
 			_lowCount -= (uint8_t)TCNT_BIT_PERIOD;
 		else
			_lowCount = 0;
			
		// If was the low frequency carrier, reset the temporary variable	
		high = 0x00;
	}
	
	// If was recognized a start bit
	if(_recvStat == FSK_START_BIT){	// Start bit
		if(!high){
			// Start bit shall be low
			_recvStat++;	// Move to next enumerated state
		}else{
			goto end_recv;
		}
	}
	// If we are waiting for data bits
	else if(_recvStat <= FSK_D7_BIT) { 
		_recvBits >>= 1;	// Shift one bit
		_recvBits |= high;	// Set the actual most significant bit
		_recvStat++;		// Move to next enumerated state
	}
	// If we are waiting for the stop bit
	else if(_recvStat == FSK_STOP_BIT){
		uint8_t new_tail = ((_recvBufferTail + 1) % (BFSK_MODEM_MAX_RX_BUFF - 1));
		if(new_tail != _recvBufferHead){
			_recvBuffer[_recvBufferTail] = _recvBits;	// Store the received byte in the RX buffer
			_recvBufferTail = new_tail;
		}
		goto end_recv;
	}
	else{
	end_recv:
		_recvStat = 0xff;				// Reset the rx status
		TIMSK2 &= ~_BV(OCIE2A);			// Disable the interrupt
		
		plinio_setbus(BFSK_CA_RST);		// Byte received, reduce the bus load value
	}
}

/**************************************************************************/
/*!
	Set the interrupt timer at the wave period, once the initial carrier is 
	found, the timer is activated. When the interrupt is active, we are near
	the end of the carrier.
*/
/**************************************************************************/
ISR(TIMER2_COMPA_vect)
{
	OCR2A += (uint8_t)TCNT_BIT_PERIOD;			// Set comparison to next wave period
	plinio_recv(); 
}

/**************************************************************************/
/*!
	Returns if the data buffer is full
*/
/**************************************************************************/
uint8_t plinio_full()
{
	return (((_recvBufferTail + 1) % (BFSK_MODEM_MAX_RX_BUFF - 1)) == _recvBufferHead);
}

/**************************************************************************/
/*!
	Access the circular buffer and read a byte without removing it from
	the buffer
*/
/**************************************************************************/
uint8_t plinio_getbyte(uint8_t byteno)
{

	if(_recvBufferHead == _recvBufferTail)
		return 0;		// No data

	if(_recvBufferTail > _recvBufferHead) 
	{		
		// Get the amount of bytes into the buffer
		uint8_t len = _recvBufferTail - _recvBufferHead;
		
		// Return the requested byte
		if(len>byteno)
			return _recvBuffer[_recvBufferHead+byteno];
	}
	else	// The tail of the circual buffer rounds up to the begin of the buffer
	{	
		// Read all values up the end of the buffer
		uint8_t len = BFSK_MODEM_MAX_RX_BUFF - _recvBufferHead;
		
		// Return the requested byte
		if(len>byteno)
			return _recvBuffer[_recvBufferHead+byteno];
	}	
	
	return 0;
}

/**************************************************************************/
/*!
	Remove len bytes starting from index byte in the circular buffer
*/
/**************************************************************************/
void plinio_remove(uint8_t index, uint8_t len)
{
	if(_recvBufferHead == _recvBufferTail)
		return;		// No data
	
	_recvBufferHead = (_recvBufferHead + index + len) % (BFSK_MODEM_MAX_RX_BUFF - 1);
}

/**************************************************************************/
/*!
	Access the circular buffer and get incoming available data
*/
/**************************************************************************/
uint8_t plinio_retrieve(uint8_t *data, uint8_t max_len)
{
	if(max_len == 0)
		return 0;		// No buffer

	if(_recvBufferHead == _recvBufferTail)
		return 0;		// No data

	if(_recvBufferTail > _recvBufferHead) 
	{		
		uint8_t len = _recvBufferTail - _recvBufferHead;
		if(len>max_len) len=max_len;

		memmove(data, &(_recvBuffer[_recvBufferHead]), len);
		_recvBufferHead = (_recvBufferHead + len) % (BFSK_MODEM_MAX_RX_BUFF - 1);		

		return len;
	}
	else	// The tail of the circual buffer rounds up to the begin of the buffer
	{	
		// Read all values up the end of the buffer
		uint8_t len = BFSK_MODEM_MAX_RX_BUFF - _recvBufferHead;
		if(len>max_len) len=max_len;		
		
		memmove(data, &(_recvBuffer[_recvBufferHead]), len);
		_recvBufferHead = 0;	// Re init the head
		
		// If still there are data to be read
		if(_recvBufferTail)
		{
			uint8_t len2;
			
			// Check how many data can be retrieved
			if(_recvBufferTail+len>max_len)
				len2 = max_len - len;
			else
				len2 = _recvBufferTail;
			
			// Move data into the top layer buffer
			memmove(data+len, &(_recvBuffer[_recvBufferHead]), len2);				
		
			// Init the circular buffer
			_recvBufferTail = 0;
		
			// Return the complete lenght of the message
			return len+len2;
		}	
		else
			return len;
	}	
}

/**************************************************************************/
/*!
	Build a PWM signal for the '1' and '0' carrier wave
*/
/**************************************************************************/
void plinio_modulate(uint8_t b)
{
	uint8_t cnt,tcnt,tcnt2,adj;
	if(b){
		cnt = (uint8_t)(BFSK_MODEM_HIGH_CNT);			// Number of wave periods for a bit
		tcnt2 = (uint8_t)(TCNT_HIGH_FREQ / 2);			// Period in uC cycle	
		tcnt = (uint8_t)(TCNT_HIGH_FREQ) - tcnt2;		// Half period
	}else{
		cnt = (uint8_t)(BFSK_MODEM_LOW_CNT);			// Number of wave periods for a bit
		tcnt2 = (uint8_t)(TCNT_LOW_FREQ / 2);			// Period in uC cycle	
		tcnt = (uint8_t)(TCNT_LOW_FREQ) - tcnt2;		// Half period
	}
	do {
		// Build a square wave with cnt periods at tcnt2 frequency, duty cycle 50%
		cnt--;
		{
			OCR2B += tcnt;
			TIFR2 |= _BV(OCF2B);
			while(!(TIFR2 & _BV(OCF2B)));
		}
		*_txPortReg ^= _txPortMask;
		{
			OCR2B += tcnt2;
			TIFR2 |= _BV(OCF2B);
			while(!(TIFR2 & _BV(OCF2B)));
		}
		*_txPortReg ^= _txPortMask;
	} while (cnt);
}

/**************************************************************************/
/*!
	If there are unread bytes, return the amount of them
*/
/**************************************************************************/
uint8_t plinio_available()	
{
	// Record the RX process, this free new transmission
	plinio_rx_count();

	if(_recvBufferTail < _recvBufferHead)
		return (_recvBufferTail - _recvBufferHead) % (BFSK_MODEM_MAX_RX_BUFF - 1);
	else if(_recvBufferTail > _recvBufferHead)
		return (_recvBufferTail + BFSK_MODEM_MAX_RX_BUFF - _recvBufferHead) % (BFSK_MODEM_MAX_RX_BUFF - 1);
	else
		return 0;
}

/**************************************************************************/
/*!
	Send a byte as set of carrier waves
*/
/**************************************************************************/
uint8_t plinio_send(uint8_t *data, uint8_t len)
{
	// Verify bus availability
	for(uint8_t i=0;i<BFSK_CA_TX_RETRY;i++)
	{	
		wait_time=millis();						// Record the current time
		uint8_t busaval = plinio_busval();		// Record bus load value
		
		// If the bus is not available, wait a bit
		while((!plinio_busavailable()) && (wait_time+BFSK_CA_MAX_WAIT > millis()));
	
		// If the bus is now available and the max TX/RX ratio is not above limit
		if((!(plinio_busval() > busaval)) && (plinio_tx_allowed()))	
			break;
		else if(i == (BFSK_CA_TX_RETRY - 1) || !(plinio_tx_allowed()))	
			return	BFSK_FAIL;				// Return as fail	
	}
	
	// The bus is free, start transmission
	plinio_tx_count();
	
	// Send data
	for(uint8_t j=0;j<BFSK_CA_TX_NO;j++)
	{	
		for(uint8_t i = 0; i<(uint8_t)BFSK_MODEM_CARRIR_CNT; i++){
			plinio_modulate(HIGH);
		}	
		
		// Send the data stream
		for(uint8_t i=0;i<len;i++)
		{	
			// Send the start bit
			plinio_modulate(LOW);
			for(uint8_t mask = 1; mask; mask <<= 1){ // Data Bits	
				if(data[i] & mask){
					plinio_modulate(HIGH);
				}
				else{
					plinio_modulate(LOW);
				}
			}
			
			plinio_modulate(HIGH);				// Stop Bit
			plinio_modulate(HIGH);				// Push Bit	
		}	
		
		// Wait BFSK_CA_TX_DELAY milliseconds and verify if collision occurred
		wait_time=millis();						
		while(wait_time+BFSK_CA_TX_DELAY > millis());		
		
		// Verify if a collision is occurred
		uint8_t collision_index=0;
		for(uint8_t i=0;i<len;i++)
			if(data[i] != _recvBuffer[((_recvBufferHead+i) % (BFSK_MODEM_MAX_RX_BUFF - 1))])
				collision_index++;
		
		if(collision_index < BFSK_CA_COLIDX_MAX)
		{
			// I've just sent a message, increase my penalty 
			plinio_setbus(BFSK_CA_RST);
			
			return BFSK_SUCCESS;
		}		
		
		// Wait a random time
		wait_time=millis() & 0x2A;
		wait_time+=millis();
		while(wait_time > millis());	
	}
		
	// I've just sent a message, increase my penalty 
	plinio_setbus(2*BFSK_CA_RST);
	
	return BFSK_FAIL;
}

/**************************************************************************/
/*!
	Decrease the collision detection index
*/
/**************************************************************************/
void plinio_collisionavoidance()
{
	if(bus_ca > BFSK_CA_DCR)
		bus_ca -= BFSK_CA_DCR;
	else
		bus_ca = BFSK_CA_MIN;
}		



	