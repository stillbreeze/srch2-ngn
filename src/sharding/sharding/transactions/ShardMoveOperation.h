#ifndef __SHARDING_SHARDING_SHARD_MOVE_OPERATION_H__
#define __SHARDING_SHARDING_SHARD_MOVE_OPERATION_H__

#include "./AtomicLock.h"
#include "./AtomicRelease.h"
#include "./AtomicMetadataCommit.h"
#include "../metadata_manager/Shard.h"
#include "../metadata_manager/Cluster_Writeview.h"
#include "../state_machine/State.h"
#include "../notifications/Notification.h"
#include "../notifications/MoveToMeNotification.h"
#include "../notifications/CommitNotification.h"
#include "../notifications/LockingNotification.h"

namespace srch2is = srch2::instantsearch;
using namespace srch2is;
using namespace std;
namespace srch2 {
namespace httpwrapper {


class ShardMoveOperation : public ProducerInterface, public NodeIteratorListenerInterface{
public:

	ShardMoveOperation(const NodeId & srcNodeId, const ClusterShardId & moveShardId, ConsumerInterface * consumer);

	~ShardMoveOperation();

	SP(Transaction) getTransaction() ;

	void produce();


	void lock();
	void consume(bool granted);
	// **** If lock granted
	void transfer();

	// for transfer
	void consume(const ShardMigrationStatus & status);

	// **** If transfer was successful
	void commit();
	// decide to do the cleanup
    // **** end if

	// **** end if
	void release();

	void end(map<NodeId, SP(ShardingNotification) > & replies);

	void finalize();
	string getName() const {return "shard-move";};

private:

	enum CurrentOperation{
		PreStart,
		Lock,
		Transfer,
		Commit,
		Release,
		Cleanup
	};
	const ClusterShardId shardId;
	NodeOperationId currentOpId; // used to be able to release locks, and also talk with MM
	NodeOperationId srcAddress;

	CurrentOperation currentOp;

	bool successFlag;

	bool transferAckReceived;
	ShardMigrationStatus transferStatus;

	LocalPhysicalShard physicalShard;

	AtomicLock * locker;
	SP(MoveToMeNotification) moveToMeNotif ;
	AtomicMetadataCommit * committer;
	AtomicRelease * releaser;
	SP(MoveToMeNotification::CleanUp) cleaupNotif;

};

}
}


#endif // __SHARDING_SHARDING_SHARD_MOVE_OPERATION_H__
