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
/*!
    \file 
    \ingroup

*/
/**************************************************************************/

#include "src/ELClient.cpp"
#include "src/ELClientSocket.cpp"

ELClient espLink(&ESPLINKSERIAL);
ELClientSocket udp(&espLink);
ELClientPacket *packet;
#if (VNET_DEBUG)
	#define VNET_LOG LOG.print
#endif

/**************************************************************************/
/*!
    Init esplink
*/
/**************************************************************************/
void vNet_Init_M1()
{
	VNET_LOG(F("Inizio!"));
	
	ESPLINKSERIAL.begin(9600);
	bool ok;
	do {
		ok = espLink.Sync();			// sync up with esp-link, blocks for up to 2 seconds
		if (!ok) VNET_LOG(F("EL-Client sync failed!")); VNET_LOG("\r\n");
	} while(!ok);
	
	#if (VNET_DEBUG)
	espLink.GetWifiStatus();
	
	if ((packet=espLink.WaitReturn()) != NULL) {
		VNET_LOG(F("Wifi status: "));
		VNET_LOG(packet->value);
		VNET_LOG("\r\n");
	}
	#endif
}

/**************************************************************************/
/*!
	Cannot set address from esplink
*/
/**************************************************************************/
void vNet_SetAddress_M1(uint16_t addr)
{
	
}

/**************************************************************************/
/*!
	Send data out
*/
/**************************************************************************/
uint8_t vNet_Send_M1(uint16_t addr, oFrame *frame, uint8_t len)
{
	
	uint16_t vNet_port;

	// Define the standard vNet port
	vNet_port = ETH_PORT;
	
	if(len > VNET_MAX_PAYLOAD)
		return VNET_DATA_FAIL;

	// If is a vNet broadcast, move on the broadcast pipe
	//if((addr == VNET_ADDR_wBRDC) || (addr == VNET_ADDR_nBRDC))
	//	addr = VNET_ADDR_BRDC;
	
	//int err = udp.begin(addr, vNet_port, SOCKET_UDP, udpCb);
	int err = udp.begin(addr, vNet_port, SOCKET_UDP);
	if (err < 0) {
		VNET_LOG(F("UDP begin failed: "));
		VNET_LOG(err);
		VNET_LOG("\r\n");
		//delay(10000);
		//asm volatile ("  jmp 0");
	}
	
	udp.send(frame,&len);
	
	
	/*
	// Before write, stop listening pipe
	radio.stopListening();
	
	// Send out the oFrame, doesn't need to specify the length
    if(radio.write(frame, 0))
	{
		// Listening back
		radio.startListening();
		
		return NRF24_SUCC;
	}	
	else
	{
		oFrame_Reset();		// Free the frame
		radio.startListening();
	
		return NRF24_FAIL;
	}	*/
}	

/**************************************************************************/
/*!
	Check for incoming data
*/
/**************************************************************************/
uint8_t vNet_DataAvailable_M1()
{
	packet = espLink.Process();
	if (packet != 0) 
		return true;
    else
		return false;
}

/**************************************************************************/
/*!
	Retrieve data from wifi
*/
/**************************************************************************/
uint8_t vNet_RetrieveData_M1(uint8_t *data)
{
	/* uint8_t* data_pnt = data;

	uint8_t len = radio.getDynamicPayloadSize();
	uint8_t state = radio.read(data, len);
	
	//if(!state)	return 0;					// Just skip out
	
	// The nRF24L01 support small payloads and it could be cut just before sending
	// at this stage we verify the original length and fill the missing with zeros.
	
	uint8_t	original_len = *(data);			// First byte is the expected length of
											// the vNet frame
	// Fill in the missing bytes
	if(original_len > len)
		for(uint8_t i=0; i<(original_len-len); i++)
			*(data+len+i) = 0; 
	*/
			
	return 0;
}

/**************************************************************************/
/*!
	Actually isn't possible to get the source address for the last received
	frame.
*/
/**************************************************************************/
uint16_t vNet_GetSourceAddress_M1()
{
	return 0;
}

/**************************************************************************/
/*!
    Set the base IP address
*/
/**************************************************************************/
void eth_SetBaseIP(uint8_t *ip_addr)
{
}

/**************************************************************************/
/*!
    Set the IP address
*/
/**************************************************************************/
void eth_SetIPAddress(uint8_t *ip_addr)
{
}

/**************************************************************************/
/*!
    Set the Subnet mask
*/
/**************************************************************************/
void eth_SetSubnetMask(uint8_t *submask)
{
}

/**************************************************************************/
/*!
    Set the Gateway
*/
/**************************************************************************/
void eth_SetGateway(uint8_t *gateway)
{
}

/**************************************************************************/
/*!
    Get the IP address
*/
/**************************************************************************/
void eth_GetIP(uint8_t *ip_addr)
{
}

/**************************************************************************/
/*!
    Get the base IP address
*/
/**************************************************************************/
void eth_GetBaseIP(uint8_t *ip_addr)
{
}

/**************************************************************************/
/*!
    Get the Subnet mask
*/
/**************************************************************************/
void eth_GetSubnetMask(uint8_t *submask)
{	
}

/**************************************************************************/
/*!
    Get the Gateway
*/
/**************************************************************************/
void eth_GetGateway(uint8_t *gateway)
{	
}
