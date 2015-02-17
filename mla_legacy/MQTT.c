/*********************************************************************
 *
 *	MQTT Internet Of Things (MQTT) Client
 *	Module for Microchip TCP/IP Stack
 *   -Provides ability to receive Emails
 *	 -Reference: RFC 2821
 *
 *********************************************************************
 * FileName:        MQTT.c
 * Dependencies:    TCP, ARP, DNS, Tick
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

 *	https://developer.ibm.com/iot/recipes/improvise/
 *	https://internetofthings.ibmcloud.com/dashboard/#/organizations/mkxk7/devices
 *	https://developer.ibm.com/iot/recipes/improvise-registered-devices/
 *
 * Author               Date    Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Howard Schlunder     3/03/06	Original
 * Howard Schlunder			11/2/06	Vastly improved for release
 * Dario Greggio				30/9/14	client MQTT (IoT), inizio
 * - A simple client for MQTT.  Original Code - Nicholas O'Leary  http://knolleary.net
 * - Adapted for Spark Core by Chris Howard - chris@kitard.com  (Based on PubSubClient 1.9.1)
 *  D. Guz                           20/01/15  Fix, cleanup, testing
 ********************************************************************/
#define __MQTT_C

#include "TCPIPConfig.h"


#if defined(STACK_USE_MQTT_CLIENT)

#include "TCPIP Stack/TCPIP.h"

#include "generictypedefs.h"

/****************************************************************************
  Section:
	MQTT Client Configuration Parameters
  ***************************************************************************/
#define MQTT_PORT					1883					// Default port to use when unspecified
#define MQTT_PORT_SECURE	8883					// Default port to use when unspecified
#define MQTT_SERVER_REPLY_TIMEOUT	(TICK_SECOND*8)		// How long to wait before assuming the connection has been dropped (default 8 seconds)


/****************************************************************************
  Section:
	MQTT Client Public Variables
  ***************************************************************************/
// The global set of MQTT_POINTERS.
// Set these parameters after calling MQTTBeginUsage successfully.
MQTT_POINTERS MQTTClient;

/****************************************************************************
  Section:
	MQTT Client Internal Variables
  ***************************************************************************/
static IP_ADDR MQTTServer;						// IP address of the remote MQTT server
static TCP_SOCKET MySocket = INVALID_SOCKET;	// Socket currently in use by the MQTT client

WORD MQTTResponseCode;

BYTE MQTTBuffer[MQTT_MAX_PACKET_SIZE];

static BYTE rxBF[200];
static word bytesRecieved = 0;

static WORD lastInActivity=0,lastOutActivity=0;

static word LastPingTick = 0;

// Message state machine for the MQTT Client
static enum {
    /*
	MQTT_HOME = 0,					// Idle start state for MQTT client 
	MQTT_BEGIN,							// Preparing to make connection
	MQTT_NAME_RESOLVE,			// Resolving the MQTT server address
	MQTT_OBTAIN_SOCKET,			// Obtaining a socket for the MQTT connection
	MQTT_SOCKET_OBTAINED,		// MQTT connection successful
	//MQTT_CLOSE,							// MQTT socket is closed
	MQTT_CONNECT,						// connection
	MQTT_CONNECT_ACK,				// connected
	MQTT_PUBLISH,						// Publish
	MQTT_PUBLISH_ACK,				// Publish command accepted (if QOS)
	MQTT_PUBACK,						// PublishACK
	MQTT_SUBSCRIBE,					// Subscribe
	MQTT_SUBSCRIBE_ACK,			// Subscribe command accepted (if QOS)
	MQTT_UNSUBSCRIBE,					// Subscribe
	MQTT_UNSUBSCRIBE_ACK,			// Subscribe command accepted (if QOS)
	MQTT_PING,							// Ping
	MQTT_PING_ACK,					// Pingback
	MQTT_LOOP,							// Idle Loop for receiving messages
	MQTT_IDLE=MQTT_LOOP,		// Idle Loop for receiving messages
	MQTT_DISCONNECT_INIT,		// Sending Disconnect
	MQTT_DISCONNECT,				// Disconnect accepted, and...
	MQTT_QUIT								// ...connection closing
	} MQTTState;
    */

        MQTT_HOME = 0,
        MQTT_BEGIN,
        MQTT_NAME_RESOLVE,
        MQTT_OBTAIN_SOCKET,
        MQTT_SOCKET_OBTAINED,
        MQTT_CONNECT,
        MQTT_CONNECT_ACK,
        MQTT_PING,
        MQTT_PING_ACK,
        MQTT_PUBLISH,
        MQTT_PUBLISH_ACK,
        MQTT_SUBSCRIBE,
        MQTT_SUBSCRIBE_ACK,
        MQTT_PUBACK,
        MQTT_UNSUBSCRIBE,
        MQTT_UNSUBSCRIBE_ACK,
        MQTT_DISCONNECT_INIT,
        MQTT_DISCONNECT,
        MQTT_CLOSE,
        MQTT_QUIT,
        MQTT_IDLE
    } MQTTState;

// Internal flags used by the MQTT Client
static union {
	BYTE Val;
	struct {
		unsigned char MQTTInUse:1;
		unsigned char ReceivedSuccessfully:1;
		unsigned char ConnectedOnce:1;
		unsigned char PingOutstanding:1;
		unsigned char filler:5;
		} bits;
	} MQTTFlags = {0x00};
	

/****************************************************************************
  Section:
	MQTT Client Internal Function Prototypes
  ***************************************************************************/


/****************************************************************************
  Section:
	MQTT Function Implementations
  ***************************************************************************/

