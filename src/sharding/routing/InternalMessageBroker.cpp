#include "InternalMessageBroker.h"
#include "processor/serializables/SerializableSearchCommandInput.h"
#include "processor/serializables/SerializableSearchCommandInput.h"
#include "processor/serializables/SerializableSearchResults.h"
#include "processor/serializables/SerializableInsertUpdateCommandInput.h"
#include "processor/serializables/SerializableDeleteCommandInput.h"
#include "processor/serializables/SerializableCommandStatus.h"
#include "processor/serializables/SerializableSerializeCommandInput.h"
#include "processor/serializables/SerializableResetLogCommandInput.h"
#include "processor/serializables/SerializableCommitCommandInput.h"
#include "processor/serializables/SerializableGetInfoCommandInput.h"
#include "processor/serializables/SerializableGetInfoResults.h"
#include "transport/Message.h"

namespace srch2is = srch2::instantsearch;
using namespace std;

using namespace srch2::httpwrapper;


template<typename InputType, typename Deserializer, typename OutputType>
Message* InternalMessageBroker::broker(Message *msg, Srch2Server* server,
		OutputType (DPInternalRequestHandler::*internalDPRequestHandlerFunction) (Srch2Server*, InputType*)) {

	// TODO : currently nothing is local and the local messaging part doesn't work ...
	InputType *inputSerializedObject = (msg->isLocal()) ? (InputType*) msg->buffer
			: (InputType*) &Deserializer::deserialize((void*) msg->buffer);

	OutputType outputSerializedObject((internalDP.*internalDPRequestHandlerFunction)(server, inputSerializedObject));
	// prepare the byte stream of reply
	void *reply = (msg->isLocal()) ? (void*) inputSerializedObject
			: inputSerializedObject->serialize(getMessageAllocator());

	if(!msg->isLocal())
		delete inputSerializedObject, outputSerializedObject;
  
  return (Message*) reply - sizeof(Message);
}

Message* InternalMessageBroker::notify(Message * message){
	if(message == NULL){
		return NULL;
	}

	Srch2Server* server = getShardIndex(message->shard);
	if(server == NULL){
		//TODO : what if message shardID is not present in the map?
		// example : message is late and shard is not present anymore ....
		return NULL;
	}

	//1. Deserialize message and get command input 	object
	//2. Call the appropriate internal DP function and get the response object
	//3. Serialize response object into a message
	//4. give the new message out
	switch (message->type) {
	case SearchCommandMessageType: // -> for LogicalPlan object
		return broker<SerializableSearchCommandInput, SerializableSearchCommandInput,SerializableSearchResults>
										(message, server, &DPInternalRequestHandler::internalSearchCommand);
	case InsertUpdateCommandMessageType: // -> for Record object (used for insert and update)
		/*broker<SerializableInsertUpdateCommandInput, SerializableInsertUpdateCommandInput, SerializableCommandStatus>
		 	 	 	 	 	 	 	 	(message, server, &DPInternalRequestHandler::internalInsertUpdateCommand);*/
		// TODO : we also need to pass schema for insert/update
		break;
	case DeleteCommandMessageType: // -> for DeleteCommandInput object (used for delete)
		return broker<SerializableDeleteCommandInput, SerializableDeleteCommandInput, SerializableCommandStatus>
										(message, server, &DPInternalRequestHandler::internalDeleteCommand);
	case SerializeCommandMessageType: // -> for SerializeCommandInput object
		// (used for serializing index and records)
    /*
		broker<SerializableSerializeCommandInput, SerializableSerializeCommandInput, SerializableCommandStatus>
										(message, server, &DPInternalRequestHandler::internalSerializeCommand);*/
		break;
	case GetInfoCommandMessageType: // -> for GetInfoCommandInput object (used for getInfo)
		//TODO: needs versionInfo

//		broker<SerializableGetInfoCommandInput, SerializableGetInfoCommandInput, SerializableGetInfoResults>
//										(message, server, &DPInternalRequestHandler::internalGetInfoCommand);
		break;
	case CommitCommandMessageType: // -> for CommitCommandInput object
		return broker<SerializableCommitCommandInput, SerializableCommitCommandInput, SerializableCommandStatus>
										(message, server, &DPInternalRequestHandler::internalCommitCommand);
	case ResetLogCommandMessageType: // -> for ResetLogCommandInput (used for resetting log)
		return broker<SerializableResetLogCommandInput,SerializableResetLogCommandInput, SerializableCommandStatus>
										(message, server, &DPInternalRequestHandler::internalResetLogCommand);
	case SearchResultsMessageType: // -> for SerializedQueryResults object
	case GetInfoResultsMessageType: // -> for GetInfoResults object
	case StatusMessageType: // -> for CommandStatus object (object returned from insert, delete, update)
  default:
		// These message types are only used for reponses to other requests and code should
		// never reach to this point
    return NULL;
	}
  return NULL;
}

Srch2Server * InternalMessageBroker::getShardIndex(ShardId & shardId){
	return routingManager.getShardIndex(shardId);
}

MessageAllocator * InternalMessageBroker::getMessageAllocator() {
	// return routingManager.transportManager.getMessageAllocator();
	return routingManager.getMessageAllocator();
}

void InternalMessageBroker::sendReply(Message* msg, void* replyObject) {
	// get shardID out of message
	ShardId shardId = msg->shard;
	//
	//TODO
}
