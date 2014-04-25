#ifndef __SHARDING_PROCESSOR_SERIALIZABLE_INSERT_UPDATE_COMMAND_INPUT_H_
#define __SHARDING_PROCESSOR_SERIALIZABLE_INSERT_UPDATE_COMMAND_INPUT_H_

namespace srch2is = srch2::instantsearch;
using namespace std;

#include "sharding/configuration/ShardingConstants.h"
#include <instantsearch/Record.h>

namespace srch2 {
namespace httpwrapper {


struct SerializableInsertUpdateCommandInput{
	bool insertOrUpdate; // true => insert, false=> update
	srch2is::Record * record;

    //serializes the object to a byte array and places array into the region
    //allocated by given allocator
    void* serialize(std::allocator<char>);

    //given a byte stream recreate the original object
    const SerializableInsertUpdateCommandInput& deserialize(void*);

    //Returns the type of message which uses this kind of object as transport
    ShardingMessageType messsageKind();
};


}
}

#endif // __SHARDING_PROCESSOR_SERIALIZABLE_INSERT_UPDATE_COMMAND_INPUT_H_