/*****************************************************************************
  Function:
	BOOL MQTTBeginUsage(void)

  Summary:
	Requests control of the MQTT client module.

  Description:
	Call this function before calling any other MQTT Client APIs.  This 
	function obtains a lock on the MQTT Client, which can only be used by
	one stack application at a time.  Once the application is finished
	with the MQTT client, it must call MQTTEndUsage to release control
	of the module to any other waiting applications.
	
	This function initializes all the MQTT state machines and variables
	back to their default state.

  Precondition:
	None

  Parameters:
	None

  Return Values:
	TRUE - The application has successfully obtained control of the module
	FALSE - The MQTT module is in use by another application.  Call 
		MQTTBeginUsage again later, after returning to the main program loop
  ***************************************************************************/
BOOL MQTTBeginUsage(void) {

	if(MQTTFlags.bits.MQTTInUse)
		return FALSE;

	MQTTFlags.Val = 0x00;
	MQTTFlags.bits.MQTTInUse = TRUE;
	MQTTState = MQTT_BEGIN;
	memset((void*)&MQTTClient, 0x00, sizeof(MQTTClient));
	MQTTClient.Ver=MQTTPROTOCOLVERSION;
	MQTTClient.KeepAlive=MQTT_KEEPALIVE_LONG;
	MQTTClient.MsgId=1;
		
	return TRUE;
	}

/*****************************************************************************
  Function:
	WORD MQTTEndUsage(void)

  Summary:
	Releases control of the MQTT client module.

  Description:
	Call this function to release control of the MQTT client module once
	an application is finished using it.  This function releases the lock
	obtained by MQTTBeginUsage, and frees the MQTT client to be used by 
	another application.

  Precondition:
	MQTTBeginUsage returned TRUE on a previous call.

  Parameters:
	None

  Return Values:
	MQTT_SUCCESS - A message was successfully sent
	MQTT_RESOLVE_ERROR - The MQTT server could not be resolved
	MQTT_CONNECT_ERROR - The connection to the MQTT server failed or was prematurely terminated
	1-199 and 300-399 - The last MQTT server response code BOH!
  ***************************************************************************/
WORD MQTTEndUsage(void) {

	if(!MQTTFlags.bits.MQTTInUse)
		return 0xFFFF;

	// Release the DNS module, if in use
	if(MQTTState == MQTT_NAME_RESOLVE)
		DNSEndUsage();
	
	if(MQTTClient.bConnected)
		MQTTDisconnect();

	// Release the TCP socket, if in use
	if(MySocket != INVALID_SOCKET) {
		TCPDisconnect(MySocket);
		MySocket = INVALID_SOCKET;
		}
	
	// Release the MQTT module
	MQTTFlags.bits.MQTTInUse = FALSE;
	MQTTState = MQTT_HOME;

	if(MQTTFlags.bits.ReceivedSuccessfully)	{
		return MQTT_SUCCESS;
		}
	else {
//		return MQTTResponseCode;
		}
	}

/*****************************************************************************
  Function:
	void MQTTTask(void)

  Summary:
	Performs any pending MQTT client tasks

  Description:
	This function handles periodic tasks associated with the MQTT client,
	such as processing initial connections and command sequences.

  Precondition:
	None

  Parameters:
	None

  Returns:
	None

  Remarks:
	This function acts as a task (similar to one in an RTOS).  It
	performs its task in a co-operative manner, and the main application
	must call this function repeatedly to ensure that all open or new
	connections are served in a timely fashion.
  ***************************************************************************/
