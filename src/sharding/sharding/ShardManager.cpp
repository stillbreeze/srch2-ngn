#include "ShardManager.h"


#include "ClusterOperationContainer.h"
#include "./metadata_manager/ResourceMetadataManager.h"
#include "./metadata_manager/ResourceLocks.h"
#include "./metadata_manager/Cluster_Writeview.h"
#include "./metadata_manager/Cluster.h"
#include "./metadata_manager/MetadataInitializer.h"
#include "./cluster_operations/ClusterSaveOperation.h"
#include "./cluster_operations/ClusterShutdownOperation.h"
#include "./notifications/Notification.h"
#include "./notifications/NewNodeLockNotification.h"
#include "./notifications/CommitNotification.h"
#include "./notifications/LoadBalancingReport.h"
#include "./notifications/LockingNotification.h"
#include "./notifications/MetadataReport.h"
#include "./notifications/MoveToMeNotification.h"
#include "./notifications/CopyToMeNotification.h"
#include "./node_initialization/NewNodeJoinOperation.h"
#include "./load_balancer/LoadBalancingStartOperation.h"
#include "./load_balancer/ShardMoveOperation.h"
#include "sharding/migration/MigrationManager.h"

#include "core/util/Assert.h"
#include <pthread.h>

namespace srch2is = srch2::instantsearch;
using namespace srch2is;
using namespace std;
namespace srch2 {
namespace httpwrapper {

ShardManager * ShardManager::singleInstance = NULL;

ShardManager * ShardManager::createShardManager(ConfigManager * configManager, ResourceMetadataManager * metadataManager){
	if(singleInstance != NULL){
		return singleInstance;
	}
	// only shard manager must be singleton. ConfigManager must be accessed from shard manager
	singleInstance = new ShardManager(configManager, metadataManager);
	return singleInstance;
}

void ShardManager::deleteShardManager(){
	if(singleInstance == NULL){
		return ;
	}
	delete singleInstance;
	singleInstance = NULL;
	return;
}

ShardManager * ShardManager::getShardManager(){
	if(singleInstance == NULL){
		ASSERT(false);
		return NULL;
	}
	return singleInstance;
}

NodeId ShardManager::getCurrentNodeId(){
	return ShardManager::getWriteview()->currentNodeId;
}
Cluster_Writeview * ShardManager::getWriteview(){
	return ShardManager::getShardManager()->getMetadataManager()->getClusterWriteview();
}
void ShardManager::getReadview(boost::shared_ptr<const ClusterResourceMetadata_Readview> & readview) {
	ShardManager::getShardManager()->getMetadataManager()->getClusterReadView(readview);
}

ShardManager::ShardManager(ConfigManager * configManager,ResourceMetadataManager * metadataManager){

	this->configManager = configManager;
	this->metadataManager = metadataManager;
	this->lockManager = new ResourceLockManager();
	this->stateMachine = new ClusterOperationStateMachine();
	this->joinedFlag = false;
	this->cancelledFlag = false;

}

void ShardManager::attachToTransportManager(TransportManager * tm){
	this->transportManager = tm;
	this->transportManager->registerCallbackForShardingMessageHandler(this);
}

ShardManager::~ShardManager(){
	boost::unique_lock<boost::mutex> bouncedNotificationsLock(shardManagerGlobalMutex);
	setCancelled();
}


TransportManager * ShardManager::getTransportManager() const{
	return transportManager;
}

ConfigManager * ShardManager::getConfigManager() const{
	return configManager;
}

ResourceMetadataManager * ShardManager::getMetadataManager() const{
	return metadataManager;
}
ResourceLockManager * ShardManager::getLockManager() const{
	return lockManager;
}

ClusterOperationStateMachine * ShardManager::getStateMachine() const{
	return this->stateMachine;
}

void ShardManager::initMigrationManager(){
	this->migrationManager = new MigrationManager(transportManager, configManager);
}

void ShardManager::setJoined(){
    joinedFlag = true;
}

bool ShardManager::isJoined() const{
    return joinedFlag;
}

void ShardManager::setCancelled(){
	this->cancelledFlag = true;
}
bool ShardManager::isCancelled() {
	boost::unique_lock<boost::mutex> bouncedNotificationsLock(shardManagerGlobalMutex);
	return this->cancelledFlag;
}

void ShardManager::setLoadBalancing(){
	this->loadBalancingFlag = true;
}
void ShardManager::resetLoadBalancing(){
	this->loadBalancingFlag = false;
}
bool ShardManager::isLoadBalancing() const{
	return this->loadBalancingFlag;
}

void ShardManager::print(){
	metadataManager->print();

	lockManager->print();

	stateMachine->print();

	// bounced notifications
	if(bouncedNotifications.size() == 0){
		return;
	}
	cout << "**************************************************************************************************" << endl;
	cout << "Bounced notifications' source addresses : " ;
	if(bouncedNotifications.size() == 0 ){
		cout << "empty." << endl;
	}else{
		cout << endl;
	}
	cout << "**************************************************************************************************" << endl;
	for(unsigned i = 0; i < bouncedNotifications.size(); ++i){
		const ShardingNotification * notif = bouncedNotifications.at(i);
		cout << notif->getDest().toString() << endl;
	}

}

void ShardManager::start(){
	unsigned numberOfNodes = this->metadataManager->getClusterWriteview()->nodes.size();
	if(numberOfNodes == 1){ // we are the first node:
		// assign primary shards to this node :
		MetadataInitializer nodeInitializer(configManager, this->metadataManager);
		nodeInitializer.initializeCluster();
		this->getMetadataManager()->commitClusterMetadata();
		this->setJoined();

		Logger::info("Cluster is ready to accept new nodes. Current node ID : %d", ShardManager::getCurrentNodeId());
		//TODO remove
		print();
	}else{
		// commit the readview to be accessed by readers until we join
		this->getMetadataManager()->commitClusterMetadata();
		Logger::info("Joining the existing cluster ...");
		//TODO remove
		print();

		// we must join an existing cluster :
		NewNodeJoinOperation * joinOperation = new NewNodeJoinOperation();
		stateMachine->registerOperation(joinOperation);
	}
	pthread_t loadBalancingThread;
    if (pthread_create(&loadBalancingThread, NULL, ShardManager::periodicWork , NULL) != 0){
        //        Logger::console("Cannot create thread for handling local message");
        perror("Cannot create thread for load balancing.");
        return;
    }
    pthread_detach(loadBalancingThread);
}

/*
 * Saves the cluster metadata and indices ...
 */
void ShardManager::save(evhttp_request *req){
	if(this->metadataManager->getClusterWriteview() == NULL){
		 bmhelper_evhttp_send_reply2(req, HTTP_BADREQUEST, "Save failed.",
		                    "Cluster is not ready to save yet.");
	}


    /* Yes, we are expecting a post request */
    switch (req->type) {
    case EVHTTP_REQ_PUT: {
    	ClusterSaveOperation * saveOperation = new ClusterSaveOperation(req);
    	this->stateMachine->registerOperation(saveOperation);
        break;
    }
    default: {
        bmhelper_evhttp_send_reply2(req, HTTP_BADREQUEST, "INVALID REQUEST",
                "{\"error\":\"The request has an invalid or missing argument. See Srch2 API documentation for details.\"}");
        Logger::error(
                "The request has an invalid or missing argument. See Srch2 API documentation for details");
        break;
    }
    };
}

/*
 * Shuts the cluster down.
 * In this process, we first save all the indices and the cluster metadata and then shut the entire cluster down.
 */
void ShardManager::shutdown(evhttp_request *req){
	boost::unique_lock<boost::mutex> shardManagerGlobalLock(shardManagerGlobalMutex);
    switch (req->type) {
    case EVHTTP_REQ_PUT: {
    	ClusterShutdownOperation * shutdownOperation = new ClusterShutdownOperation();
    	this->stateMachine->registerOperation(shutdownOperation);
		bmhelper_evhttp_send_reply2(req, HTTP_OK, "OK",
				"{\"message\":\"Shutting down the cluster...\"}\n");
        break;
    }
    default: {
        bmhelper_evhttp_send_reply2(req, HTTP_BADREQUEST, "INVALID REQUEST",
                "{\"error\":\"The request has an invalid or missing argument. See Srch2 API documentation for details.\"}");
        Logger::error(
                "The request has an invalid or missing argument. See Srch2 API documentation for details");
        break;
    }
    };
}

void ShardManager::_shutdown(){
	Logger::console("Shutting down the instance ...");
	raise(SIGTERM);
//    kill(getpid(),SIGTERM);
//	exit(0);
	//TODO
}

// sends this sharding notification to destination using TM
bool ShardManager::send(ShardingNotification * notification){
	if(notification == NULL){
		ASSERT(false);
		return false;
	}
	if(notification->getDest().nodeId == getCurrentNodeId()){
		ASSERT(false);
		return true;
	}
	unsigned notificationByteSize = notification->getNumberOfBytes();
	void * bodyByteArray = transportManager->getMessageAllocator()->allocateMessageReturnBody(notificationByteSize);
	notification->serialize(bodyByteArray);
	Message * notificationMessage = Message::getMessagePointerFromBodyPointer(bodyByteArray);
	notificationMessage->setShardingMask();
	notificationMessage->setMessageId(transportManager->getUniqueMessageIdValue());
	notificationMessage->setType(notification->messageType());

	Logger::debug("%s | Sending [Type: %s, ID: %d] ... ", notification->getDescription().c_str(),
			notificationMessage->getDescription().c_str(), notificationMessage->getMessageId());

	transportManager->sendMessage(notification->getDest().nodeId , notificationMessage, 0);
	transportManager->getMessageAllocator()->deallocateByMessagePointer(notificationMessage);

	// currently TM always send the message
	return true;
}

bool ShardManager::resolveMessage(Message * msg, NodeId senderNode){
	if(msg == NULL){
		return false;
	}

	// Global mutex lock which will last as long as we are in this scope.
	boost::unique_lock<boost::mutex> shardManagerGlobalLock(shardManagerGlobalMutex);


	Cluster_Writeview * writeview = ShardManager::getWriteview();

	// if a node fails, we don't accept any more messages from it.
	if(writeview->nodes.find(senderNode) != writeview->nodes.end() &&
			writeview->nodes[senderNode].first == ShardingNodeStateFailed){
		Logger::debug("!!! Warning: Message with type %s was", msg->getDescription().c_str());
		Logger::debug("ignore because src was failed before.!!!");
		return true;
	}
	Logger::debug("SHM | Type :  %s , MsgID : %d . Going to be processed ...", msg->getDescription().c_str() , msg->getMessageId());
	switch (msg->getType()) {
		case ShardingNewNodeLockMessageType:
		{
			NewNodeLockNotification * newNodeLockNotification =
					ShardingNotification::deserializeAndConstruct<NewNodeLockNotification>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | Batch of size %d, %s",
					newNodeLockNotification->getDescription().c_str(),
					newNodeLockNotification->getLockRequest()->requestBatch.size(),
					newNodeLockNotification->getLockRequest()->isBlocking ? "blocking." : "non-blocking.");
			if(newNodeLockNotification->isBounced()){
				saveBouncedNotification(newNodeLockNotification);
				Logger::debug("==> Bounced.");
				break;
			}
			if(! isJoined()){
				bounceNotification(newNodeLockNotification);
				Logger::debug("==> Not joined yet. Bounced.");
				break;
			}
			this->lockManager->resolve(newNodeLockNotification);
			delete newNodeLockNotification;
			break;
		}
		case ShardingNewNodeLockACKMessageType:
		{
			NewNodeLockNotification::ACK * newNodeLockAck =
					ShardingNotification::deserializeAndConstruct<NewNodeLockNotification::ACK>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | .", newNodeLockAck->getDescription().c_str());
			if(newNodeLockAck->isBounced()){
				Logger::debug("==> Bounced.");
				ASSERT(false);
				delete newNodeLockAck;
				break;
			}
			stateMachine->handle(newNodeLockAck);
			delete newNodeLockAck;
			break;
		}
		case ShardingMoveToMeMessageType:
		{
			MoveToMeNotification * moveNotif  =
					ShardingNotification::deserializeAndConstruct<MoveToMeNotification>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | Shard : %s", moveNotif->getDescription().c_str(),
					moveNotif->getDescription().c_str(), moveNotif->getShardId().toString().c_str());
			if(moveNotif->isBounced()){
				Logger::debug("==> Bounced.");
				ASSERT(false);
				delete moveNotif;
				break;
			}

			Cluster_Writeview * writeview = this->getWriteview();

			if(writeview->localClusterDataShards.find(moveNotif->getShardId()) == writeview->localClusterDataShards.end()){
				// NOTE : requested shard does not exist on this node
				ASSERT(false);
				break;
			}
			// call migration manager to transfer this shard
			this->migrationManager->migrateShard(moveNotif->getShardId(),
					writeview->localClusterDataShards.at(moveNotif->getShardId()).server, moveNotif->getShardId(),moveNotif->getDest(), moveNotif->getSrc());

			delete moveNotif;
			break;
		}
		case ShardingMoveToMeStartMessageType:
		{
			MoveToMeNotification::START * moveNotif  =
					ShardingNotification::deserializeAndConstruct<MoveToMeNotification::START>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | Shard : %s", moveNotif->getDescription().c_str() ,
					moveNotif->getShardId().toString().c_str());
			if(moveNotif->isBounced()){
				Logger::debug("==> Bounced.");
				saveBouncedNotification(moveNotif);
				break;
			}
			if(! isJoined()){
				bounceNotification(moveNotif);
				Logger::debug("==> Not joined yet. Bounced.");
				break;
			}
			this->stateMachine->registerOperation(new ShardMoveSrcOperation(moveNotif->getSrc(), moveNotif->getShardId()));
			delete moveNotif;
			break;
		}
		case ShardingMoveToMeACKMessageType:
		{
			MoveToMeNotification::ACK * moveAckNotif  =
					ShardingNotification::deserializeAndConstruct<MoveToMeNotification::ACK>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | .", moveAckNotif->getDescription().c_str());
			if(moveAckNotif->isBounced()){
				Logger::debug("==> Bounced.");
				ASSERT(false);
				delete moveAckNotif;
				break;
			}

			this->stateMachine->handle(moveAckNotif);
			delete moveAckNotif;
			return true;
		}
		case ShardingMoveToMeAbortMessageType:
		{
			MoveToMeNotification::ABORT * moveAbortNotif  =
					ShardingNotification::deserializeAndConstruct<MoveToMeNotification::ABORT>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | .", moveAbortNotif->getDescription().c_str());
			if(moveAbortNotif->isBounced()){
				Logger::debug("==> Bounced.");
				ASSERT(false);
				delete moveAbortNotif;
				break;
			}

			this->stateMachine->handle(moveAbortNotif);
			delete moveAbortNotif;
			break;
		}
		case ShardingMoveToMeFinishMessageType:
		{
			MoveToMeNotification::FINISH * moveFinishNotif  =
					ShardingNotification::deserializeAndConstruct<MoveToMeNotification::FINISH>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | .", moveFinishNotif->getDescription().c_str());
			if(moveFinishNotif->isBounced()){
				Logger::debug("==> Bounced.");
				ASSERT(false);
				delete moveFinishNotif;
				break;
			}

			this->stateMachine->handle(moveFinishNotif);
			delete moveFinishNotif;
			break;
		}
		case ShardingNewNodeReadMetadataRequestMessageType:
		{
			MetadataReport::REQUEST * readNotif =
					ShardingNotification::deserializeAndConstruct<MetadataReport::REQUEST>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | .", readNotif->getDescription().c_str());
			if(readNotif->isBounced()){
				saveBouncedNotification(readNotif);
				Logger::debug("==> Bounced.");
				break;
			}
			if(! isJoined()){
				bounceNotification(readNotif);
				Logger::debug("==> Not joined yet. Bounced.");
				break;
			}

			metadataManager->resolve(readNotif);
			delete readNotif;
			break;
		}
		case ShardingNewNodeReadMetadataReplyMessageType:
		{
			MetadataReport * readAckNotif =
					ShardingNotification::deserializeAndConstruct<MetadataReport>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | .", readAckNotif->getDescription().c_str());
			if(readAckNotif->isBounced()){
				Logger::debug("==> Bounced.");
				ASSERT(false);
				delete readAckNotif;
				break;
			}

			stateMachine->handle(readAckNotif);
			delete readAckNotif;
			break;
		}
		case ShardingLockMessageType:
		{
			LockingNotification * lockNotif =
					ShardingNotification::deserializeAndConstruct<LockingNotification>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | Batch of size %d, %s",
					lockNotif->getDescription().c_str(),
					lockNotif->getLockRequest()->requestBatch.size(),
					lockNotif->getLockRequest()->isBlocking ? "blocking." : "non-blocking.");
			if(lockNotif->isBounced()){
				saveBouncedNotification(lockNotif);
				Logger::debug("==> Bounced.");
				break;
			}
			if(! isJoined()){
				bounceNotification(lockNotif);
				Logger::debug("==> Not joined yet. Bounced.");
				break;
			}

