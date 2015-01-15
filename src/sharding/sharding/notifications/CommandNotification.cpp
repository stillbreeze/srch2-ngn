#include "CommandNotification.h"


#include "sharding/configuration/ShardingConstants.h"
#include "core/util/SerializationHelper.h"
#include "sharding/transport/MessageAllocator.h"
#include "server/HTTPJsonResponse.h"
#include "Notification.h"
#include "core/util/Assert.h"
#include "core/util/Logger.h"
#include "sharding/processor/DistributedProcessorInternal.h"
#include "../ShardManager.h"


using namespace std;
namespace srch2 {
namespace httpwrapper {

CommandNotification::CommandNotification(boost::shared_ptr<const ClusterResourceMetadata_Readview> clusterReadview,
		const NodeTargetShardInfo & target, ShardCommandCode commandCode, const string & filePath ){
	this->target = target;
	this->commandCode = commandCode;
	if(this->commandCode == ShardCommandCode_Export){
		jsonFilePath = filePath;
	}
	if(this->commandCode == ShardCommandCode_ResetLogger){
		newLogFilePath = filePath;
	}
	this->clusterReadview = clusterReadview;
}
CommandNotification::CommandNotification(){
	// for deserialization
	this->commandCode = ShardCommandCode_Merge;
	ShardManager::getReadview(clusterReadview);
}

CommandNotification::~CommandNotification(){}

bool CommandNotification::resolveNotification(SP(ShardingNotification) notif){
	SP(CommandStatusNotification) response =
			ShardManager::getShardManager()->getDPInternal()->resolveShardCommand(boost::dynamic_pointer_cast<CommandNotification>(notif));
	if(! response){
		response = create<CommandStatusNotification>();
	}
    response->setSrc(notif->getDest());
    response->setDest(notif->getSrc());
	send(response);
	return true;
}

ShardingMessageType CommandNotification::messageType() const{
	return ShardingShardCommandMessageType;
}
void * CommandNotification::serializeBody(void * buffer) const{
	buffer = target.serialize(buffer);
	buffer = srch2::util::serializeFixedTypes((uint32_t)commandCode, buffer);
	if(commandCode == ShardCommandCode_Export){
		buffer = srch2::util::serializeString(jsonFilePath, buffer);
	}
	if(commandCode == ShardCommandCode_ResetLogger){
		buffer = srch2::util::serializeString(newLogFilePath, buffer);
	}
	return buffer;
}
unsigned CommandNotification::getNumberOfBytesBody() const{
	unsigned numberOfBytes = 0;
	numberOfBytes += target.getNumberOfBytes();
	numberOfBytes += srch2::util::getNumberOfBytesFixedTypes((uint32_t)commandCode);
	if(commandCode == ShardCommandCode_Export){
		numberOfBytes += srch2::util::getNumberOfBytesString(jsonFilePath);
	}
	if(commandCode == ShardCommandCode_ResetLogger){
		numberOfBytes += srch2::util::getNumberOfBytesString(newLogFilePath);
	}
	return numberOfBytes;
}
void * CommandNotification::deserializeBody(void * buffer) {
	buffer = target.deserialize(buffer);
	uint32_t intVar = 0;
	buffer = srch2::util::deserializeFixedTypes(buffer, intVar);
	commandCode = (ShardCommandCode)intVar;
	if(commandCode == ShardCommandCode_Export){
		buffer = srch2::util::deserializeString(buffer, jsonFilePath);
	}
	if(commandCode == ShardCommandCode_ResetLogger){
		buffer = srch2::util::deserializeString(buffer, newLogFilePath);
	}
	return buffer;
}

ShardCommandCode CommandNotification::getCommandCode() const{
	return commandCode;
}

string CommandNotification::getJsonFilePath() const{
	return jsonFilePath;
}

string CommandNotification::getNewLogFilePath() const{
	return newLogFilePath;
}

NodeTargetShardInfo CommandNotification::getTarget(){
	return target;
}

boost::shared_ptr<const ClusterResourceMetadata_Readview> CommandNotification::getReadview() const{
	return clusterReadview;
}

}
}