void MQTTTask(void) {
	WORD			i;
	WORD			w;
	static DWORD	Timer;
	static WORD nextMsgId;
	static ROM BYTE *ROMStrPtr;
	static BYTE 	*RAMStrPtr;

	switch(MQTTState)	{
		case MQTT_HOME:
			// MQTTBeginUsage() is the only function which will kick 
			// the state machine into the next state
			break;

		case MQTT_BEGIN:

			// Obtain ownership of the DNS resolution module
			if(!DNSBeginUsage())
				break;

			// Obtain the IP address associated with the MQTT mail server
			if(MQTTClient.Server.szRAM) {
#if defined(__18CXX)
				if(MQTTClient.ROMPointers.Server)
					DNSResolveROM(MQTTClient.Server.szROM, DNS_TYPE_A);
				else
#endif
					DNSResolve(MQTTClient.Server.szRAM, DNS_TYPE_A);
				}
			else {
				MQTTState=MQTT_HOME;		// can't do anything
				break;
				}
			
			Timer = TickGet();
			MQTTState++;
			break;

		case MQTT_NAME_RESOLVE:
			// Wait for the DNS server to return the requested IP address
			if(!DNSIsResolved(&MQTTServer))	{
				// Timeout after 6 seconds of unsuccessful DNS resolution
				if(TickGet() - Timer > 6*TICK_SECOND)	{
					MQTTResponseCode = MQTT_RESOLVE_ERROR;
					MQTTState = MQTT_HOME;
					DNSEndUsage();
					}
				break;
				}

			// Release the DNS module, we no longer need it
			if(!DNSEndUsage()) {
				// An invalid IP address was returned from the DNS 
				// server.  Quit and fail permanantly if host is not valid.
				MQTTResponseCode = MQTT_RESOLVE_ERROR;
				MQTTState = MQTT_HOME;
				break;
				}

			MQTTState++;
			// No need to break here

		case MQTT_OBTAIN_SOCKET:
			// Connect a TCP socket to the remote MQTT server
			MySocket = TCPOpen(MQTTServer.Val, TCP_OPEN_IP_ADDRESS, MQTTClient.ServerPort, TCP_PURPOSE_DEFAULT);
			
			// Abort operation if no TCP sockets are available
			// If this ever happens, add some more 
			// TCP_PURPOSE_DEFAULT sockets in TCPIPConfig.h
			if(MySocket == INVALID_SOCKET)
				break;

			MQTTState++;
			Timer = TickGet();
			// No break; fall into MQTT_SOCKET_OBTAINED
			
		
		case MQTT_SOCKET_OBTAINED:
			if(!TCPIsConnected(MySocket)) {
				// Don't stick around in the wrong state if the
				// server was connected, but then disconnected us.
				// Also time out if we can't establish the connection to the MQTT server
				if(MQTTFlags.bits.ConnectedOnce || ((LONG)(TickGet()-Timer) > (LONG)(MQTT_SERVER_REPLY_TIMEOUT)))	{
					MQTTResponseCode = MQTT_CONNECT_ERROR;
					MQTTState = MQTT_CLOSE;
					}

				break;
				}
			MQTTFlags.bits.ConnectedOnce = TRUE;


		case MQTT_CONNECT:
			if(!MQTTConnected()) {

				if(MQTTFlags.bits.ConnectedOnce) {
					nextMsgId = 1;
					BYTE d[9] = {0x00,0x06,'M','Q','I','s','d','p',MQTTPROTOCOLVERSION};
					// Leave room in the buffer for header and variable length field
					WORD length = 5;
					unsigned int j;

					for(j=0; j<9; j++) 
						MQTTBuffer[length++] = d[j];

					BYTE v;
					if(MQTTClient.WillTopic.szRAM) {
						MQTTClient.QOS=MQTTClient.WillQOS;
						v = 0x06 | (MQTTClient.WillQOS<<3) | (MQTTClient.WillRetain<<5);
						}
					else 
						v = 0x02;

					if(MQTTClient.Username.szRAM) {
						v |= 0x80;
						if(MQTTClient.Password.szRAM) 
							v = v | (0x80>>1);
 						}

					MQTTBuffer[length++] = v;

					MQTTBuffer[length++] = HIBYTE(MQTTClient.KeepAlive);
					MQTTBuffer[length++] = LOBYTE(MQTTClient.KeepAlive);

#if defined(__18CXX)
					if(MQTTClient.ROMPointers.ConnectId) 
						length = MQTTWriteROMString(MQTTClient.ConnectId.szROM,MQTTBuffer,length);
					else
#endif
						length = MQTTWriteString(MQTTClient.ConnectId.szRAM,MQTTBuffer,length);
					if(MQTTClient.WillTopic.szRAM) {
#if defined(__18CXX)
						if(MQTTClient.ROMPointers.WillTopic) 
							length = MQTTWriteString(MQTTClient.WillTopic.szROM,MQTTBuffer,length);
						else
#endif
							length = MQTTWriteString(MQTTClient.WillTopic.szRAM,MQTTBuffer,length);
#if defined(__18CXX)
						if(MQTTClient.ROMPointers.WillMessage) 
							length = MQTTWriteROMString(MQTTClient.WillMessage.szROM,MQTTBuffer,length);
						else
#endif
							length = MQTTWriteString(MQTTClient.WillMessage.szRAM,MQTTBuffer,length);
						}

					if(MQTTClient.Username.szRAM) {		// il check su union è ok!
#if defined(__18CXX)
						if(MQTTClient.ROMPointers.Username) {
							length = MQTTWriteROMString(MQTTClient.Username.szROM,MQTTBuffer,length);
							if(MQTTClient.Password.szRAM) {
								if(MQTTClient.ROMPointers.Password) {
									length = MQTTWriteROMString(MQTTClient.Password.szROM,MQTTBuffer,length);
									}	
								else {
									length = MQTTWriteString(MQTTClient.Password.szRAM,MQTTBuffer,length);
									}	
								}
							}	
						else {
#endif
							length = MQTTWriteString(MQTTClient.Username.szRAM,MQTTBuffer,length);
							if(MQTTClient.Password.szRAM) {
#if defined(__18CXX)
								if(MQTTClient.ROMPointers.Password)
									length = MQTTWriteROMString(MQTTClient.Password.szROM,MQTTBuffer,length);
								else
#endif
									length = MQTTWriteString(MQTTClient.Password.szRAM,MQTTBuffer,length);
								}
#if defined(__18CXX)
							}
#endif
						}
                                                    MQTTWrite(MQTTCONNECT,MQTTBuffer,length-5);
                                                    MQTTState=MQTT_CONNECT_ACK;
                                                    MQTTResponseCode=MQTT_SUCCESS;
					//if(MQTTWrite(MQTTCONNECT,MQTTBuffer,length-5)){		// si potrebbe spezzare in 2 per non rifare tutto il "prepare" qua sopra...
                                       // TCPPutArray(MySocket, MQTTBuffer, length-5);
                                       // TCPFlush(MySocket);
                                        //			MQTTState++;
//					lastOutActivity = TickGet();	// già in write
					//MQTTResponseCode=MQTT_SUCCESS;
                                       // }

					}
                                    MQTTStop();
                                    //lastInActivity =TickGet();
				}
			break;
		case MQTT_CONNECT_ACK:
			lastInActivity =TickGet();
                     /*
                    if(TCPIsConnected(MySocket)) {


                        int rec = TCPIsGetReady(MySocket);
                        if ( rec <= 0 )
                        {
                            return;
                        }
                        int length = TCPGetArray(MySocket, &rxBF[bytesRecieved], rec);
                        bytesRecieved += length;
                              */
			while(!MQTTAvailable()) {
				WORD t = TickGet();
//								theApp.PumpMessage();
				if(t-lastInActivity > TICK_SECOND*10) {
					MQTTStop();
					//MQTTState=MQTT_IDLE;
					break;
					}
				}

			//BYTE llen[4];
			WORD len= MQTTReadPacket();
                         
 
//			char myBuf[128];
//			wsprintf(myBuf,"Connect: len=%u, %02X,%02X,%02X,%02X",len,buffer[0],buffer[1],buffer[2],buffer[3]);
//			AfxMessageBox(myBuf);
			if(len >= 2) { //len    bytesRecieved
 				switch(rxBF[1]) {   //MQTTBuffer
					case 0:
						lastInActivity = TickGet();
						MQTTFlags.bits.PingOutstanding = FALSE;
						MQTTClient.bConnected=TRUE;
						break;
					case 1:		// unacceptable protocol version
						MQTTClient.bConnected=FALSE;		// 
						MQTTResponseCode=MQTT_BAD_PROTOCOL;
						break;
					case 2:		// identifier rejected
						MQTTClient.bConnected=FALSE;		// 
						MQTTResponseCode=MQTT_IDENT_REJECTED;
						break;
					case 3:		// server unavailable
						MQTTClient.bConnected=FALSE;		// 
						MQTTResponseCode=MQTT_SERVER_UNAVAILABLE;
						break;
					case 4:		// bad user o password
						MQTTClient.bConnected=FALSE;		// 
						MQTTResponseCode=MQTT_BAD_USER_PASW;
						break;
					case 5:		// unauthorized
#ifdef _DEBUG
						AfxMessageBox("unauthorized");
#endif
						MQTTClient.bConnected=FALSE;		// 
						MQTTResponseCode=MQTT_UNAUTHORIZED;
						break;
					}

                                        MQTTState=MQTT_IDLE;    //go to idle now
				}
                        /*
                    }
                    else{
                         MQTTState=MQTT_HOME;
                    }
                         * */
			break;

		case MQTT_PING:
			MQTTBuffer[0]=MQTTPINGREQ;
			MQTTBuffer[1]=0;
			if(TCPIsPutReady(MySocket) >= 2) {
				MQTTPutArray(MQTTBuffer,2);
				lastOutActivity = TickGet();
				MQTTState=MQTT_IDLE;			// 
				}
			break;

		case MQTT_PING_ACK:					// Pingback, 
			MQTTBuffer[0]=MQTTPINGRESP;
			MQTTBuffer[1]=0;
			if(TCPIsPutReady(MySocket)>=2) {
				MQTTPutArray(MQTTBuffer,2);
				lastOutActivity = TickGet();
				MQTTState=MQTT_IDLE;			// 
				}
			break;

		case MQTT_PUBLISH:	
			//publish				
			if(MQTTConnected()) {
				// Leave room in the buffer for header and variable length field
				WORD length = 5;
#if defined(__18CXX)
				if(MQTTClient.ROMPointers.Topic)
					length = MQTTWriteString(MQTTClient.Topic.szROM, MQTTBuffer,length);
				else
#endif
					length = MQTTWriteString(MQTTClient.Topic.szRAM, MQTTBuffer,length);
				WORD i;
				for(i=0;i<MQTTClient.Plength;i++)
					MQTTBuffer[length++] = MQTTClient.Payload.szRAM[i];		// idem ROM/RAM ..
				BYTE header = MQTTPUBLISH | (MQTTClient.QOS ? (MQTTClient.QOS==2 ? MQTTQOS2 : MQTTQOS1) : MQTTQOS0);
				if(MQTTClient.Retained) 
					header |= 1;

				//if(MQTTWrite(header,MQTTBuffer,length-5))		// si potrebbe spezzare in 2 per non rifare tutto il "prepare" qua sopra...
				MQTTWrite(header,MQTTBuffer,length-5);
                                MQTTState++;
				MQTTResponseCode=MQTT_SUCCESS;

				}
			else
				MQTTResponseCode=MQTT_OPERATION_FAILED;
			break;

		case MQTT_PUBLISH_ACK:				// Publish command accepted (if QOS)
			if(MQTTClient.QOS>0) {			// FINIRE...

				//BYTE llen;
			WORD len= MQTTReadPacket(); //&llen

                        /*
                        int rec = TCPIsGetReady(MySocket);
                        if ( rec <= 0 )
                        {
                                        break;
                        }
                        int length = TCPGetArray(MySocket, rxBF, rec);
                            */

//			char myBuf[128];
//			wsprintf(myBuf,"publish: len=%u, %02X,%02X,%02X,%02X",len,buffer[0],buffer[1],buffer[2],buffer[3]);
//			AfxMessageBox(myBuf);
				if(len >= 2) {		// finire!
					MQTTState=MQTT_IDLE;
					}
				}
			else
				MQTTState=MQTT_IDLE;
			break;

		case MQTT_SUBSCRIBE:	
		//			subscribe(const char *topic, BYTE qos

		// mmm no	m_QOS=qos;
		// ma cmq usiamo lo stesso, per praticità...
			if(/*MQTTClient.QOS < 0 || boh */ MQTTClient.QOS > 1)
				break;

			if(MQTTConnected()) {
				// Leave room in the buffer for header and variable length field
				WORD length = 5;
				nextMsgId++;
				if(nextMsgId == 0) 
					 nextMsgId = 1;

				MQTTBuffer[length++] = HIBYTE(nextMsgId);
				MQTTBuffer[length++] = LOBYTE(nextMsgId);
#if defined(__18CXX)
				if(MQTTClient.ROMPointers.Topic)
					length = MQTTWriteString(MQTTClient.Topic.szROM, MQTTBuffer,length);
				else
#endif
					length = MQTTWriteString(MQTTClient.Topic.szRAM, MQTTBuffer,length);
				MQTTBuffer[length++] = MQTTClient.QOS;

				if(MQTTWrite(MQTTSUBSCRIBE | (MQTTClient.QOS ? (MQTTClient.QOS==2 ? MQTTQOS2 : MQTTQOS1) : MQTTQOS0),MQTTBuffer,length-5))		// si potrebbe spezzare in 2 per non rifare tutto il "prepare" qua sopra...
					MQTTState++;
				MQTTResponseCode=MQTT_SUCCESS;
				}
			else
				MQTTResponseCode=MQTT_OPERATION_FAILED;

			break;

		case MQTT_SUBSCRIBE_ACK:			// Subscribe command accepted (if QOS)
			if(MQTTClient.QOS>0) {			// FINIRE...
				BYTE llen;
				WORD len= MQTTReadPacket(&llen);
   
//			char myBuf[128];
//			wsprintf(myBuf,"subscribe: len=%u, %02X,%02X,%02X,%02X",len,buffer[0],buffer[1],buffer[2],buffer[3]);
//			AfxMessageBox(myBuf);
				if(len==4) {
					MQTTState=MQTT_IDLE;
					}
				}
			else
				MQTTState=MQTT_IDLE;
			break;

		case MQTT_PUBACK:	
		// puback(WORD msgId) 

			if(MQTTConnected()) {
				// Leave room in the buffer for header and variable length field
				WORD length = 5;
				MQTTBuffer[length++] = HIBYTE(MQTTClient.MsgId);
				MQTTBuffer[length++] = LOBYTE(MQTTClient.MsgId);
				if(MQTTWrite(MQTTPUBACK,MQTTBuffer,length-5))
					MQTTState=MQTT_IDLE;

				// però in loop() faceva 			
//			MQTTBuffer[0] = MQTTPUBACK;
//			MQTTBuffer[1] = 2;
//			MQTTBuffer[2] = HIBYTE(id);
//			MQTTBuffer[3] = LOBYTE(id);
//			MQTTPutArray(MQTTBuffer,4);
//			lastOutActivity = t;

				}
			break;

		case MQTT_UNSUBSCRIBE:	
				//unsubscribe(const char *topic) 
			if(MQTTConnected()) {
				WORD length = 5;
				nextMsgId++;
				if(nextMsgId == 0)
					 nextMsgId = 1;
			
				MQTTBuffer[length++] = HIBYTE(nextMsgId);
				MQTTBuffer[length++] = LOBYTE(nextMsgId);
#if defined(__18CXX)
				if(MQTTClient.ROMPointers.Topic)
					length = MQTTWriteString(MQTTClient.Topic.szROM, MQTTBuffer,length);
				else
#endif
					length = MQTTWriteString(MQTTClient.Topic.szRAM, MQTTBuffer,length);
				if(MQTTWrite(MQTTUNSUBSCRIBE | MQTTQOS1,MQTTBuffer,length-5))
					MQTTState++;
				MQTTResponseCode=MQTT_SUCCESS;
				}
			else
				MQTTResponseCode=MQTT_OPERATION_FAILED;
			break;

		case MQTT_UNSUBSCRIBE_ACK:			// Subscribe command accepted (if QOS)
			MQTTState=MQTT_IDLE;
			break;

		case MQTT_DISCONNECT_INIT:
			MQTTState++;
			break;

		case MQTT_DISCONNECT:	
				//disconnect() 
			MQTTBuffer[0] = MQTTDISCONNECT;
			MQTTBuffer[1] = 0;
			if(TCPIsPutReady(MySocket) >= 2) {
				MQTTPutArray(MQTTBuffer,2);
				lastOutActivity = TickGet();
				MQTTState=MQTT_CLOSE;
				MQTTStop();
				}
			break;

		case MQTT_CLOSE:
			// Close the socket so it can be used by other modules
			TCPDisconnect(MySocket);
			MySocket = INVALID_SOCKET;
			MQTTFlags.bits.ConnectedOnce = FALSE;

			// Go back to doing nothing
			MQTTState = MQTT_QUIT;
			break;

		case MQTT_QUIT:	
			if(MySocket != INVALID_SOCKET)
				TCPClose(MySocket);
			MQTTState = MQTT_HOME;
			break;
		case MQTT_IDLE:	
			if(MQTTConnected()) {
				WORD t = TickGet();
				WORD i;
				if(t - LastPingTick >   MQTT_KEEPALIVE_REALTIME*TICK_SECOND) {
					if( !MQTTFlags.bits.PingOutstanding) {
						MQTTState=MQTT_PING;
						MQTTFlags.bits.PingOutstanding = TRUE;
                                                LastPingTick = TickGet();
					 }
					}
				if(MQTTAvailable()) {
					BYTE llen;
					WORD len = MQTTReadPacket(&llen);
					WORD msgId = 0;
					BYTE *payload;

					if(len > 0) {
						lastInActivity = t;
						BYTE type = MQTTBuffer[0] & 0xF0;

						switch(type) {
							case MQTTPUBLISH:
								if(MQTTClient.m_Callback) {
									WORD tl = MAKEWORD(MQTTBuffer[llen+2],MQTTBuffer[llen+1]);
									char *topic=malloc(tl+1);

									for(i=0; i<tl; i++)
										topic[i] = MQTTBuffer[llen+3+i];
									topic[tl] = 0;
									// msgId only present for QOS>0
									if((MQTTBuffer[0] & 0x06) == MQTTQOS1) {
										msgId = MAKEWORD(MQTTBuffer[llen+3+tl+1],MQTTBuffer[llen+3+tl]);
										payload = MQTTBuffer+llen+3+tl+2;
										MQTTClient.m_Callback(topic,payload,len-llen-3-tl-2);

										MQTTPubACK(msgId);
										} 
									else {
										payload = MQTTBuffer+llen+3+tl;
										MQTTClient.m_Callback(topic,payload,len-llen-3-tl);
										}
									free(topic);
									}
								break;
							case MQTTPINGREQ:
								MQTTState=MQTT_PING_ACK;
								break;
							case MQTTPINGRESP:
								MQTTFlags.bits.PingOutstanding = FALSE;
								break;
							}
						}
					}
				}
			break;
	
		}
	}


