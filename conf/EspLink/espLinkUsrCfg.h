/**************************************************************************
	Souliss - vNet Virtualized Network
    Copyright (C) 2014  Veseo

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
	
	Originally developed by Dario Di Maio
	
***************************************************************************/
/**************************************************************************/
/*!
    \file 
    \ingroup
 
    This is the user configurable file. It contains parameters that users
    can change to suit their needs. 

*/
/**************************************************************************/
#ifndef ESPLINK_USR_CFG_H
#define ESPLINK_USR_CFG_H

/**************************************************************************/
/*!
	Select the Serial connection
	        
		Examples       
        Serial
		SoftwareSerial
*/
/**************************************************************************/

#ifndef	ESPLINK_SERIAL_INSKETCH
#	define	ESPLINKSERIAL		Serial
#endif

#define NRF24_MAX_PAYLOAD   32					// Max payload allowed for this radio
#define NRF24_CHANNEL     	120 // 76					// Default channel to be used
#define	NRF24_PIPE			0xF0F0F00000LL		// Base address for the node

#define NRF24_SUCC			1
#define NRF24_FAIL			0

#endif
