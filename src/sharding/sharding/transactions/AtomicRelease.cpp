#include "AtomicRelease.h"

#include "core/util/SerializationHelper.h"
#include "core/util/Assert.h"
#include "../metadata_manager/Shard.h"
#include "../metadata_manager/Node.h"
#include "../metadata_manager/Cluster_Writeview.h"
#include "../ShardManager.h"
#include "../state_machine/StateMachine.h"

#include <sstream>

namespace srch2is = srch2::instantsearch;
using namespace srch2is;
using namespace std;
namespace srch2 {
namespace httpwrapper {

/// copy
AtomicRelease::AtomicRelease(const ClusterShardId & srcShardId,
		const ClusterShardId & destShardId,
		const NodeOperationId & copyAgent,
		ConsumerInterface * consumer) : ProducerInterface(consumer){
	ASSERT(this->getTransaction());
	// prepare the locker and locking notification
	this->releaseNotification = SP(LockingNotification)(new LockingNotification(srcShardId, destShardId, copyAgent, true));
	this->lockType = LockRequestType_Copy;
	this->finalizeFlag = false;
	init();
}

/// node arrival
AtomicRelease::AtomicRelease(const NodeOperationId & newNodeOpId,
		ConsumerInterface * consumer): ProducerInterface(consumer){ // releases the metadata
	ASSERT(this->getTransaction());
	// prepare the locker and locking notification
	this->releaseNotification = SP(LockingNotification)(new LockingNotification(newNodeOpId, vector<NodeId>(),
			LockLevel_X, true, true));
	// LockLevel_X is just a place holder,
	this->lockType = LockRequestType_Metadata;
	this->finalizeFlag = false;
	init();
}

/// record releasing
AtomicRelease::AtomicRelease(const vector<string> & primaryKeys, const NodeOperationId & writerAgent, const ClusterPID & pid,
		ConsumerInterface * consumer): ProducerInterface(consumer){
	ASSERT(this->getTransaction());
	// prepare the locker and locking notification
	this->releaseNotification = SP(LockingNotification)(new LockingNotification(primaryKeys, writerAgent, pid, true));
	this->lockType = LockRequestType_PrimaryKey;
	this->pid = pid;
	this->finalizeFlag = false;
	init();
}


/// general purpose cluster shard releasing
AtomicRelease::AtomicRelease(const ClusterShardId & shardId, const NodeOperationId & agent,
		ConsumerInterface * consumer): ProducerInterface(consumer){
	ASSERT(this->getTransaction());
	// prepare the locker and locking notification
	this->releaseNotification = SP(LockingNotification)(new LockingNotification(shardId, agent));
	this->lockType = LockRequestType_GeneralPurpose;
	this->finalizeFlag = false;
	init();
}


AtomicRelease::~AtomicRelease(){
    if(releaser != NULL){
        delete releaser;
    }
}

SP(Transaction) AtomicRelease::getTransaction(){
	if(this->getConsumer() == NULL){
		return SP(Transaction)();
	}
	return this->getConsumer()->getTransaction();
}

void AtomicRelease::produce(){
    Logger::sharding(Logger::Detail, "AtomicRelease| starts. Consumer is %s",
    		this->getConsumer() == NULL ? "NULL" : this->getConsumer()->getName().c_str());
    if(releaser == NULL){
        releaser = new OrderedNodeIteratorOperation(releaseNotification, ShardingLockACKMessageType , participants, this);
    }

    SP(const ClusterNodes_Writeview) nodesWriteview = ShardManager::getNodesWriteview_read();
    bool participantsChangedFlag = false;
    for(int nodeIdx = 0; nodeIdx < participants.size(); ++nodeIdx){
    	if(! nodesWriteview->isNodeAlive(participants.at(nodeIdx))){
    		participants.erase(participants.begin() + nodeIdx);
    		nodeIdx --;
    		participantsChangedFlag = true;
    	}
    }
    nodesWriteview.reset();
    if(participants.empty()){
        Logger::sharding(Logger::Detail, "AtomicLock| ends unattached, no participant found.");
    	return;
    }else if(participantsChangedFlag){
    	releaser->setParticipants(participants);
    }

    ShardManager::getShardManager()->getStateMachine()->registerOperation(releaser);
    releaser = NULL;
}

void AtomicRelease::end(map<NodeId, SP(ShardingNotification) > & replies){
	finalize();
}

void AtomicRelease::finalize(){
    Logger::sharding(Logger::Detail, "AtomicRelease| ends ...");
	this->finalizeFlag = true;

	if(lockType == LockRequestType_PrimaryKey){
		this->getConsumer()->consume(true, pid);
	}else{
		this->getConsumer()->consume(true);
	}
}

void AtomicRelease::setParticipants(vector<NodeId> & participants){
	if(releaser == NULL){
	    releaser = new OrderedNodeIteratorOperation(releaseNotification, ShardingLockACKMessageType , participants, this);
	}
	releaser->setParticipants(participants);
}

bool AtomicRelease::updateParticipants(){
	releaseNotification->getInvolvedNodes(this->getTransaction(), participants);
	if(participants.empty()){
		return false;
	}
	if(releaser == NULL){
	    releaser = new OrderedNodeIteratorOperation(releaseNotification, ShardingLockACKMessageType , participants, this);
	}
	releaser->setParticipants(participants);
	return true;
}

void AtomicRelease::init(){
	// participants are all node
	releaseNotification->getInvolvedNodes(this->getTransaction(), participants);
    releaser = NULL;
}


}
}