/*****************************************************************************
  Function:
	BOOL MQTTIsBusy(void)

  Summary:
	Determines if the MQTT client is busy.

  Description:
	Call this function to determine if the MQTT client is busy performing
	background tasks.  This function should be called after any call to 
	MQTTReceiveMail, MQTTPutDone to determine if the stack has finished
	performing its internal tasks.  It should also be called prior to any
	call to MQTTIsPutReady to verify that the MQTT client has not
	prematurely disconnected.  When this function returns FALSE, the next
	call should be to MQTTEndUsage to release the module and obtain the
	status code for the operation.

  Precondition:
	MQTTBeginUsage returned TRUE on a previous call.

  Parameters:
	None

  Return Values:
	TRUE - The MQTT Client is busy with internal tasks or sending an 
		on-the-fly message.
	FALSE - The MQTT Client is terminated and is ready to be released.
  ***************************************************************************/
BOOL MQTTIsBusy(void) {

	return MQTTState != MQTT_HOME;
	}

BOOL MQTTIsIdle(void) {

	return MQTTState != MQTT_IDLE;
	}

/*****************************************************************************
  Function:
	WORD MQTTPutArray(BYTE* Data, WORD Len)

  Description:
	Writes a series of bytes to the MQTT client.

  Precondition:
	MQTTBeginUsage returned TRUE on a previous call.

  Parameters:
	Data - The data to be written
	Len - How many bytes should be written

  Returns:
	The number of bytes written.  If less than Len, then the TX FIFO became
	full before all bytes could be written.
	
  Remarks:
	This function should only be called externally when the MQTT client is
	generating an on-the-fly message.  (That is, MQTTSendMail was called
	with MQTTClient.Body set to NULL.)
	
  Internal:
  ***************************************************************************/
