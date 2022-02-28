//
// Author: Matteo Marescotti
//
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <iostream>
#include <random>
#include <mutex>
#include "client/SolverProcess.h"
#include "OpenSMTSolver.h"


using namespace opensmt;

std::unique_ptr<OpenSMTSolver> openSMTSolver;
const char *SolverProcess::solver = "OpenSMT2";


void SolverProcess::init() {
    //mtx_solve.lock();
//    FILE *file = fopen("/dev/null", "w");
//    dup2(fileno(file), fileno(stdout));
//    dup2(fileno(file), fileno(stderr));
//    fclose(file);

    static const char *default_split = spts_none;
    static const char *default_seed = "0";

    if (this->header.get(net::Header::parameter, "seed").size() == 0) {
        this->header.set(net::Header::parameter, "seed", default_seed);
    }

    if (this->header.get(net::Header::parameter, "split-type").size() > 0 &&
        this->header.get(net::Header::parameter, "split-type") != spts_lookahead &&
        this->header.get(net::Header::parameter, "split-type") != spts_scatter) {
        this->warning("bad parameter.split-type: '" + this->header.get(net::Header::parameter, "seed") +
            "'. using default (" +default_split +")");
        this->header.remove(net::Header::parameter, "split-type");
    }
    if (this->header.get(net::Header::parameter, "split-type").size() == 0) {
        this->header.set(net::Header::parameter, "split-type", default_split);
    }
}
void static segfault_sigaction(int signal, siginfo_t *si, void *arg)
{
    printf("Caught segfault at address %p\n", si->si_errno);
    exit(0);
}
void SolverProcess::solve() {
    const char *msg;
    SMTConfig config;
    config.setRandomSeed(atoi(this->header.get(net::Header::parameter, "seed").c_str()));
    config.setOption(SMTConfig::o_sat_split_type,
                     SMTOption(this->header.get(net::Header::parameter, "split-type").c_str()), msg) ;
#ifdef ENABLE_DEBUGING
    synced_stream.println(true ? Color::FG_BrightCyan : Color::FG_DEFAULT,
                          "split-preference:: "+this->header["split-preference"]+" seed:",this->header.get(net::Header::parameter, "seed"));
#endif
    if (not this->header["split-preference"].empty()) {
        config.setOption(SMTConfig::o_sat_split_preference, SMTOption(this->header["split-preference"].c_str()), msg);
        this->header.erase("split-preference");
    }
    openSMTSolver.reset( new OpenSMTSolver(config,  this->instance, getChannel()));

    try
    {
        search();

    }
    catch (OsmtInternalException& ex)
    {
        this->error(std::string(ex.what()));
    }
}

