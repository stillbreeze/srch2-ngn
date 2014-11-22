#ifndef __SHARDING_TRANSPORT_MESSAGE_H__
#define __SHARDING_TRANSPORT_MESSAGE_H__

#include "src/sharding/configuration/ShardingConstants.h"
#include "src/sharding/configuration/ConfigManager.h"

namespace srch2 {
namespace httpwrapper {

typedef unsigned int MessageID_t;

const int MSG_HEADER_CONST_SIZE = 17;     // 00010000
const int MSG_HEADER_PADDING_SIZE = 15;

const char MSG_LOCAL_MASK = 0x1;        // 00000001
const char MSG_DISCOVERY_MASK = 0x2;    // 00000010
const char MSG_DP_REQUEST_MASK = 0x4;   // 00000100
const char MSG_DP_REPLY_MASK = 0x8;     // 00001000
const char MSG_SHARDING_MASK = 0x10;     // 00010000

const char MSG_MIGRATION_MASK = 0x40;  //01000000
class Message {

public:

   //helper Functions

   bool isValidMask(){
	   char maskBackup = this->getMask();
	   unsetMask(maskBackup, MSG_LOCAL_MASK);
	   unsetMask(maskBackup, MSG_DISCOVERY_MASK);
	   unsetMask(maskBackup, MSG_DP_REQUEST_MASK);
	   unsetMask(maskBackup, MSG_DP_REPLY_MASK);
	   unsetMask(maskBackup, MSG_SHARDING_MASK);
	   unsetMask(maskBackup, MSG_MIGRATION_MASK);
	   return (maskBackup == 0);
   }

   void unsetMask(char & mask, const char msg_mask) const{
	   mask &= ~(msg_mask);
   }

   bool isSMRelated(){
	   char & mask = this->_getMask();
	 return (mask == 0);
   }

   bool isLocal(){
	   char & mask = this->_getMask();
     return mask & MSG_LOCAL_MASK;
   }
   bool isDPRequest() {
	   char & mask = this->_getMask();
     return mask & MSG_DP_REQUEST_MASK;
   }
   bool isDPReply() {
	   char & mask = this->_getMask();
     return mask & MSG_DP_REPLY_MASK;
   }
   bool isDiscovery() {
	   char & mask = this->_getMask();
     return mask & MSG_DISCOVERY_MASK;
   }

   bool isMigration() {
	   char & mask = this->_getMask();
	   return mask & MSG_MIGRATION_MASK;
   }

   bool isSharding() {
	   char & mask = this->_getMask();
     return mask & MSG_SHARDING_MASK;
   }
   Message * setLocal(){
	   char & mask = this->_getMask();
	   mask |= MSG_LOCAL_MASK;
	   return this;
   }
   Message * setDPRequestMask(){
	   char & mask = this->_getMask();
	   mask |= MSG_DP_REQUEST_MASK;
	   return this;
   }
   Message * setDPReplyMask(){
	   char & mask = this->_getMask();
	   mask |= MSG_DP_REPLY_MASK;
	   return this;
   }
   Message * setDiscoveryMask(){
	   char & mask = this->_getMask();
	   mask |= MSG_DISCOVERY_MASK;
	   return this;
   }

   Message * setMigrationMask(){
	   char & mask = this->_getMask();
	   mask |= MSG_MIGRATION_MASK;
	   return this;
   }

   Message * setShardingMask(){
	   char & mask = this->_getMask();
	   mask |= MSG_SHARDING_MASK;
	   return this;
   }
   unsigned getBodySize(){
	   return this->_getBodySize();
   }
   void setMessageId(MessageID_t id){
	   MessageID_t & idRef = this->_getMessageId();
	   idRef = id;
   }
   MessageID_t getMessageId(){
	   return this->_getMessageId();
   }
   void setRequestMessageId(MessageID_t requestMessageId){
	   MessageID_t & requestMessageIdRef = this->_getReqMessageId();
	   requestMessageIdRef = requestMessageId;
   }
   MessageID_t getRequestMessageId(){
	   return this->_getReqMessageId();
   }
   void setBodyAndBodySize(void * src, unsigned bodySize){
	   setBodySize(bodySize);
	   if(this->_getBodySize() == 0){
		   return;
	   }
	   memcpy(this->getMessageBody(), src, this->_getBodySize());
   }
   ShardingMessageType getType(){
	   return _getShardingMessageType();
   }
   void setType(ShardingMessageType type){
	   ShardingMessageType & typeRef = _getShardingMessageType();
	   typeRef = type;
   }
   void setBodySize(unsigned bodySize){
	   unsigned int & bodySizeRef = _getBodySize();
	   bodySizeRef = bodySize;
   }

   void setMask(char mask) {
	   char & maskRef = _getMask();
	   maskRef = mask;
   }
   char getMask(){return this->_getMask();};
   char * getMessageBody() {
	   return (char *)((char *)this + sizeof(Message));
   }

   static Message * getMessagePointerFromBodyPointer( void * bodyPointer){
	   return (Message *)((char *)bodyPointer - sizeof(Message));
   }
   static char * getBodyPointerFromMessagePointer(Message * messagePointer){
//	   return messagePointer->body;
	   return (char *)((char *)messagePointer + sizeof(Message));
   }

   void populate(char * headerDataStart){
	   memcpy(headerData, headerDataStart, MSG_HEADER_CONST_SIZE);
   }

   inline char * getHeaderInfoStart(){
	   return headerData;
   }

   string getDescription(){
	   return string(getShardingMessageTypeStr(_getShardingMessageType()));
   }


private:
   inline ShardingMessageType & _getShardingMessageType(){
	   return (ShardingMessageType &)(*((ShardingMessageType *)headerData));
   }
   inline char & _getMask(){
	   return (char &)(*((char *)headerData + sizeof(ShardingMessageType)));
   }
   inline unsigned & _getBodySize(){
	   return (unsigned &)(*((unsigned *)headerData + sizeof(ShardingMessageType) + sizeof(char)));
   }
   inline unsigned & _getMessageId(){
	   return (MessageID_t &)(*((MessageID_t *)headerData + sizeof(ShardingMessageType)
			   + sizeof(char) + sizeof(unsigned int)));
   }
   inline unsigned & _getReqMessageId(){
	   return (MessageID_t &)(*((MessageID_t *)headerData + sizeof(ShardingMessageType)
			   + sizeof(char) + sizeof(unsigned int) + sizeof(MessageID_t)));
   }
   // 4 + 1 + 4 + 4 + 4 = 17 = MSG_CONST_SIZE
   char headerData[MSG_HEADER_CONST_SIZE]; // because different machines can have different alignments ...
   char _headerPaddingPlaceHolders[MSG_HEADER_PADDING_SIZE];
   /// now, we can always safely use the Message * cast technique
//   ShardingMessageType shardingMessageType;
//   char mask;
//   unsigned int bodySize;
//   MessageID_t id;
//   MessageID_t requestMessageId; //used by response type messages
//   char body[0];

   /*
    * NOTE : allocate must be like allocate( sizeof(Message) + length of body )
    *        but serialize and deserialize must be only on the headerData array because different
    *        platforms can have different paddings ...
    */
};

}
}

#endif // __SHARDING_ROUTING_MESSAGE_H__