WORD MQTTPutArray(BYTE* Data, WORD Len) {
	WORD result = 0;

        result = TCPPutArray(MySocket, Data, Len);
        TCPFlush(MySocket);

        /*
	while(Len--) {
		if(TCPPut(MySocket,*Data++)) {
			result++;
			}
		else {
			Data--;
			break;
			}
		}
         */

	return result;
	}

/*****************************************************************************
  Function:
	WORD MQTTPutROMArray(ROM BYTE* Data, WORD Len)

  Description:
	Writes a series of bytes from ROM to the MQTT client.

  Precondition:
	MQTTBeginUsage returned TRUE on a previous call.

  Parameters:
	Data - The data to be written
	Len - How many bytes should be written

  Returns:
	The number of bytes written.  If less than Len, then the TX FIFO became
	full before all bytes could be written.
	
  Remarks:
	This function should only be called externally when the MQTT client is
	generating an on-the-fly message.  (That is, MQTTSendMail was called
	with MQTTClient.Body set to NULL.)
	
  	This function is aliased to MQTTPutArray on non-PIC18 platforms.
	
  Internal:
	MQTTPut must be used instead of TCPPutArray because "\r\n." must be
	transparently replaced by "\r\n..".
  ***************************************************************************/
#if defined(__18CXX)
WORD MQTTPutROMArray(ROM BYTE* Data, WORD Len) {
	WORD result = 0;

	while(Len--) {
		if(TCPPut(*Data++)) {
			result++;
			}
		else {
			Data--;
			break;
			}
		}

	return result;
	}