void SolverProcess::clausePull(const string & seed, const string & n1, const string & n2)
{
    try {
//        timer.start();
//        int counter = 0;
//        srand(getpid() + time(NULL));
        //channel.getMutex().lock();
        int interval = atoi(n1.c_str()) + ( atoi(seed.c_str()) % ( atoi(n2.c_str())- atoi(n1.c_str()) + 1 ) );
        synced_stream.println(true ? Color::Code::FG_BrightRed : Color::Code::FG_DEFAULT, "pull time: ",
                              interval);
//        std::chrono::time_point wakeupAt(std::chrono::system_clock::now() + std::chrono::milliseconds (interval));
        int counter = 0;
        std::chrono::duration<double> wakeupAt = std::chrono::milliseconds (interval);
        while (true) {
            if (getChannel().shouldTerminate()) break;
            std::unique_lock<std::mutex> lk(getChannel().getMutex());
#ifdef ENABLE_DEBUGING
//            opensmt::PrintStopWatch timer1("[t pull wait for pull red]",synced_stream,
//                                           true ? Color::Code::FG_Red : Color::Code::FG_DEFAULT );
#endif
            if (not getChannel().waitFor(lk, wakeupAt))
            {
                if (getChannel().shouldTerminate()) break;
                net::Header header = this->header.copy({"name", "node"});
                header["lemmas"] = "-" + this->header["lemmas"];
                lk.unlock();
                std::vector<net::Lemma> lemmas;
//                if (header["node"]=="[]") {
//                    synced_stream.println(true ? Color::FG_Red : Color::FG_DEFAULT, "[t pull root: "+header["node"]);
//                    continue;
//                }
                if (this->lemma_pull(lemmas, header)) {
                    lk.lock();
                    node_PulledLemmas[header["node"]].insert(std::end(node_PulledLemmas[header["node"]]),
                                                             std::begin(lemmas), std::end(lemmas));

                    this->interrupt(PartitionChannel::Command.ClauseInjection);
                    lk.unlock();

                }
//                counter++;

//                    std::cout << "[t comunication] Wakeup time not reached! " << "\tLemmas:" << pulled_lemmas.size()
//                              << std::endl;
//                wakeupAt = std::chrono::milliseconds ( lemmaIsPulled ? interval: interval / 2);
            }
            else
            {
//                std::cerr << "Break pull : " << std::endl;
                break;
            }
#ifdef ENABLE_DEBUGING
            synced_stream.println(true ? Color::Code::FG_BrightRed : Color::Code::FG_DEFAULT, "pull time: ",
                                  timer.elapsed_time_milliseconds());
#endif
        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception pull : " << e.what() << std::endl;
    }
}

void SolverProcess::clausePush(const string & seed, const string & n1, const string & n2)
{
    try {
//        timer.start();
        int interval = atoi(n1.c_str()) + ( atoi(seed.c_str()) % ( atoi(n2.c_str())- atoi(n1.c_str()) + 1 ) );
        synced_stream.println(true ? Color::Code::FG_BrightRed : Color::Code::FG_DEFAULT, "push time: ",
                              interval);
//        std::chrono::time_point wakeupAt(std::chrono::system_clock::now() + std::chrono::milliseconds (interval));
        std::chrono::duration<double> wakeupAt = std::chrono::milliseconds (interval);
        std::map<std::string, std::vector<net::Lemma>> toPush_lemmas;
        std::map<std::string, std::vector<std::pair<string ,int>>> clauses;
        while (true) {
#ifdef ENABLE_DEBUGING
//            opensmt::PrintStopWatch timer1("[t push wait for push red]",synced_stream,
//                                           true ? Color::Code::FG_RED : Color::Code::FG_DEFAULT );
#endif
            if (getChannel().shouldTerminate()) break;
            std::unique_lock<std::mutex> lk(getChannel().getMutex());

            if (not getChannel().waitFor(lk, wakeupAt))
            {


#ifdef ENABLE_DEBUGING

                synced_stream.println(true ? Color::Code::FG_BrightBlack : Color::Code::FG_DEFAULT, "push time: ",
                                      timer.elapsed_time_milliseconds());
#endif
                if (getChannel().shouldTerminate()) break;
                else if (not getChannel().empty())
                {
                    clauses = getChannel().getClauseMap();
                    getChannel().clear();
                    net::Header header = this->header.copy({"name"});
                    lk.unlock();
                    //Read buffer

                    for ( const auto &toPushClause : clauses )
                    {
//                        if (toPushClause.first=="[]") {
//                            synced_stream.println(true ? Color::FG_Red : Color::FG_DEFAULT, "[t root: "+toPushClause.first);
//                            continue;
//                        }
//                        int i=0;
                        for (auto clause = toPushClause.second.cbegin(); clause != toPushClause.second.cend(); ++clause) {
//                            string s(toPushClause.first);
//                            size_t count =std::count_if( s.begin(), s.end(),
//                                           []( char c ) { return std::isdigit( c ); } );

//                            if(i==1 and toPushClause.first!="[]") {
//                                count = (count / 2) - 1;
//                                toPush_lemmas[toPushClause.first].push_back(net::Lemma(clause->first, count));
//                            }
//
//                            else
                                toPush_lemmas[toPushClause.first].push_back(net::Lemma(clause->first, clause->second));
//                            if(i==2)
//                                break;
//#ifdef ENABLE_DEBUGING
////                            if (clause->first.find(".frame") != string::npos) {
////                                this->error(std::string("unusual clause found: "));
//////                                synced_stream.println(true ? Color::FG_Red : Color::FG_DEFAULT, "[t push frame caught: "+clause->first);
//////                                continue;
////                            }
//#endif
//                            if (i==0) {
//                                if (toPushClause.first == "[0, 0]" and i % 2 == 0)
//                                    toPush_lemmas[toPushClause.first].push_back(net::Lemma(clause->first, 0));
//                                else if (toPushClause.first == "[0, 1]" and i % 2 == 0)
//                                    toPush_lemmas[toPushClause.first].push_back(net::Lemma(clause->first, 0));
//                                else if (toPushClause.first == "[0, 0, 0, 0]" and i % 2 == 0)
//                                    toPush_lemmas[toPushClause.first].push_back(net::Lemma(clause->first, 1));
//                                if (toPushClause.first == "[0, 1, 0, 0]" and i % 2 == 0)
//                                    toPush_lemmas[toPushClause.first].push_back(net::Lemma(clause->first, 1));
//                                else if (toPushClause.first == "[]")
//                                    toPush_lemmas[toPushClause.first].push_back(net::Lemma(clause->first, 0));
//                            }
//                            else
//                            {
//                                if (toPushClause.first == "[0, 0]")
//                                    toPush_lemmas[toPushClause.first].push_back(net::Lemma(clause->first, 1));
//                                else if (toPushClause.first == "[0, 1]")
//                                    toPush_lemmas[toPushClause.first].push_back(net::Lemma(clause->first, 1));
//                                else if (toPushClause.first == "[0, 0, 0, 0]")
//                                    toPush_lemmas[toPushClause.first].push_back(net::Lemma(clause->first, 2));
//                                else if (toPushClause.first == "[0, 1, 0, 0]")
//                                    toPush_lemmas[toPushClause.first].push_back(net::Lemma(clause->first, 2));
//                                else if (toPushClause.first == "[]")
//                                    toPush_lemmas[toPushClause.first].push_back(net::Lemma(clause->first, 0));
//                            }
//                            i++;
                        }
#ifdef ENABLE_DEBUGING
                        synced_stream.println(true ? opensmt::Color::FG_BrightBlue : opensmt::Color::FG_DEFAULT,
                                              "[t push -> PID= "+to_string(getpid())+" ] pushed from Node -> "+ toPushClause.first+" Size:"+ to_string(toPushClause.second.size()));
#endif
//                        break;
                    }

//Synchronize with CC

                    lemma_push(toPush_lemmas, header);
                    toPush_lemmas.clear();
//                    if (i>0) return;
//                } else std::cout << "\033[1;51m [t comunication] No shared clauses to copy -> size::\033[0m" << endl;
//                    wakeupAt = std::chrono::system_clock::now() + std::chrono::milliseconds (interval);
                }
//                else std::cout << "[t push] Channel empty! " <<endl;

            }
            else { break; }
//        wakeupAt = std::chrono::system_clock::now() + minSleep;
        }
//        std::unique_lock<std::mutex> lk(getChannel().getStopMutex());
//        getChannel().shouldPushTerminate();
//
//        if (getChannel().shouldPullTerminate()) {
//            lk.unlock();
//            getChannel().notify_one_t();
//        }
//        }
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception push : " << e.what() << std::endl;
    }
}

void SolverProcess::checkForlearned_pushBeforIncrementality() {
//    std::scoped_lock<std::mutex> _l(this->lemma.mtx_li);
    if (openSMTSolver->learned_push) {
        openSMTSolver->learned_push = false;
        openSMTSolver->getMainSplitter().pop();
    }
}


void SolverProcess::search()
{
    std::string smtlib;
//    try {
    while (true) {

#ifdef ENABLE_DEBUGING
        std::cout << "[t comunication -> PID= "+to_string(getpid())+" ] Before interpFile: "<< std::endl
                  <<(char *) ( smtlib + this->header["query"]).c_str()<<endl;
        synced_stream.println(true
                              ? opensmt::Color::FG_Blue : opensmt::Color::FG_DEFAULT,this->header["node"].substr(1, this->header["node"].size() - 2));
#endif
        getChannel().setWorkingNode(this->header["node"]);

        ( (ScatterSplitter &) openSMTSolver->getMainSplitter().getSMTSolver() ).set_solver_address(this->header["node"].substr(1, this->header["node"].size() - 2));
//        openSMTSolver->getMainSplitter().set_num_of_pushes(count_Occurrences((char *) (smtlib).c_str(), "(push 1)"));
        std::string query = this->header["query"];
        getChannel().getMutex().unlock();

        openSMTSolver->preInterpret->interpFile((char *) (smtlib + query).c_str());

        if (getChannel().shouldTerminate()) {
            break;
        }
        smtlib.clear();
        std::unique_lock<std::mutex> lock_channel(getChannel().getMutex());
#ifdef ENABLE_DEBUGING
        std::cout<<"node: "<<this->header["node"]<<endl;
        for (int i = 0; i <getChannel().size_query(); ++i) {
            std::cout<<"query "<<i+1<<": "<<getChannel().get_queris()[i]<<endl;
        }
#endif
        openSMTSolver->setResult(openSMTSolver->getMainSplitter().getStatus());
        if (openSMTSolver->getResult() == s_True) {
            std::string statusInfo = openSMTSolver->getMainSplitter().getConfig().getInfo(":status").toString();
            this->report(PartitionChannel::Status::sat, statusInfo);
        } else if (openSMTSolver->getResult() == s_False) {
            std::string statusInfo = openSMTSolver->getMainSplitter().getConfig().getInfo(":status").toString();
            this->report(PartitionChannel::Status::unsat, statusInfo);

        }

        if (openSMTSolver->getResult() != s_Undef) {
            openSMTSolver->setResult (s_Undef);

#ifdef ENABLE_DEBUGING
            synced_stream.println(true ? Color::FG_Red : Color::FG_DEFAULT, "[t comunication -> result: " + openSMTSolver->getResult().getValue()
                    ," from Node " + this->header["node"]);
#endif
            getChannel().waitForQueryOrTemination(lock_channel);
//                std::cout <<defred<< "[t comunication -> PID= "+to_string(getpid())+" ]  after waiting to receive command ... ]"<<def1<<endl ;
            if (getChannel().shouldTerminate())
                break;

        }
        if (getChannel().shouldInjectClause())
        {
#ifdef ENABLE_DEBUGING
            synced_stream.println(true ? opensmt::Color::FG_Cyan : opensmt::Color::FG_DEFAULT,
                                  "[t comunication -> PID= "+to_string(getpid())+" ] Current Node -> "+ this->header["node"] );
#endif
            for ( const auto &lemmaPulled : node_PulledLemmas )
            {
#ifdef ENABLE_DEBUGING
                synced_stream.println(true ? opensmt::Color::FG_Cyan : opensmt::Color::FG_DEFAULT,
                                      "[t comunication -> PID= "+to_string(getpid())+" ] check for pulled Node -> "+ lemmaPulled.first);
#endif
                if (not node_PulledLemmas[lemmaPulled.first].empty())
                {
                    if (isPrefix(lemmaPulled.first.substr(1, lemmaPulled.first.size() - 2),
                                 this->header["node"].substr(1, this->header["node"].size() - 2)))
                    {
#ifdef ENABLE_DEBUGING
                        synced_stream.println(true ? opensmt::Color::FG_Cyan : opensmt::Color::FG_DEFAULT,
                                              "[t comunication -> PID= " + to_string(getpid()) +
                                              " ] Node is prefix of the current -> " + lemmaPulled.first);
                        std::string conflict = std::to_string(( (ScatterSplitter &) openSMTSolver->getMainSplitter().getSMTSolver() ).conflicts );
                        std::cout<<";conflict before Clause Injection: "<< conflict << endl;
#endif

                        injectPulledClauses(lemmaPulled.first);
                    }
                }
            }

            node_PulledLemmas.clear();
            getChannel().clearInjectClause();
        }
        else if (not getChannel().isEmpty_query())
        {
            if (getChannel().shouldTerminate()) {
                break;
            }
            PartitionChannel::Task task = this->wait(0 );
            switch (task.command) {
                case PartitionChannel::Task::incremental:
                    smtlib = task.smtlib;
//                                checkForlearned_pushBeforIncrementality();
//                                getChannel().setShouldStop();
////
//                                openSMTSolver->preInterpret->interpFile((char *) (smtlib + this->header["query"]).c_str());
//                                smtlib.clear();
////
//                                getChannel().clearShouldStop();
                    break;
                case PartitionChannel::Task::resume:
                    break;
            }
//                        std::unique_lock<std::mutex> lk(getChannel().getStopMutex());
//                        getChannel().clearShouldStop();
            //getChannel().notify_one();
            getChannel().pop_front_query();
//                    std::scoped_lock<std::mutex> _l(this->mtx_listener_solve);
            header_Temp.pop_front();
            instance_Temp.pop_front();
        }

        if (getChannel().size_query() > 0)
            getChannel().setShouldStop();
        else getChannel().clearShouldStop();



#ifdef ENABLE_DEBUGING

#endif
    }
//    }
//    catch (std::exception& e)
//    {
//        std::cerr << "Exception search : " << e.what() << std::endl;
//    }
}

void SolverProcess::interrupt(const std::string& command) {
#ifdef ENABLE_DEBUGING
    synced_stream.println(true ? opensmt::Color::FG_Yellow : opensmt::Color::FG_DEFAULT,
                          "[t Listener , Pull -> PID= "+to_string(getpid())+" ] OpenSMT2 Should  -> "
                          + command +" on Node -> "+this->header["node"]+" "+this->header["name"]);
#endif
//    if (getChannel().shouldTerminate()) return;
    if (command == PartitionChannel::Command.Stop) {
        getChannel().setTerminate();
        getChannel().setShouldStop();
        return;
    }
    if (command == PartitionChannel::Command.ClauseInjection) {
        getChannel().setInjectClause();
    }
    else {
        getChannel().push_back_query(command);
    }
    getChannel().setShouldStop();
}
void SolverProcess::kill_child()
{
    int wstatus;
//    printf("printed from parent process - %d\n", getpid());
//    std::cout<<"send kill signall to forked process!: "<<child_pid<<endl;
    int ret;
    ret = kill(child_pid, SIGKILL);
    if (ret == -1) {
        perror("kill");
        exit(EXIT_FAILURE);
    }

    if (waitpid(child_pid, &wstatus, WUNTRACED | WCONTINUED) == -1) {
        perror("waitpid");
        exit(EXIT_FAILURE);
    }
}
volatile sig_atomic_t shutdown_flag = 1;

void cleanupRoutine(int signal_number)
{
    shutdown_flag = 0;
}
void SolverProcess::partition(uint8_t n) {

    pid_t pid = getpid();
    //fork() returns -1 if it fails, and if it succeeds, it returns the forked child's pid in the parent, and 0 in the child.
    // So if (fork() != 0) tests whether it's the parent process.
    child_pid = fork();
    if (child_pid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }
    if (child_pid > 0)
    {
        forked = true;
        return;
    }
//    std::thread _th([&] {
//        while (getppid() == pid)
//            sleep(1);
//        exit(0);
//    });
    std::thread _ts([&] {
        while (true) {
            sleep(1);
            if (getppid() == 1)
                exit(0);
        }

    });
//    printf("printed from child process - %d\n", getpid());

    struct sigaction sigterm_action;
    memset(&sigterm_action, 0, sizeof(sigterm_action));
    sigterm_action.sa_handler = &cleanupRoutine;
    sigterm_action.sa_flags = 0;

    // Mask other signals from interrupting SIGTERM handler
    if (sigfillset(&sigterm_action.sa_mask) != 0)
    {
        std::cout<<";sigfillset: "<<endl;
        perror("sigfillset");
        exit(EXIT_FAILURE);
    }
    // Register SIGTERM handler
    if (sigaction(SIGTERM, &sigterm_action, NULL) != 0)
    {
        perror("sigaction SIGTERM");
        exit(EXIT_FAILURE);
    }
//    printf("count = %d\n", count);
//
//    exit(EXIT_SUCCESS);
//    FILE *file = fopen("/dev/null", "w");
//    dup2(fileno(file), fileno(stdout));
//    dup2(fileno(file), fileno(stderr));
//    fclose(file);
//    std::cout << "\033[1;51m [t partition] Inside partition after fork -> size::\033[0m" << endl;
    getChannel().clearClauseShareMode();
    std::vector<std::string> partitions;
    std::string conflict = std::to_string(( (ScatterSplitter &) openSMTSolver->getMainSplitter().getSMTSolver() ).conflicts );
    std::string statusInfo = openSMTSolver->getMainSplitter().getConfig().getInfo(":status").toString();
//    std::cout<<";conflict before splitting: "<< conflict<<endl;
//    std::cout<<";statusInfo before splitting: "<< statusInfo<<endl;
    const char *msg;
    if ( not(
            openSMTSolver->getMainSplitter().getConfig().setOption(SMTConfig::o_sat_split_num, SMTOption(int(n)),msg) and
            openSMTSolver->getMainSplitter().getConfig().setOption(SMTConfig::o_sat_solver_limit,SMTOption(INT_MAX),msg) and
            openSMTSolver->getMainSplitter().getConfig().setOption(SMTConfig::o_smt_split_format_length,SMTOption("brief"),msg) and
            openSMTSolver->getMainSplitter().getConfig().setOption(SMTConfig::o_sat_split_units, SMTOption(spts_time), msg) and
            openSMTSolver->getMainSplitter().getConfig().setOption(SMTConfig::o_sat_split_inittune, SMTOption(double(2)), msg) and
            openSMTSolver->getMainSplitter().getConfig().setOption(SMTConfig::o_sat_split_midtune, SMTOption(double(2)), msg) and
            openSMTSolver->getMainSplitter().getConfig().setOption(SMTConfig::o_sat_split_asap, SMTOption(0), msg)))
    {
        this->report(partitions, conflict, statusInfo, msg);
    }
    else {
        try {
            static_cast<ScatterSplitter &> (openSMTSolver->getMainSplitter().getSMTSolver()).setSplitConfig_split_on();

            sstat status = openSMTSolver->getMainSplitter().solve();
//        if (this->header["node"]== "[0, 0]") {
//            std::cout<<"test"<<endl;
//            this->report(PartitionChannel::Status::unsat);
//            exit(0);
//        }
            if (status == s_Undef) {
                partitions = openSMTSolver->getMainSplitter().getPartitionSplits();
#ifdef ENABLE_DEBUGING
                std::cout <<"Recieved PN from SMTS Server-> "<<int(n)<<"\tSplits constructed by solver: "<<partitions.size()<<this->header["node"]<<endl;
#endif
//
                this->report(partitions, conflict, statusInfo);

            } else if (status == s_True) {
#ifdef ENABLE_DEBUGING
                std::cout<<"PartitionProcess - Result is SAT"<<endl;
#endif
                this->report(PartitionChannel::Status::sat, statusInfo, conflict);
            } else if (status == s_False) {
#ifdef ENABLE_DEBUGING
                std::cout<<"PartitionProcess - Result is UNSAT"<<endl;
#endif
//            Logger::writeIntoFile(false, this->header["node"] ,"Result is UNSAT ",getpid());
                this->report(PartitionChannel::Status::unsat, statusInfo, conflict);
            } else {
                this->report(partitions, conflict, statusInfo, "error during partitioning");
            }
        }
        catch (OsmtInternalException& ex)
        {
//            this->error(std::string(ex.what()));
            exit(EXIT_FAILURE);
        }
    }

//    std::cout <<"PartitionProcess - Main Thread: End to partition process"<<endl;
    exit(EXIT_SUCCESS);

}

void SolverProcess::getCnfClauses(net::Header &header, const std::string &payload) {
    if (header.count("query")) {

        pid_t pid = getpid();
        if (fork() != 0) {
            return;
        }

        std::thread _t([&] {
            while (getppid() == pid) {
                sleep(1);
            }
            exit(0);
        });

        SMTConfig config;
        config.set_dryrun(true);
        openSMTSolver.reset( new OpenSMTSolver(config, instance, getChannel()));
        openSMTSolver->preInterpret->interpFile((char *) (payload + header["query"]).c_str());

        std::string cnf = openSMTSolver->getMainSplitter().getSMTSolver().printCnfClauses();
        this->report(header, header["command"], cnf);
        exit(0);
    } else {
        std::string cnf = openSMTSolver->getMainSplitter().getSMTSolver().printCnfClauses();
        this->report(header, header["command"], cnf);
    }
}

void SolverProcess::getCnfLearnts(net::Header &header) {
    std::string cnf = openSMTSolver->getMainSplitter().getSMTSolver().printCnfLearnts();
    this->report(header, header["command"], cnf);
}

void SolverProcess::injectPulledClauses(const std::string& nodePath) {
//    if (pulled_lemmas[nodePath].empty()) return;
//    if (openSMTSolver->learned_push)
//    openSMTSolver->getMainSplitter().pop();

#ifdef ENABLE_DEBUGING
//    struct sigaction sa;
//    memset(&sa, 0, sizeof(struct sigaction));
//    sigemptyset(&sa.sa_mask);
//    sa.sa_sigaction = segfault_sigaction;
//    sa.sa_flags   = SA_SIGINFO;
//    sigaction(SIGILL, &sa, NULL);
    synced_stream.println(true ? opensmt::Color::FG_Green : opensmt::Color::FG_DEFAULT,
                          "[t comunication -> PID= "+to_string(getpid())+" ] inject accumulated clauses -> Size:" + to_string(node_PulledLemmas[nodePath].size()));
#endif
//    openSMTSolver->getMainSplitter().push();
//    openSMTSolver->learned_push = true;
//    try
//    {
    for (auto &lemma:node_PulledLemmas[nodePath])
    {
        if (lemma.smtlib.size() > 0)
        {

#ifdef ENABLE_DEBUGING

            cout <<" clause: "<<endl<< (char *) ("(assert " + lemma.smtlib + ")").c_str()<<std::endl;
#endif
            openSMTSolver->preInterpret->interpFile((char *) ("(assert " + lemma.smtlib + ")").c_str());

        }
    }
//    }
//    catch (std::exception& e)
//    {
//        std::cerr << "Exception injectPulledClauses : " << e.what() << std::endl;
//    }
}