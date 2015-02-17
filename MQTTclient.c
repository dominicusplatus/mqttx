/*********************************************************************
 *
 *  MQTT Client Demonstrations
 *	  - MQTT Client (publish, subscribe)
 *
 *********************************************************************
 * FileName:        MQTTClient.c
 * Dependencies:    TCP/IP stack
 * Processor:       PIC32
 * Compiler:        Microchip C32 v1.05 or higher
 *
 * Author               Date      Comment
 *~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * E. Wood     			4/26/08	  Moved from MainDemo.c
 * D. Greggio  			9/30/14	  wrote example
 * D. Guz 			20/01/15  Fix, cleanup, testing
 ********************************************************************/

#define __MQTTDEMO_C

#include "TCPIPConfig.h"

#if defined(STACK_USE_MQTT_CLIENT)

#include "MQTTclient.h"
#include "typedefs.h"
#include "TCPIP Stack/TCPIP.h"

 static bool RequestPending = 0;
 static byte PendingRequests = 0;

#define REQ_TIMEOUT_CYCLES  100

 static word RequestTimeoutCounter = 0;
 
 static char DefaultUsername[20] = "";
 static char DefaultPassword[20] = "";
 static char DefaultDeviceId[20] = "";

 static BYTE ServerName[30] =	"";
 static byte ServerPasswd[30] = "";

 #define Mqtt_Request_DevId_Pos 0x00
 #define Mqtt_Request_DevId_Len 0x01
 #define Mqtt_Request_ServerAddr_Pos (Mqtt_Request_DevId_Pos+Mqtt_Request_DevId_Len)
 #define Mqtt_Request_ServerAddr_Len 0x01
 #define Mqtt_Request_TopicName_Pos (Mqtt_Request_ServerAddr_Pos+Mqtt_Request_ServerAddr_Len)
 #define Mqtt_Request_TopicName_Len 0x01
 #define Mqtt_Request_MsgBuff_Pos   (Mqtt_Request_TopicName_Pos+Mqtt_Request_TopicName_Len)
 #define Mqtt_Request_MsgBuff_Len   0x01
 #define Mqtt_Request_Username_Pos   (Mqtt_Request_MsgBuff_Pos+Mqtt_Request_MsgBuff_Len)
 #define Mqtt_Request_Username_Len   0x01
 #define Mqtt_Request_Password_Pos   (Mqtt_Request_Username_Pos+Mqtt_Request_Username_Len)
 #define Mqtt_Request_Password_Len   0x01
 #define Mqtt_Request_Size          (Mqtt_Request_Password_Pos+Mqtt_Request_Password_Len)
 #define Mqtt_Max_Client_Requests 10

 byte* MqttClientRequests[Mqtt_Max_Client_Requests][Mqtt_Request_Size];
 void (*MqttClientRequestCallbacks[Mqtt_Max_Client_Requests])();

void MqttSendTestPacket(){
    RequestPending = 1;
}

void MqttSetDefaultCred(byte* Username, byte* Password, byte* DeviceId){

}

/***********    Send message without credentials    ************/
void MqttQueueMsg(byte* Msg, byte* Topic, byte* Id, byte* serverAddr){
    if (PendingRequests < Mqtt_Max_Client_Requests){
        MqttClientRequests[PendingRequests][Mqtt_Request_DevId_Pos] = Id;
        MqttClientRequests[PendingRequests][Mqtt_Request_ServerAddr_Pos] = serverAddr;
        MqttClientRequests[PendingRequests][Mqtt_Request_TopicName_Pos] = Topic;
        MqttClientRequests[PendingRequests][Mqtt_Request_MsgBuff_Pos] = Msg;
        PendingRequests++;
    }
}

void MqttQueueMsgWithCred(byte* Msg, byte* Topic, byte* Id, byte* Username, byte* Password, byte* serverAddr){
    if (PendingRequests < Mqtt_Max_Client_Requests){
    MqttQueueMsg(Msg, Topic, Id, serverAddr);
    SetCredForRequest( Username, Password, PendingRequests-1);
    }
}

void MqttDequeueCurrentRequest(){
    byte reqinc = 0;
    for (reqinc =0; reqinc <PendingRequests-1; reqinc++){
        memcpy(&MqttClientRequests[reqinc],&MqttClientRequests[reqinc+1],Mqtt_Request_Size*4);
    }
    memset(&MqttClientRequests[PendingRequests-1],0x00,Mqtt_Request_Size*4); //clear last
    RequestPending = 0;
    PendingRequests--;
}

void SetCredForRequest(byte* Username, byte* Password, word ReqId){
    MqttClientRequests[ReqId][Mqtt_Request_Username_Pos] = Username;
    MqttClientRequests[ReqId][Mqtt_Request_Password_Pos] = Password;
}