#endif

/*****************************************************************************
  Function:
	WORD MQTTPutString(BYTE* Data)

  Description:
	Writes a string to the MQTT client.

  Precondition:
	MQTTBeginUsage returned TRUE on a previous call.

  Parameters:
	Data - The data to be written

  Returns:
	The number of bytes written.  If less than the length of Data, then the 
	TX FIFO became full before all bytes could be written.
	
  Remarks:
	This function should only be called externally when the MQTT client is
	generating an on-the-fly message.  (That is, MQTTSendMail was called
	with MQTTClient.Body set to NULL.)
	
  Internal:
  ***************************************************************************/
WORD MQTTPutString(BYTE* Data) {
	WORD result = 0;

	while(*Data) {
		if(TCPPut(MySocket,*Data++)) {
			result++;
			}
		else {
			Data--;
			break;
			}
		}

	return result;
	}

/*****************************************************************************
  Function:
	WORD MQTTPutROMString(ROM BYTE* Data)

  Description:
	Writes a string from ROM to the MQTT client.

  Precondition:
	MQTTBeginUsage returned TRUE on a previous call.

  Parameters:
	Data - The data to be written

  Returns:
	The number of bytes written.  If less than the length of Data, then the 
	TX FIFO became full before all bytes could be written.
	
  Remarks:
	This function should only be called externally when the MQTT client is
	generating an on-the-fly message.  (That is, MQTTSendMail was called
	with MQTTClient.Body set to NULL.)
	
  	This function is aliased to MQTTPutString on non-PIC18 platforms.
	
  Internal:
	MQTTPut must be used instead of TCPPutString because "\r\n." must be
	transparently replaced by "\r\n..".
  ***************************************************************************/
