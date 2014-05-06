#ifndef __TRANSPORT_MANAGER_H__
#define  __TRANSPORT_MANAGER_H__

#include "RouteMap.h"
#include <event.h>
#include<pthread.h>
#include "Message.h"
#include "MessageAllocator.h"
#include "PendingMessages.h"
#include "routing/InternalMessageBroker.h"


#include "CallbackHandler.h"

namespace srch2 {
namespace httpwrapper {

typedef std::vector<event_base*> EventBases;
typedef std::vector<Node> Nodes;

class TransportManager {
public:


	TransportManager(EventBases&, Nodes&);

	//third argument is a timeout in seconds
	MessageTime_t route(NodeId, Message*, unsigned=0, CallbackReference=CallbackReference());
	CallbackReference registerCallback(void*,Callback*,
			ShardingMessageType,bool,int = 1);
	void registerCallbackHandlerForSynchronizeManager(CallBackHandler*);




	inline MessageTime_t getDistributedTime() const ;

	inline CallBackHandler* getInternalTrampoline() const ;

	inline pthread_t getListeningThread() const ;

	inline MessageAllocator * getMessageAllocator() const ;

	inline PendingMessages * getMsgs() const ;

	inline RouteMap * getRouteMap() const ;

	inline CallBackHandler* getSmHandler() const ;


private:
	/*
	 * This member maps nodes to their sockets
	 */
	RouteMap routeMap;

	/*
	 * The thread that the current node is listening for the incoming request from new nodes
	 */
	pthread_t listeningThread;

	/*
	 * The current distributed time of system
	 * It is synchronized in cb_recieveMessage(int fd, short eventType, void *arg)
	 */
	MessageTime_t distributedTime;

	/*
	 * The allocator used for messaging
	 */
	MessageAllocator messageAllocator;

	/*
	 * the data structure which stores all pending messages in this node
	 */
	PendingMessages pendingMessages;

	/*
	 * Handles SynchManager callbacks
	 */
	CallBackHandler *synchManagerHandler;

	/*
	 * Handles internal message broker callbacks
	 */
	CallBackHandler *internalMessageBrokerHandler;
};

inline void TransportManager::registerCallbackHandlerForSynchronizeManager(CallBackHandler
		*callBackHandler) {
	synchManagerHandler = callBackHandler;
}

inline CallbackReference TransportManager::registerCallback(void* obj,
		Callback* cb, ShardingMessageType type,bool all,int shards) {
	return pendingMessages.registerCallback(obj, cb, type, all, shards);
}

}}

#endif /* __TRANSPORT_MANAGER_H__ */