			lockManager->resolve(lockNotif);
			delete lockNotif;
			break;
		}
		case ShardingLockACKMessageType:
		{
			LockingNotification::ACK * lockAckNotif =
					ShardingNotification::deserializeAndConstruct<LockingNotification::ACK>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | ACK : %d", lockAckNotif->getDescription().c_str(), lockAckNotif->isGranted());
			if(lockAckNotif->isBounced()){
				Logger::debug("==> Bounced.");
				ASSERT(false);
				delete lockAckNotif;
				break;
			}

			stateMachine->handle(lockAckNotif);
			delete lockAckNotif;
			break;
		}
		case ShardingLockRVReleasedMessageType:
		{
			ASSERT(false);
			break;
		}
		case ShardingLoadBalancingReportMessageType:
		{
			LoadBalancingReport * loadBalancingReportNotif =
					ShardingNotification::deserializeAndConstruct<LoadBalancingReport>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | Load : %d", loadBalancingReportNotif->getDescription().c_str(), loadBalancingReportNotif->getLoad());
			if(loadBalancingReportNotif->isBounced()){
				Logger::debug("==> Bounced.");
				ASSERT(false);
				delete loadBalancingReportNotif;
				break;
			}

			stateMachine->handle(loadBalancingReportNotif);
			delete loadBalancingReportNotif;
			break;
		}
		case ShardingLoadBalancingReportRequestMessageType:
		{
			LoadBalancingReport::REQUEST * loadBalancingReqNotif =
					ShardingNotification::deserializeAndConstruct<LoadBalancingReport::REQUEST>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | .", loadBalancingReqNotif->getDescription().c_str());
			if(loadBalancingReqNotif->isBounced()){
				saveBouncedNotification(loadBalancingReqNotif);
				Logger::debug("==> Bounced.");
				break;
			}
			if(! isJoined()){
				bounceNotification(loadBalancingReqNotif);
				Logger::debug("==> Not joined yet. Bounced.");
				break;
			}

			LoadBalancingReport * report = new LoadBalancingReport(writeview->getLocalNodeTotalLoad());
			report->setDest(loadBalancingReqNotif->getSrc());
			report->setSrc(NodeOperationId(ShardManager::getCurrentNodeId()));
			send(report);
			delete report;
			delete loadBalancingReqNotif;
			break;
		}
		case ShardingCopyToMeMessageType:
		{

			CopyToMeNotification * copyNotif =
					ShardingNotification::deserializeAndConstruct<CopyToMeNotification>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | cp %s %s", copyNotif->getDescription().c_str(),
					copyNotif->getReplicaShardId().toString().c_str(),
					copyNotif->getUnassignedShardId().toString().c_str());
			if(copyNotif->isBounced()){
				saveBouncedNotification(copyNotif);
				Logger::debug("==> Bounced.");
				break;
			}
			if(! isJoined()){
				bounceNotification(copyNotif);
				Logger::debug("==> Not joined yet. Bounced.");
				break;
			}

			// call migration manager to start transfering this shard.
			ClusterShardId replicaShardId = copyNotif->getReplicaShardId();
			ClusterShardId unassignedShardId = copyNotif->getUnassignedShardId();
			Cluster_Writeview * writeview = this->getWriteview();
			if(writeview->localClusterDataShards.find(replicaShardId) == writeview->localClusterDataShards.end()){
				// NOTE: requested shard does not exist on this node.
				ASSERT(false);
				break;
			}

			this->migrationManager->migrateShard(replicaShardId, writeview->localClusterDataShards.at(replicaShardId).server, unassignedShardId,
					copyNotif->getDest(), copyNotif->getSrc());

			delete copyNotif;
			break;
		}
		case ShardingCommitMessageType:
		{
			CommitNotification * commitNotif =
					ShardingNotification::deserializeAndConstruct<CommitNotification>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | Change : %s", commitNotif->getDescription().c_str(),
					commitNotif->getMetadataChange()->toNameString().c_str());
			if(commitNotif->isBounced()){
				saveBouncedNotification(commitNotif);
				Logger::debug("==> Bounced.");
				break;
			}
			if(! isJoined()){
				bounceNotification(commitNotif);
				Logger::debug("==> Not joined yet. Bounced.");
				break;
			}

			metadataManager->resolve(commitNotif);
			delete commitNotif;
			break;
		}
		case ShardingCommitACKMessageType:
		{
			CommitNotification::ACK * commitAckNotif =
					ShardingNotification::deserializeAndConstruct<CommitNotification::ACK>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | .", commitAckNotif->getDescription().c_str());
			if(commitAckNotif->isBounced()){
				Logger::debug("==> Bounced.");
				ASSERT(false);
				delete commitAckNotif;
				break;
			}

			stateMachine->handle(commitAckNotif);
			delete commitAckNotif;
			break;
		}
		case ShardingSaveDataMessageType:
		{
			SaveDataNotification * saveDataNotif =
					ShardingNotification::deserializeAndConstruct<SaveDataNotification>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | .", saveDataNotif->getDescription().c_str());
			if(saveDataNotif->isBounced()){
				saveBouncedNotification(saveDataNotif);
				Logger::debug("==> Bounced.");
				break;
			}
			if(! isJoined()){
				bounceNotification(saveDataNotif);
				Logger::debug("==> Not joined yet. Bounced.");
				break;
			}

			this->resolve(saveDataNotif);
			delete saveDataNotif;
			break;
		}
		case ShardingSaveDataACKMessageType:
		{
			SaveDataNotification::ACK * saveDataNotif =
					ShardingNotification::deserializeAndConstruct<SaveDataNotification::ACK>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | .", saveDataNotif->getDescription().c_str());
			if(saveDataNotif->isBounced()){
				Logger::debug("==> Bounced.");
				ASSERT(false);
				delete saveDataNotif;
				break;
			}

			this->stateMachine->handle(saveDataNotif);
			delete saveDataNotif;
			break;
		}
		case ShardingSaveMetadataMessageType:
		{
			SaveMetadataNotification * saveMetadataNotif =
					ShardingNotification::deserializeAndConstruct<SaveMetadataNotification>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | .", saveMetadataNotif->getDescription().c_str());
			if(saveMetadataNotif->isBounced()){
				saveBouncedNotification(saveMetadataNotif);
				Logger::debug("==> Bounced.");
				break;
			}
			if(! isJoined()){
				bounceNotification(saveMetadataNotif);
				Logger::debug("==> Not joined yet. Bounced.");
				break;
			}

			this->resolve(saveMetadataNotif);
			delete saveMetadataNotif;
			break;
		}
		case ShardingSaveMetadataACKMessageType:
		{
			SaveMetadataNotification::ACK * saveMetadataNotif =
					ShardingNotification::deserializeAndConstruct<SaveMetadataNotification::ACK>(Message::getBodyPointerFromMessagePointer(msg));
			Logger::debug("%s | .", saveMetadataNotif->getDescription().c_str());
			if(saveMetadataNotif->isBounced()){
				Logger::debug("==> Bounced.");
				ASSERT(false);
				delete saveMetadataNotif;
				break;
			}

			this->stateMachine->handle(saveMetadataNotif);
			delete saveMetadataNotif;
			break;
		}
		case ShardingMergeMessageType:
		{
			MergeNotification * mergeNotification =
					ShardingNotification::deserializeAndConstruct<MergeNotification>(Message::getBodyPointerFromMessagePointer(msg));
			if(mergeNotification->isBounced()){
				saveBouncedNotification(mergeNotification);
				Logger::debug("==> Bounced.");
				break;
			}
			if(! isJoined()){
				bounceNotification(mergeNotification);
				Logger::debug("==> Not joined yet. Bounced.");
				break;
			}
			this->resolve(mergeNotification);
			delete mergeNotification;
			break;
		}
		case ShardingMergeACKMessageType:
		{
			MergeNotification::ACK * mergeNotification =
					ShardingNotification::deserializeAndConstruct<MergeNotification::ACK>(Message::getBodyPointerFromMessagePointer(msg));
			if(mergeNotification->isBounced()){
				Logger::debug("==> Bounced.");
				ASSERT(false);
				delete mergeNotification;
				break;
			}

			this->stateMachine->handle(mergeNotification);
			delete mergeNotification;
			break;
		}
		case ShardingShutdownMessageType:
		{
			ShutdownNotification * shutdownNotification =
					ShardingNotification::deserializeAndConstruct<ShutdownNotification>(Message::getBodyPointerFromMessagePointer(msg));
			if(shutdownNotification->isBounced()){
				saveBouncedNotification(shutdownNotification);
				Logger::debug("==> Bounced.");
				break;
			}
			if(! isJoined()){
				bounceNotification(shutdownNotification);
				Logger::debug("==> Not joined yet. Bounced.");
				break;
			}
			this->_shutdown();
			break;
		}
		default:
			break;
	}

	Logger::debug("SHM | Type :  %s , MsgID : %d . Processed.", msg->getDescription().c_str() , msg->getMessageId());
