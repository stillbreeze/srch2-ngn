#include "RoutingManager.h"
#include "server/MongodbAdapter.h"

using namespace srch2::httpwrapper;

namespace srch2 {
namespace httpwrapper {


RoutingManager::RoutingManager(ConfigManager&  cm, TransportManager& transportManager)  : 
    						configurationManager(cm),  transportManager(transportManager), dpInternal(&cm),
    						internalMessageBroker(*this, dpInternal) {


	// TODO : do we have one Srch2Server per core? now, yes.
	// create a server (core) for each data source in config file
	for(ConfigManager::CoreInfoMap_t::const_iterator coreInfoMapItr =
			cm.coreInfoIterateBegin(); coreInfoMapItr != cm.coreInfoIterateEnd();
			coreInfoMapItr++) {
		shardServers.insert(std::pair<unsigned, Srch2Server *>(coreInfoMapItr->second->getCoreId(), new Srch2Server()));
		Srch2Server *srch2Server = shardServers[coreInfoMapItr->second->getCoreId()];
		srch2Server->setCoreName(coreInfoMapItr->second->getName());

		if(coreInfoMapItr->second->getDataSourceType() ==
				srch2::httpwrapper::DATA_SOURCE_MONGO_DB) {
			// set current time as cut off time for further updates
			// this is a temporary solution. TODO
			MongoDataSource::bulkLoadEndTime = time(NULL);
			//require srch2Server
			MongoDataSource::spawnUpdateListener(srch2Server);
		}

		//load the index from the data source
		try{
			srch2Server->init(&cm);
		} catch(exception& ex) {
			/*
			 *  We got some fatal error during server initialization. Print the error
			 *  message and exit the process.
			 *
			 *  Note: Other internal modules should make sure that no recoverable
			 *        exception reaches this point. All exceptions that reach here are
			 *        considered fatal and the server will stop.
			 */
			Logger::error(ex.what());
			exit(-1);
		}
	}
}

ConfigManager* RoutingManager::getConfigurationManager() {
	return &this->configurationManager;

}

DPInternalRequestHandler* RoutingManager::getDpInternal() {
	return &this->dpInternal;
}

InternalMessageBroker * RoutingManager::getInternalMessageBroker(){
	return &this->internalMessageBroker;
}

MessageAllocator * RoutingManager::getMessageAllocator() {
	return transportManager.getMessageAllocator();
}

} }
//save indexes in deconstructor
