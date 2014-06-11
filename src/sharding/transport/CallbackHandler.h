// authors : RJ
#ifndef __CALLBACK_HANDLER_H__
#define __CALLBACK_HANDLER_H__

#include "sharding/transport/Message.h"

namespace srch2 {
namespace httpwrapper {

/*
 * this class is an abstraction for callback
 * currently used in SM and internal message broker.
 * This structure also allows us to initialize SM and RM after TM.
 *
 */
class CallBackHandler {
public:
	virtual std::pair<Message*,void*> notifyWithReply(Message *msg) = 0;
	virtual void notifyNoReply(Message * msg) = 0;
	virtual ~CallBackHandler() {}
};


}
}

#endif /* __CALLBACK_H__ */
