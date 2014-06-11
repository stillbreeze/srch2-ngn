// author RJ/Surendra/Jamshid

#ifndef __TRANSPORT_MANAGER_H__
#define  __TRANSPORT_MANAGER_H__

#include "RouteMap.h"
#include <event.h>
#include <pthread.h>
#include "Message.h"
#include "MessageAllocator.h"
#include "CallbackHandler.h"
#include <boost/thread.hpp>
namespace srch2 {
namespace httpwrapper {

typedef std::vector<event_base*> EventBases;
typedef std::vector<Node> Nodes;
class TransportManager ;

/*
 *  This structure holds the required information by the libevent callbacks
 *  1. TM poninter
 *  2. Connection Pointer
 *  3. Event Pointer
 *  4. even_base Pointer
 */

struct TransportCallback {
	TransportManager *const tm;
	Connection *const conn;
	struct event* eventPtr;
	const struct event_base *const base;

	TransportCallback(TransportManager *tm, Connection *c, event* e,
			event_base* b) : tm(tm), conn(c), eventPtr(e), base(b) {}
	TransportCallback() : tm(NULL), conn(NULL), eventPtr(NULL), base(NULL) {}
};

class RoutingManager;

class TransportManager {
public:
	TransportManager(EventBases&, Nodes&);
	//third argument is a timeout in seconds
	MessageID_t sendMessage(NodeId, Message *, unsigned timeout=0);
	//route message through a particular socket
	MessageID_t _sendMessage(int fd, Message *);
	// this API enables SM to register its callback with TM
	void registerCallbackHandlerForSynchronizeManager(CallBackHandler*);
	// get the value of maximum message Id for this node.
	MessageID_t& getCurrentMessageId();
	// generate a unique ID for current message
	MessageID_t getUniqueMessageIdValue();
	// this API enables RM to register message Broker with TM
	void setInternalMessageBroker(CallBackHandler*);
	// getter function for current listening thread. Temp for V0
	pthread_t getListeningThread() const;
	// getter function for message Allocator object.
	MessageAllocator * getMessageAllocator();
	// get RM object
	RoutingManager * getRoutingManager();
	// this API enables RM to register its pointer with TM
	void setRoutingManager(RoutingManager * rm);
	// returns a data structure which holds all node-node socket informations.
	RouteMap * getRouteMap();
	// get SM callback handler
	CallBackHandler* getSmHandler();
	// get RM callback handler
	CallBackHandler* getRmHandler();
	~TransportManager();
	// API for the libevent callback to call into TM
	bool receiveMessage(int fd, TransportCallback *cb);
private:
	/*
	 *  The function dispatches messages to upstream handlers.
	 */
   void * notifyUpstreamHandlers(Message *msg, int fd, NodeId  nodeId);
   /*
    *  This is a simple low level function which reads the data from the supplied socket descriptor
    *  and fills it into the buffer. It also returns the total number of bytes read.
    *
    *  Return status :
    *
    *  0  : success
    *  1  : Partial data read
    *  -1 : error
    */
   int readDataFromSocket(int fd, char *buffer, const int byteToRead, int *byteReadCount);
   /*
    * This function reads a fixed size header from the socket stream. Each message starts
    * with a message header and then followed by message body.
    *
    * --------------------------------
    * | Message Header | Rest of Body |
    * ---------------------------------
    */
   int readMessageHeader(const Message *message,  int fd);
   /*
    *   The function reads the message body which follows the message header.
    */
   int readMessageBody(int fd, MessageBuffer &readBuffer);

   int checkSocketIsReadyForRead(int fd) {
	   return checkSocketIsReady(fd, true);
   }
   int checkSocketIsReadyForWrite(int fd) {
	   return checkSocketIsReady(fd, false);
   }
   /*
    *  use select system call to check whether the socket is ready for
    *  read or write.
    *  checkForRead = false -> check whether socket is ready for write
    *  checkForRead = true -> check whether socket is ready for read
    */
   int checkSocketIsReady(int socket, bool checkForRead);

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
	MessageID_t distributedUniqueId;

	/*
	 * The allocator used for messaging
	 */
	MessageAllocator messageAllocator;

	/*
	 * Handles SynchManager callbacks
	 */
	CallBackHandler *synchManagerHandler;

	/*
	 * Handles internal message broker callbacks
	 */
	CallBackHandler *routeManagerHandler;

	/*
	 *  Stores the default socket read buffer size
	 */
	unsigned socketReadBuffer;
	/*
	 *  Stores the default socket write buffer size
	 */
	unsigned socketSendBuffer;

	/*
	 * Routing Manager
	 */
	RoutingManager * routingManager;
};
}}

#endif /* __TRANSPORT_MANAGER_H__ */