#if defined(__18CXX)
WORD MQTTPutROMString(ROM BYTE* Data) {
	WORD result = 0;

	while(*Data) {
		if(TCPPut(MySocket,*Data++)) {
			result++;
			}
		else {
			Data--;
			break;
			}
		}

	return result;
	}
#endif



BOOL MQTTWrite(BYTE header, BYTE *buf, WORD length) {
	BYTE lenBuf[4];
	BYTE llen = 0;
	BYTE digit;
	BYTE pos = 0;
	BYTE rc;
	BYTE len = length;
	int i;

  do {
    digit = len % 128;
    len = len / 128;
    if(len > 0) {
      digit |= 0x80;
      }
    lenBuf[pos++] = digit;
    llen++;
		} while(len>0);

	buf[4-llen] = header;
	for(i=0;i<llen;i++) {
                buf[5-llen+i] = lenBuf[i];
		}
        word txlen = length+1+llen;
	if(TCPIsPutReady(MySocket) >= txlen) {   //length+1+llen
               // word bsent = TCPPutArray(MySocket, buf+(4-llen), txlen);
               // TCPFlush(MySocket);
		rc = MQTTPutArray(buf+(4-llen),txlen);
                lastOutActivity = TickGet();
		return (rc==txlen);
		}
	else
		return 0;
	}

WORD MQTTWriteString(const char *string, BYTE *buf, WORD pos) {
  const char *idp = string;
  WORD i=0;

  pos += 2;
  while (*idp) {
    buf[pos++] = *idp++;
    i++;
		}
	buf[pos-i-2] = HIBYTE(i);
	buf[pos-i-1] = LOBYTE(i);
	return pos;
	}

#if defined(__18CXX)
WORD MQTTWriteROMString(const ROMchar *string, BYTE *buf, WORD pos) {
  const char *idp = string;
  WORD i=0;

  pos += 2;
  while (*idp) {
    buf[pos++] = *idp++;
    i++;
		}
	buf[pos-i-2] = HIBYTE(i);
	buf[pos-i-1] = LOBYTE(i);
	return pos;
	}
#endif

BOOL MQTTConnected(void) {
  BOOL rc;

  rc = MQTTClient.bConnected;
  if(!rc) 
		MQTTStop();

  return rc;
	}

inline BYTE MQTTReadByte(void) {		// ottimizzare, evitare..?
	BYTE ch;

	TCPGet(MySocket,&ch);
	return ch;
	}

WORD MQTTReadPacket() {
	WORD len = 0;
	BOOL isPublish = (MQTTBuffer[0] & 0xF0) == MQTTPUBLISH;
	DWORD multiplier = 1;
	WORD length = 0;
	BYTE digit = 0;
	WORD skip = 0;
	BYTE start = 0;
	WORD i;
	static BYTE m_state=0;
        static BYTE lengthLength[10];

	switch(m_state) {
		case 0:
			if(TCPIsGetReady(MySocket) > 4)				// almeno 4 ci devono essere...
				m_state++;
			//break;
		case 1:
			MQTTBuffer[len++] = MQTTReadByte();
   
			do {
				digit = MQTTReadByte();
				MQTTBuffer[len++] = digit;
				length += (digit & 127) * multiplier;
				multiplier *= 128;
				} while ((digit & 128) != 0);
			lengthLength[0] = len-1;
			m_state++;

		case 2:
			if(isPublish) {
				if(TCPIsGetReady(MySocket) >= 2) {			// almeno 2 ci devono essere...
					// Read in topic length to calculate bytes to skip over for Stream writing
					MQTTBuffer[len++] = MQTTReadByte();
					MQTTBuffer[len++] = MQTTReadByte();
					skip = MAKEWORD(MQTTBuffer[lengthLength[2]],MQTTBuffer[lengthLength[1]]);
					start=2;
					if(MQTTBuffer[0] & MQTTQOS1) {
						// skip message id
						skip += 2;
						}
					m_state=3;
					}
				}
			else
				m_state++;
			break;

		case 3:
			if(TCPIsGetReady(MySocket) >= length) {
				for(i=start; i<length; i++) {
					digit = MQTTReadByte();
					if(MQTTClient.Stream) {
						if(isPublish && len-*lengthLength-2>skip) {
							}
						}
					if(len < MQTT_MAX_PACKET_SIZE) {
						MQTTBuffer[len]=digit;
						}
					len++;
					}
				m_state++;
				}
			//break;

		case 4:
			m_state=0;
			if(!MQTTClient.Stream && len > MQTT_MAX_PACKET_SIZE) {
				len = 0; // This will cause the packet to be ignored.
				}

			MQTTFlags.bits.ReceivedSuccessfully=len>0;		// boh tanto per...

			return len;
			break;
		}
   
	return len;
	}


void MQTTCallback(const char *topic, const BYTE *payload, WORD length) {

	  // handle message arrived - we are only subscribing to one topic so assume all are led related
/*
    BYTE ledOn[] = {0x6F, 0x6E}; // hex for on
    BYTE ledOff[] = {0x6F, 0x66, 0x66}; // hex for off
    BYTE ledFlash[] ={0x66, 0x6C, 0x61, 0x73, 0x68}; // hex for flash

    if (!memcmp(ledOn, payload, sizeof(ledOn)))
        digitalWrite(LED, HIGH);

    if (!memcmp(ledOff, payload, sizeof(ledOff)))
        digitalWrite(LED, LOW);

    if (!memcmp(ledFlash, payload, sizeof(ledFlash))) {
        for (int flashLoop=0;flashLoop < 3; flashLoop++) {
            digitalWrite(LED, HIGH);
            delay(250);
            digitalWrite(LED, LOW);
            delay(250);

        }
    }
		*/
	}


