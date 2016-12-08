/*
 * Copyright (c) 2016, SRCH2
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    * Neither the name of the SRCH2 nor the
 *      names of its contributors may be used to endorse or promote products
 *      derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL SRCH2 BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/**
 * This test case tests schema section of the config file. Checks if it works for various
 * combination and number of searchable, refining, and indexed fields.
 * It checks for invalid boost, invalid record boost field and all possible combinations
 * for character offset/positionalIndex and field based search.
 *
 * The conf-logging.xml file has log tag outside core, it enables testing of global log tag.
 */

#include "server/util/xmlParser/pugixml.hpp"
#include <iostream>
#include <string>
#include <stdlib.h>
#include "util/Assert.h"
#include <assert.h>
#include "util/Logger.h"
#include "ConfigManager.h"
#include "HTTPRequestHandler.h"
#include "Srch2Server.h"

using namespace std;
using namespace srch2::instantsearch;

using namespace std;
namespace srch2is = srch2::instantsearch;
namespace srch2http = srch2::httpwrapper;

using srch2http::ConfigManager;
using namespace srch2::util;
using namespace pugi;
using srch2http::SearchableAttributeInfoContainer;
using srch2http::RefiningAttributeInfoContainer;


int main(int argc, char* argv[])
{
    string configFile1(string(getenv("srch2_config_file")) + "/conf-SRI-I.xml");
    string configFile2(string(getenv("srch2_config_file")) + "/conf-SI-RI.xml");
    string configFile3(string(getenv("srch2_config_file")) + "/conf-invalid-IR.xml");
    string configFile4(string(getenv("srch2_config_file")) + "/conf-SRI-false.xml");
    string configFile5(string(getenv("srch2_config_file")) + "/conf-SI-present-R-missing.xml");
    string configFile6(string(getenv("srch2_config_file")) + "/conf-inconsistent-attribute.xml");
    string configFile7(string(getenv("srch2_config_file")) + "/conf-unique-searchable.xml");
    string configFile8(string(getenv("srch2_config_file")) + "/conf-unique-refining.xml");
    string configFile9(string(getenv("srch2_config_file")) + "/conf-invalid-boostField.xml");
    string configFile10(string(getenv("srch2_config_file")) + "/conf-invalidRecordBoostField.xml");
    string configFile11(string(getenv("srch2_config_file")) + "/conf-fieldBasedSearch.xml");
    string configFile12(string(getenv("srch2_config_file")) + "/conf-fieldBasedSearch-2.xml");
    string configFile13(string(getenv("srch2_config_file")) + "/conf-fieldBasedSearch-3.xml");
    string configFile14(string(getenv("srch2_config_file")) + "/conf-fieldBasedSearch-4.xml");
    string configFile15(string(getenv("srch2_config_file")) + "/conf-logging.xml");
    string configFile16(string(getenv("srch2_config_file")) + "/conf-singleCore.xml");
    string configFile17(string(getenv("srch2_config_file")) + "/conf-responseContent.xml");
    string configFile18(string(getenv("srch2_config_file")) + "/conf-sqlLite.xml");

    ConfigManager *serverConf1 = new ConfigManager(configFile1);
    ConfigManager *serverConf2 = new ConfigManager(configFile2);
    ConfigManager *serverConf3 = new ConfigManager(configFile3);
    ConfigManager *serverConf4 = new ConfigManager(configFile4);
    ConfigManager *serverConf5 = new ConfigManager(configFile5);
    ConfigManager *serverConf6 = new ConfigManager(configFile6);
    ConfigManager *serverConf7 = new ConfigManager(configFile7);
    ConfigManager *serverConf8 = new ConfigManager(configFile8);
    ConfigManager *serverConf9 = new ConfigManager(configFile9);
    ConfigManager *serverConf10 = new ConfigManager(configFile10);
    ConfigManager *serverConf11 = new ConfigManager(configFile11);
    ConfigManager *serverConf12 = new ConfigManager(configFile12);
    ConfigManager *serverConf13 = new ConfigManager(configFile13);
    ConfigManager *serverConf14 = new ConfigManager(configFile14);
    ConfigManager *serverConf15 = new ConfigManager(configFile15);
    ConfigManager *serverConf16 = new ConfigManager(configFile16);
    ConfigManager *serverConf17 = new ConfigManager(configFile17);
    ConfigManager *serverConf18 = new ConfigManager(configFile18);


    ASSERT(serverConf15->loadConfigFile() == true);
    ASSERT(serverConf1->loadConfigFile() == true);
    ASSERT(serverConf2->loadConfigFile() == true);
    ASSERT(serverConf3->loadConfigFile() == true);
    ASSERT(serverConf6->loadConfigFile() == true);
    ASSERT(serverConf5->loadConfigFile() == true);
    ASSERT(serverConf7->loadConfigFile() == true);
    ASSERT(serverConf8->loadConfigFile() == false);
    ASSERT(serverConf9->loadConfigFile() == false);
    ASSERT(serverConf10->loadConfigFile() == false);
    ASSERT(serverConf11->loadConfigFile() == true);
    ASSERT(serverConf12->loadConfigFile() == true);
    ASSERT(serverConf13->loadConfigFile() == true);
    ASSERT(serverConf14->loadConfigFile() == true);
    ASSERT(serverConf15->loadConfigFile() == true);
    ASSERT(serverConf16->loadConfigFile() == true);
    ASSERT(serverConf17->loadConfigFile() == true);

    //This config file is single core with no core tags and dataFile at top, but it
    //has dataDir at the top level.
    ASSERT(serverConf18->loadConfigFile() == true);

    //This checks if the log file path and log level are correctly set in the config file where log
    //tag has been moved out of core
    ASSERT(serverConf15->getHTTPServerAccessLogFile() == "./multicore/srch2-log.txt");
    ASSERT(serverConf15->getHTTPServerLogLevel() == 3);

    const std::string &expr_string = "invalid Expression";
    RankerExpression* rank = new RankerExpression(expr_string);

    ConfigManager::CoreInfoMap_t::iterator it;

    for(it = serverConf14->coreInfoIterateBegin(); it != serverConf14->coreInfoIterateEnd(); it++) {
        ASSERT(it->second->getSupportAttributeBasedSearch() == 1);
    }

    for(it = serverConf13->coreInfoIterateBegin(); it != serverConf13->coreInfoIterateEnd(); it++) {
        ASSERT(it->second->getSupportAttributeBasedSearch() == 1);
    }

    for(it = serverConf12->coreInfoIterateBegin(); it != serverConf12->coreInfoIterateEnd(); it++) {
        ASSERT(it->second->getSupportAttributeBasedSearch() == 0);
    }

    for(it = serverConf11->coreInfoIterateBegin(); it != serverConf11->coreInfoIterateEnd(); it++) {
        ASSERT(it->second->getSupportAttributeBasedSearch() == 1);
    }

    for(it = serverConf7->coreInfoIterateBegin(); it != serverConf7->coreInfoIterateEnd(); it++) {

        map<string, SearchableAttributeInfoContainer> ::const_iterator it2 = it->second->getSearchableAttributes()->begin();
        ASSERT(it2->first == "uniqueSearchableField");
    }

    for(it = serverConf5->coreInfoIterateBegin(); it != serverConf5->coreInfoIterateEnd(); it++) {

        map<string, RefiningAttributeInfoContainer> ::const_iterator it2 = it->second->getRefiningAttributes()->begin();
        ASSERT(it2->first == "id");
        it2++;
        ASSERT(it2->first == "refining1");
        it2++;
        ASSERT(it2->first == "refining2");
        it2++;
        ASSERT(it2->first == "refining3");
    }

    for(it = serverConf5->coreInfoIterateBegin(); it != serverConf5->coreInfoIterateEnd(); it++) {

        map<string, SearchableAttributeInfoContainer> ::const_iterator it2 = it->second->getSearchableAttributes()->begin();
        ASSERT(it2->first == "refining1");
        it2++;
        ASSERT(it2->first == "refining2");
        it2++;
    }

    for(it = serverConf6->coreInfoIterateBegin(); it != serverConf6->coreInfoIterateEnd(); it++) {

        map<string, SearchableAttributeInfoContainer> ::const_iterator it2 = it->second->getSearchableAttributes()->begin();
        ASSERT(it2->first == "director");
        it2++;
        ASSERT(it2->first == "inconsistentField");
        it2++;
        ASSERT(it2->first == "inconsistentField2");
        it2++;
    }
}



