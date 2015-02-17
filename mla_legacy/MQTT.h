/*********************************************************************
 *
 *                  MQTT (Internet of Things Protocol) Client
 *									Module for Microchip TCP/IP Stack
 *
 *********************************************************************
 * FileName:        mqtt.h
 * Dependencies:    TCP.h
 * Processor:       PIC18, PIC24F, PIC24H, dsPIC30F, dsPIC33F, PIC32
 * Compiler:        Microchip C32 v1.05 or higher
 *					Microchip C30 v3.12 or higher
 *					Microchip C18 v3.30 or higher
 *					HI-TECH PICC-18 PRO 9.63PL2 or higher
 * Company:         Microchip Technology, Inc.
 *
 * Software License Agreement
 *
 * Copyright (C) 2002-2009 Microchip Technology Inc.  All rights reserved.
 * Copyright (C) 2013,2014 Cyberdyne.  All rights reserved.
 *
 * Microchip licenses to you the right to use, modify, copy, and
 * distribute:
 * (i)  the Software when embedded on a Microchip microcontroller or
 *      digital signal controller product ("Device") which is
 *      integrated into Licensee's product; or
 * (ii) ONLY the Software driver source files ENC28J60.c, ENC28J60.h,
 *		ENCX24J600.c and ENCX24J600.h ported to a non-Microchip device
 *		used in conjunction with a Microchip ethernet controller for
 *		the sole purpose of interfacing with the ethernet controller.
 *
 * You should refer to the license agreement accompanying this
 * Software for additional information regarding your rights and
 * obligations.
 *
 *
 * Author               Date    Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Howard Schlunder     3/03/06	Original
 * Dario Greggio				30/9/14	mqtt client start
 * D. Guz                           20/01/15  Fix, cleanup, testing
 ********************************************************************/
#ifndef __MQTT_H
#define __MQTT_H

/****************************************************************************
  Section:
	Data Type Definitions
  ***************************************************************************/
	#define MQTT_SUCCESS		(0x0000u)	// successfully 
	#define MQTT_FAIL				(0xffffu)	// error 
	#define MQTT_RESOLVE_ERROR	(0x8000u)	// DNS lookup for MQTT server failed
	#define MQTT_CONNECT_ERROR	(0x8001u)	// Connection to MQTT server failed
	#define MQTT_BAD_PROTOCOL		(0x8101u)	// connect: bad protocol
	#define MQTT_IDENT_REJECTED	(0x8102u)	// connect: id rejected
	#define MQTT_SERVER_UNAVAILABLE	(0x8103u)	// connect: server unavailable
	#define MQTT_BAD_USER_PASW		(0x8104u)	// connect: bad user or password
	#define MQTT_UNAUTHORIZED			(0x8105u)	// connect: unauthorized
	#define MQTT_OPERATION_FAILED	(0x8201u)	// publish, subscribe error

// MQTT_MAX_PACKET_SIZE : Maximum packet size
#define MQTT_MAX_PACKET_SIZE 256

// MQTT_KEEPALIVE : keepAlive interval in Seconds
#define MQTT_KEEPALIVE_REALTIME 4
#define MQTT_KEEPALIVE_SHORT 15
#define MQTT_KEEPALIVE_LONG 120

#define MQTTPROTOCOLVERSION 3
#define MQTTCONNECT     1 << 4  // Client request to connect to Server
#define MQTTCONNACK     2 << 4  // Connect Acknowledgment
#define MQTTPUBLISH     3 << 4  // Publish message
#define MQTTPUBACK      4 << 4  // Publish Acknowledgment
#define MQTTPUBREC      5 << 4  // Publish Received (assured delivery part 1)
#define MQTTPUBREL      6 << 4  // Publish Release (assured delivery part 2)
#define MQTTPUBCOMP     7 << 4  // Publish Complete (assured delivery part 3)
#define MQTTSUBSCRIBE   8 << 4  // Client Subscribe request
#define MQTTSUBACK      9 << 4  // Subscribe Acknowledgment
#define MQTTUNSUBSCRIBE 10 << 4 // Client Unsubscribe request
#define MQTTUNSUBACK    11 << 4 // Unsubscribe Acknowledgment
#define MQTTPINGREQ     12 << 4 // PING Request
#define MQTTPINGRESP    13 << 4 // PING Response
#define MQTTDISCONNECT  14 << 4 // Client is Disconnecting
#define MQTTReserved    15 << 4 // Reserved

#define MQTTQOS0        (0 << 1)
#define MQTTQOS1        (1 << 1)
#define MQTTQOS2        (2 << 1)
	