void MqttSendSampleIbmPublishVarWithName(byte* varname, double val ){
    char msgBF[64];
    GetAsJSONValue(msgBF,varname,val);
    MqttQueueMsgWithCred(msgBF,"iot-2/evt/temperature/fmt/json","d:wo8faz:sconnGKP:00-04-A3-00-00-40","use-token-auth","m0ru(ULj91Qn4EhsZA","wo8faz.messaging.internetofthings.ibmcloud.com");
}

void MqttSendSampleGnatPublishMsgForTopic(byte* val,  byte* topic ){
    MqttQueueMsgWithCred(val,topic,"sconnGKP40","user",ServerPasswd,ServerName);
}

void MqttClientInit(){
     byte* servbuff = ipcGetHostServerHostname();
     memcpy(ServerName,servbuff,strlen(servbuff));

     byte* passbuff = ipcGetHostServerPasswd();
     memcpy(ServerPasswd,passbuff,strlen(passbuff));
}

void MqttSetPublishRxCallback(){
    
}



/*****************************************************************************
  Function:
	void MQTTDemo(void)

  Summary:
	Synchronous state machine for currently executed request.

  Precondition:
	The MQTT client is initialized.

  Parameters:
	None

  Returns:
  	None
  ***************************************************************************/
void MQTTClientTask(void) {
	static enum	{
		MQTT_HOME = 0,
		MQTT_BEGIN,
		MQTT_CONNECT,
		MQTT_CONNECT_WAIT,
		MQTT_PUBLISH,
		MQTT_PUBLISH_WAIT,
		MQTT_FINISHING,
		MQTT_DONE
		} MQTTState = MQTT_HOME;

	static DWORD WaitTime;

	switch(MQTTState)	{
		case MQTT_HOME:
		  if(PendingRequests > 0)	{
                                RequestTimeoutCounter = 0;
				MQTTState++;
                    }
			break;
		case MQTT_BEGIN:
			if(MQTTBeginUsage()) {
                            RequestPending = 0; //clear request
				MQTTClient.Server.szRAM = MqttClientRequests[0][Mqtt_Request_ServerAddr_Pos];
                                MQTTClient.ServerPort = 1883;
				MQTTClient.ConnectId.szRAM = MqttClientRequests[0][Mqtt_Request_DevId_Pos];
				MQTTClient.Username.szRAM = MqttClientRequests[0][Mqtt_Request_Username_Pos];
                                MQTTClient.Password.szRAM = MqttClientRequests[0][Mqtt_Request_Password_Pos];
				MQTTClient.bSecure=FALSE;
                               // MQTTClient.m_Callback = callback;
				MQTTClient.QOS=0;
				MQTTClient.KeepAlive=MQTT_KEEPALIVE_LONG;
                                MQTTClient.Topic.szRAM =  MqttClientRequests[0][Mqtt_Request_TopicName_Pos];
                                MQTTClient.Payload.szRAM =  MqttClientRequests[0][Mqtt_Request_MsgBuff_Pos];
				//  MQTTClient.Stream = stream;
				MQTTState++;
				}
                        else{
                            RequestTimeoutCounter++;
                            if ( RequestTimeoutCounter > REQ_TIMEOUT_CYCLES )
                                MQTTState = MQTT_DONE;
                        }
			break;
		case MQTT_CONNECT:
			MQTTConnect(MQTTClient.ConnectId.szRAM,MQTTClient.Username.szRAM,MQTTClient.Password.szRAM,
				NULL,0,0,NULL);
			MQTTState++;
			break;

		case MQTT_CONNECT_WAIT:
			if(MQTTConnected())
                        {
                                            MQTTState++;
                        }
                        else
                        {
                            RequestTimeoutCounter++;
                            if ( RequestTimeoutCounter > REQ_TIMEOUT_CYCLES )
                                MQTTState = MQTT_DONE;
                        }
			break;

		case MQTT_PUBLISH:
			MQTTPublish(MQTTClient.Topic.szRAM,MQTTClient.Payload.szRAM,strlen(MQTTClient.Payload.szRAM),0);
			MQTTState++;
			break;

		case MQTT_PUBLISH_WAIT:
			if(MQTTIsIdle()) {
				if(MQTTResponseCode == MQTT_SUCCESS)
					MQTTState=MQTT_FINISHING;
				else
					MQTTState=MQTT_FINISHING;
			}
                        else{
                            RequestTimeoutCounter++;
                            if ( RequestTimeoutCounter > REQ_TIMEOUT_CYCLES )
                                MQTTState = MQTT_DONE;
                        }
			break;

		case MQTT_FINISHING:
			if(!MQTTIsBusy())	{
				MQTTState++;
         
			}
                        else{
                            RequestTimeoutCounter++;
                            if ( RequestTimeoutCounter > REQ_TIMEOUT_CYCLES )
                                MQTTState = MQTT_DONE;
                        }
			break;

		case MQTT_DONE:
                        MQTTEndUsage();
			MQTTState = MQTT_HOME;
                        MqttDequeueCurrentRequest();
			break;
		}
	}


#endif //#if defined(STACK_USE_MQTT_CLIENT)