//    cout << "Shard Manager status after handling message:" << endl;
//    ShardManager::getShardManager()->print();
//    cout << "======================================================================" << endl;

	return true;
}

void * ShardManager::resolveReadviewRelease_ThreadChange(void * vidPtr){
	unsigned metadataVersion = *(unsigned *)vidPtr;
	delete (unsigned *)vidPtr;
	Logger::debug("DP | Metadata release VID=%d", metadataVersion);
	LockingNotification::RV_RELEASED * rvReleased = new LockingNotification::RV_RELEASED(metadataVersion);
	ShardManager::getShardManager()->getLockManager()->resolve(rvReleased);
	delete rvReleased;
	Logger::debug("DP | Metadata release VID=%d processed.", metadataVersion);

//    cout << "Shard Manager status after receiving RV release, vid = " << metadataVersion << endl;
//    ShardManager::getShardManager()->print();
//    cout << "======================================================================" << endl;

	return NULL;
}

void ShardManager::resolveMMNotification(const ShardMigrationStatus & migrationStatus){

	Logger::debug("MM | (%d => %d) was %s", migrationStatus.sourceNodeId, migrationStatus.destinationNodeId,
			(migrationStatus.status == MM_STATUS_SUCCESS)? "Done."  : "Failed.");
	boost::unique_lock<boost::mutex> shardManagerGlobalLock(shardManagerGlobalMutex);
	// TODO : second argument must be deleted when migration manager is merged with this code.
	// TODO :  Migration manager must return the ClusterShardId value

	MMNotification * mmNotif = new MMNotification(migrationStatus);
	if(mmNotif->getSrc().nodeId == ShardManager::getCurrentNodeId()){
		ASSERT(mmNotif->getDest().nodeId != ShardManager::getCurrentNodeId());
		// We are the sender of this shard migration, we need to swap src/dest before
		// passing it to stateMachine because the routing of state machine is based on the destination
		// operation id.
		NodeOperationId tmp = mmNotif->getSrc();
		mmNotif->setSrc(mmNotif->getDest());
		mmNotif->setDest(tmp);
	}//else{
	// we are the received of the shard, no need to make any changes to the notification
	//}
	this->stateMachine->handle(mmNotif);
	Logger::debug("MM | (%d => %d) was %s Processed.", migrationStatus.sourceNodeId, migrationStatus.destinationNodeId,
			(migrationStatus.status == MM_STATUS_SUCCESS)? "Done."  : "Failed.");
	delete mmNotif;

//    cout << "Shard Manager status after receiving migration manager notification:" << endl;
//    ShardManager::getShardManager()->print();
//    cout << "======================================================================" << endl;
}