/****************************************************************************
  Function:
      typedef struct MQTT_POINTERS
    
  Summary:
    Configures the MQTT client to send a message
    
  Description:
    This structure of pointers configures the MQTT Client to send an e-mail
    message. Initially, all pointers will be null.
    
        
  Parameters:
    Server -        the MQTT server to receive the message(s)
    User		 -      the user name to use when logging into the POP3 server,
                    if any is required
    Pass		 -      the password to supply when logging in, if any is required
    bSecure -       Port (method) to use
    ServerPort -    (WORD value) Indicates the port on which to connect to the
                    remote MQTT server.

  Remarks:

  ***************************************************************************/

typedef struct {
	union {
		char *szRAM;
#if defined(__18CXX)
		ROM char *szROM;
#endif	
		} Server;
	union	{
		char *szRAM;
#if defined(__18CXX)
		ROM char *szROM;
#endif	
		} Username;
	union	{
		char *szRAM;
#if defined(__18CXX)
		ROM char *szROM;
#endif	
		} Password;
	union	{
		char *szRAM;
#if defined(__18CXX)
		ROM char *szROM;
#endif	
		} ConnectId;
	union	{
		char *szRAM;
#if defined(__18CXX)
		ROM char *szROM;
#endif	
		} Topic;
	union	{
		BYTE *szRAM;
#if defined(__18CXX)
		ROM BYTE *szROM;
#endif	
		} Payload;
	union	{
		char *szRAM;
#if defined(__18CXX)
		ROM char *szROM;
#endif	
		} WillTopic;
	union	{
		char *szRAM;
#if defined(__18CXX)
		ROM char *szROM;
#endif	
		} WillMessage;

#if defined(__18CXX)
	struct {
		unsigned char Server:1;
		unsigned char Username:1;
		unsigned char Password:1;
		unsigned char ConnectId:1;
		unsigned char Topic:1;
		unsigned char Payload:1;
		unsigned char WillTopic:1;
		unsigned char WillMessage:1;
		} ROMPointers;
#endif	

	WORD Plength;
	BYTE Retained;
	BYTE WillQOS;
	BYTE WillRetain;

	BYTE Ver;
	WORD ServerPort;
	WORD MsgId;
	WORD KeepAlive;
	BYTE QOS;
	BYTE bSecure;
	BYTE bConnected;
	BYTE bAvailable;
	FILE *Stream;
	void (*m_Callback)(const char *,const BYTE *,unsigned int);
	
	} MQTT_POINTERS;


/****************************************************************************
  Section:
	Global MQTT Variables
  ***************************************************************************/
extern MQTT_POINTERS MQTTClient;
extern BYTE MQTTBuffer[MQTT_MAX_PACKET_SIZE];
extern WORD MQTTResponseCode;
	
/****************************************************************************
  Section:
	MQTT Function Prototypes
  ***************************************************************************/

BOOL MQTTBeginUsage(void);
WORD MQTTEndUsage(void);
void MQTTTask(void);
BOOL MQTTConnect(const char *, const char *, const char *, const char *, BYTE , BYTE , const char *);
BOOL MQTTIsBusy(void);
BOOL MQTTPublish(const char *, const BYTE *, WORD , BOOL );
BOOL MQTTPubACK(WORD);
BOOL MQTTSubscribe(const char *, BYTE);
BOOL MQTTPing(void);
BOOL MQTTDisconnect(void);
BOOL MQTTStop(void);

void MQTTCallback(const char *, const BYTE *, WORD );


BOOL MQTTWrite(BYTE , BYTE *, WORD );
WORD MQTTWriteString(const char *, BYTE *, WORD );
WORD MQTTReadPacket();  //BYTE *
BOOL MQTTPut(BYTE c);
WORD MQTTPutArray(BYTE *Data, WORD Len);
WORD MQTTPutString(BYTE *Data);
BYTE MQTTReadByte(void);
BOOL MQTTConnected(void);
#define MQTTAvailable() TCPIsGetReady(MySocket)

#if defined(__18CXX)
	WORD MQTTPutROMArray(ROM BYTE* Data, WORD Len);
	WORD MQTTPutROMString(ROM BYTE* Data);
	WORD MQTTPutROMString(ROM BYTE *Data);
	WORD MQTTPutROMArray(ROM BYTE *Data, WORD Len);
	WORD MQTTWriteROMString(const ROM char *, BYTE *, WORD );
#else
	// Non-ROM variant for C30 / C32
	#define MQTTPutROMArray(a,b)	MQTTPutArray((BYTE *)a,b)
	// Non-ROM variant for C30 / C32
	#define MQTTPutROMString(a)		MQTTPutString((BYTE *)a)
	#define MQTTWriteROMString(String, Data, Len) MQTTWriteROMString(String, Data, Len)
#endif


char *GetAsJSONValue(char *buf,const char *n,double v);

#endif
