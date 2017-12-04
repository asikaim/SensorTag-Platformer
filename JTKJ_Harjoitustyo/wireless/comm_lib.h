//DESCRIPTION/NOTES
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//		Name:			comm_lib.c
//		Description:	Functions for communication atop 6LoWPAN
// 		Author(s):		Teemu Leppanen, UBIComp, UOulu & Konstantin Mikhaylov, CWC, UOulu
//		Last modified:	2016.10.17
//		Note: 			The commenting style is optimized for automatic documentation generation using DOXYGEN: www.doxygen.org/
//		License:		Refer to Licence.txt file
//						partially based on CC2650 code of Contiki
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#ifndef JTKJ_EXAMPLE_WIRELESS_COMM_LIB_H_
#define JTKJ_EXAMPLE_WIRELESS_COMM_LIB_H_

#include "wireless/CWC_CC2650_154Drv.h"
#include "address.h"

#define IEEE80154_PANID				0x1337
#define IEEE80154_CHANNEL			0x0C
#define IEEE80154_SERVER_ADDR		0x1234

void Init6LoWPAN(void);
int8_t StartReceive6LoWPAN(void);
void Send6LoWPAN(uint16_t DestAddr, uint8_t *ptr_Payload, uint8_t u8_length);
int8_t Receive6LoWPAN(uint16_t *senderAddr, char *payload, uint8_t maxLen);

uint16_t GetAddr6LoWPAN(void);
uint8_t GetTXFlag(void);
uint8_t GetRXFlag(void);
int8_t GetRSSI(void);
void Radio_IRQ(CWC_CC2650_154_Events_t Event);
extern void RFCCPE0IntHandler(UArg arg0);
extern void RFCCPE1IntHandler(UArg arg0);

#endif /* JTKJ_EXAMPLE_WIRELESS_COMM_LIB_H_ */