void ShardManager::resolveSMNodeArrival(const Node & newNode){
	Logger::debug("SM | Node %d arrival.", newNode.getId());
    boost::unique_lock<boost::mutex> shardManagerGlobalLock(shardManagerGlobalMutex);
    Cluster_Writeview * writeview = this->getWriteview();
    writeview->addNode(newNode);
	Logger::debug("SM | Node %d arrival. Processed.", newNode.getId());
//    cout << "Shard Manager status after arrival of node " << newNode.getId() << ":" << endl;
//    ShardManager::getShardManager()->print();
//    cout << "======================================================================" << endl;
}

void ShardManager::resolveSMNodeFailure(const NodeId failedNodeId){
	Logger::debug("Node %d failure.", failedNodeId);
	boost::unique_lock<boost::mutex> shardManagerGlobalLock(shardManagerGlobalMutex);
	NodeFailureNotification * nodeFailureNotif = new NodeFailureNotification(failedNodeId);
	// 1. metadata manager
	this->metadataManager->resolve(nodeFailureNotif);
	// 2. lock manager
	this->lockManager->resolve(nodeFailureNotif);
	// 3. state machine
	this->stateMachine->handle(nodeFailureNotif);
	Logger::debug("Node %d failure. Processed.", failedNodeId);

}

