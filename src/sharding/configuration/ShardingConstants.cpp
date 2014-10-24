
#include "ShardingConstants.h"

namespace srch2 {
namespace httpwrapper {

const char * getShardingMessageTypeStr(ShardingMessageType shardingMessageType){
	   switch (shardingMessageType) {
	   case ShardingMoveToMeMessageType:
		   return "ShardingMoveToMeMessageType";
	   case ShardingMoveToMeACKMessageType:
		   return "ShardingMoveToMeACKMessageType";
	   case ShardingMoveToMeCleanupMessageType:
		   return "ShardingMoveToMeCleanupMessageType";
	   case ShardingNewNodeReadMetadataRequestMessageType:
		   return "ShardingNewNodeReadMetadataRequestMessageType";
	   case ShardingNewNodeReadMetadataReplyMessageType:
		   return "ShardingNewNodeReadMetadataReplyMessageType";
	   case ShardingLockMessageType:
		   return "ShardingLockMessageType";
	   case ShardingLockACKMessageType:
		   return "ShardingLockACKMessageType";
	   case ShardingLockRVReleasedMessageType:
		   return "ShardingLockACKMessageType";
	   case ShardingLoadBalancingReportMessageType:
		   return "ShardingLoadBalancingReportMessageType";
	   case ShardingLoadBalancingReportRequestMessageType:
		   return "ShardingLoadBalancingReportRequestMessageType";
	   case ShardingCopyToMeMessageType:
		   return "ShardingCopyToMeMessageType";
	   case ShardingCommitMessageType:
		   return "ShardingCommitMessageType";
	   case ShardingCommitACKMessageType:
		   return "ShardingCommitACKMessageType";
	   case ShardingMMNotificationMessageType:
		   return "ShardingMMNotificationMessageType";
	   case ShardingNodeFailureNotificationMessageType:
		   return "ShardingNodeFailureNotificationMessageType";
	   default:
		   return "Message";
		   break;
	   }
	   return "";
}


}
}