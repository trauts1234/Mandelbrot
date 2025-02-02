#undef RUN_GEN_MAGICS

#include "threadsafe_io.h"
#include "UCI.h"

#include <chrono>
#include <thread>


#ifdef RUN_GEN_MAGICS
#include "sliding_lookup_generation.h"
#endif

#ifdef QUIET_GEN
#include "quiet_gen.h"
#include <cassert>
#endif


int main() {

    #ifdef RUN_UCI

    std::string command;
    UCI::Context ctx = UCI::Context();
    UCI::Init();
    
    while(command != "quit"){
        getline(std::cin, command);
        UCI::HandleMessage(command, ctx);
    }

    #endif

    #ifdef RUN_GEN_MAGICS
    GenerateMain();
    #endif

    #ifdef QUIET_GEN

    std::signal(SIGINT, QuietGen::signalHandler);

    std::string data_line;

    assert(QuietGen::in_file.is_open());

    sync_cout << "starting to create training data" << std::endl;

    size_t idx=0;
    
    while(getline(QuietGen::in_file, data_line)){
        bool success = QuietGen::HandleJsonEntry(data_line);
        if(!success){
            break;//stop flag set or something broke
        }
        idx++;
    }

    QuietGen::out_writer.Destruct();
    QuietGen::in_file.close();
    sync_cout << "run out of training data. stopping" << std::endl;
    #endif

    return 0;

}