void ShardManager::resolve(SaveDataNotification * saveDataNotif){
	// Move on all local shards and save them on the disk.
	Cluster_Writeview * writeview = this->getWriteview();
	for(map<ClusterShardId, LocalPhysicalShard >::iterator localClusterShardItr = writeview->localClusterDataShards.begin();
			localClusterShardItr != writeview->localClusterDataShards.end(); ++localClusterShardItr){
		localClusterShardItr->second.server->getIndexer()->save();
	}

	for(map<unsigned,  LocalPhysicalShard >::iterator localNodeShardItr = writeview->localNodeDataShards.begin();
			localNodeShardItr != writeview->localNodeDataShards.end(); ++localNodeShardItr){
		localNodeShardItr->second.server->getIndexer()->save();
	}

	// reply ack
	SaveDataNotification::ACK * ack = new SaveDataNotification::ACK();
	ack->setSrc(NodeOperationId(ShardManager::getCurrentNodeId()));
	ack->setDest(saveDataNotif->getSrc());
	if(ack->getDest().nodeId == ShardManager::getCurrentNodeId()){
		this->stateMachine->handle(ack);
	}else{
		send(ack);
	}
	delete ack;
}

void ShardManager::resolve(SaveMetadataNotification * saveDataNotif){

	this->metadataManager->resolve(this->getConfigManager(), saveDataNotif);

	// reply ack
	SaveMetadataNotification::ACK * ack = new SaveMetadataNotification::ACK();
	ack->setSrc(NodeOperationId(ShardManager::getCurrentNodeId()));
	ack->setDest(saveDataNotif->getSrc());
	if(ack->getDest().nodeId == ShardManager::getCurrentNodeId()){
		this->stateMachine->handle(ack);
	}else{
		send(ack);
	}
	delete ack;
}

