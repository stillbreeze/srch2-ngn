#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string>
#include <iostream>
#include <fstream>

using namespace std;

int main() {
    void *handle;
    int (*init_server)(string);
    char *error;
    string config_path = "release_files/documentation/docs/example-demo/srch2-config.xml";
    handle = dlopen("build/src/server/libsrch2-search-server.so", RTLD_LAZY);
    if (!handle) {
        fprintf (stderr, "%s\n", dlerror());
        exit(1);
    }
    dlerror();    /* Clear any existing error */
    *(void **)(&init_server) = dlsym(handle, "_Z17initialise_serverSs");
    if ((error = dlerror()) != NULL)  {
        fprintf (stderr, "%s\n", error);
        exit(1);
    }
    else
    {
        fprintf(stdout,"got correct object reference");
    }
    cout<<endl;
    // cout<<init_server;
    (*init_server)(config_path);
    dlclose(handle);
    return 0;
}