/*****************************************************************************
  Function:
	BOOL MQTTConnect(const char *id, const char *user, const char *pass, const char *willTopic, BYTE willQos, BYTE willRetain, const char *willMessage)

  Summary:
	Connects to a server with given ID etc.

  Description:
	This function starts the state machine that performs the actual
	transmission of the message.  Call this function after all the fields
	in MQTTClient have been set.

  Precondition:
	MQTTBeginUsage returned TRUE on a previous call.

  Parameters:
	None

  Returns:
	None
  ***************************************************************************/
BOOL MQTTConnect(const char *id, const char *user, const char *pass, const char *willTopic, BYTE willQos, BYTE willRetain, const char *willMessage) {

	if(MQTTState==MQTT_IDLE) {
		MQTTClient.ConnectId.szRAM=id;
		MQTTClient.Username.szRAM=user;
		MQTTClient.Password.szRAM=pass;
		MQTTClient.ServerPort=MQTTClient.bSecure ? MQTT_PORT_SECURE : MQTT_PORT;
		MQTTClient.WillTopic.szRAM=pass;
		MQTTClient.WillQOS=willQos;
		MQTTClient.WillRetain=willRetain;
		MQTTClient.WillMessage.szRAM=willMessage;
		MQTTState=MQTT_CONNECT;
		return 1;
		}
	return 0;
	}

/*****************************************************************************
  Function:
	void MQTTPing(void)

  Summary:
	Sends a PING message

  Description:
	This function starts the state machine that performs the actual
	transmission of the message.  Call this function after all the fields
	in MQTTClient have been set.

  Precondition:
	MQTTBeginUsage returned TRUE on a previous call.

  Parameters:
	None

  Returns:
	None
  ***************************************************************************/
BOOL MQTTPing(void) {

	if(MQTTState==MQTT_IDLE) {
		if(MQTTClient.bConnected) {
			MQTTState=MQTT_PING;
			return 1;
			}
		}
	return 0;
	}

/*****************************************************************************
  Function:
	void MQTTPublish(const char *topic, BYTE *payload, WORD plength, BOOL retained)

  Summary:
	Publishes data for a topic

  Description:
	This function starts the state machine that performs the actual
	transmission of the message.  Call this function after all the fields
	in MQTTClient have been set.

  Precondition:
	MQTTBeginUsage returned TRUE on a previous call.

  Parameters:
	None

  Returns:
	None
  ***************************************************************************/
BOOL MQTTPublish(const char *topic, const BYTE *payload, WORD plength, BOOL retained) {

	//solo per ROM ovvero per C30!
	if(MQTTState==MQTT_IDLE) {
		if(MQTTClient.bConnected) {
			MQTTClient.Topic.szRAM=topic;
			MQTTClient.Payload.szRAM=payload;
			MQTTClient.Plength=plength;
			MQTTClient.Retained=retained;
			MQTTState=MQTT_PUBLISH;
			return 1;
			}
		}
	return 0;
	}

/*****************************************************************************
  Function:
	void MQTTPubACK(WORD id)

  Summary:
	Replies to Publish if needed

  Description:
	This function starts the state machine that performs the actual
	transmission of the message.  Call this function after all the fields
	in MQTTClient have been set.

  Precondition:
	MQTTBeginUsage returned TRUE on a previous call.

  Parameters:
	None

  Returns:
	None
  ***************************************************************************/
BOOL MQTTPubACK(WORD id) {

	if(MQTTState==MQTT_IDLE) {
		if(MQTTClient.bConnected) {
			MQTTClient.MsgId=id;			// uso questo, anche per loop()... (v.)
			MQTTState=MQTT_PUBACK;
			return 1;
			}
		}
	return 0;
	}

/*****************************************************************************
  Function:
	void MQTTSubscribe(const char topic, BYTE qos)

  Summary:
	Subscribes to a topic

  Description:
	This function starts the state machine that performs the actual
	transmission of the message.  Call this function after all the fields
	in MQTTClient have been set.

  Precondition:
	MQTTBeginUsage returned TRUE on a previous call.

  Parameters:
	None

  Returns:
	None
  ***************************************************************************/
BOOL MQTTSubscribe(const char *topic, BYTE qos) {

	//solo per ROM ovvero per C30!
	if(MQTTState==MQTT_IDLE) {
		if(MQTTClient.bConnected) {
			MQTTClient.Topic.szRAM=topic;
			MQTTClient.QOS=qos;
			MQTTState=MQTT_SUBSCRIBE;
			return 1;
			}
		}
	return 0;
	}

/*****************************************************************************
  Function:
	void MQTTDisconnect()

  Summary:
	Disconnects gracefully (already done in Close anyway)

  Description:

  Precondition:
	MQTTBeginUsage returned TRUE on a previous call.

  Parameters:
	None

  Returns:
	None
  ***************************************************************************/
BOOL MQTTDisconnect(void) {

	if(MQTTState==MQTT_IDLE) {
		if(MQTTClient.bConnected) {
			MQTTState=MQTT_DISCONNECT;
			return 1;
			}
		}
	return 0;
	}

BOOL MQTTStop(void) {
    //TODO
	return 0;
	}	



char *GetAsJSONValue(char *buf,const char *n,double v) {
	
	sprintf(buf,"{\n  \"d\": {\n    \"Device\": \"PIC\",\n    \"%s\": %4.1f\n    }\n  }\n",n ? n : "value",v);
	return buf;
	}

#endif //#if defined(STACK_USE_MQTT_CLIENT)