void ShardManager::resolve(MergeNotification * mergeNotification){
	//TODO : merge can be unsuccessful. What should we do in that case ?
	// Move on all local shards and merge them.
	Cluster_Writeview * writeview = this->getWriteview();
	for(map<ClusterShardId, LocalPhysicalShard >::iterator localClusterShardItr = writeview->localClusterDataShards.begin();
			localClusterShardItr != writeview->localClusterDataShards.end(); ++localClusterShardItr){
		localClusterShardItr->second.server->getIndexer()->merge();
	}

	for(map<unsigned,  LocalPhysicalShard >::iterator localNodeShardItr = writeview->localNodeDataShards.begin();
			localNodeShardItr != writeview->localNodeDataShards.end(); ++localNodeShardItr){
		localNodeShardItr->second.server->getIndexer()->merge();
	}


	// reply ack
	MergeNotification::ACK * ack = new MergeNotification::ACK();
	ack->setSrc(NodeOperationId(ShardManager::getCurrentNodeId()));
	ack->setDest(mergeNotification->getSrc());
	if(ack->getDest().nodeId == ShardManager::getCurrentNodeId()){
		stateMachine->handle(ack);
	}else{
		send(ack);
	}
	delete ack;
}



void * ShardManager::periodicWork(void *args) {

	while(! ShardManager::getShardManager()->isCancelled()){

		/*
		 * 1. Resend bounced notifications.
		 * 2. is we are joined, start load balancing.
		 */
		//
		sleep(2);

		boost::unique_lock<boost::mutex> shardManagerGlobalLock(ShardManager::getShardManager()->shardManagerGlobalMutex);

		// 1. Resend bounced notifications.
		for(unsigned i = 0 ; i < ShardManager::getShardManager()->bouncedNotifications.size() ; ++i){
			ShardingNotification * notif = ShardManager::getShardManager()->bouncedNotifications.at(i);
			ShardManager::getShardManager()->send(notif);
		}
		ShardManager::getShardManager()->bouncedNotifications.clear();


		// 2. if we are joined, start load balancing.
		if(ShardManager::getShardManager()->isJoined() && ! ShardManager::getShardManager()->isLoadBalancing()){
			ShardManager::getShardManager()->setLoadBalancing();
			ShardManager::getShardManager()->stateMachine->registerOperation(new LoadBalancingStartOperation());
		}

		// TODO remove
		cout << "===========================================================================================" << endl;
		cout << "===========================================================================================" << endl;
		cout << "===========================================================================================" << endl;
		cout << "===========================================================================================" << endl;
		cout << "===========================================================================================" << endl;
	    ShardManager::getShardManager()->print();
	}
	return NULL;
}

void ShardManager::saveBouncedNotification(ShardingNotification * notif){
	notif->resetBounced();
	notif->swapSrcDest();
	bouncedNotifications.push_back(notif);
}

void ShardManager::bounceNotification(ShardingNotification * notif){
	notif->setBounced();
	notif->swapSrcDest();
	send(notif);
	delete notif;
}

}
}

