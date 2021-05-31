//
// Author: Matteo Marescotti
//

#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <random>
#include <mutex>
#include "client/SolverProcess.h"
#include "OpenSMTSolver.h"


using namespace opensmt;

OpenSMTInterpret *interpret = nullptr;

const char *SolverProcess::solver = "OpenSMT2";

std::mutex mtx_solve;

void SolverProcess::init() {
    mtx_solve.lock();
    FILE *file = fopen("/dev/null", "w");
    dup2(fileno(file), fileno(stdout));
    dup2(fileno(file), fileno(stderr));
    fclose(file);

    static const char *default_split = "scattering";
    static const char *default_seed = "0";

    if (this->header.get(net::Header::parameter, "seed").size() == 0) {
        this->header.set(net::Header::parameter, "seed", default_seed);
    }

    if (this->header.get(net::Header::parameter, "split").size() > 0 &&
        this->header.get(net::Header::parameter, "split") != spts_lookahead &&
        this->header.get(net::Header::parameter, "split") != spts_scatter) {
        this->warning(
                "bad parameter.split: '" + this->header.get(net::Header::parameter, "seed") + "'. using default (" +
                default_split +
                ")");
        this->header.remove(net::Header::parameter, "split");
    }
    if (this->header.get(net::Header::parameter, "split").size() == 0) {
        this->header.set(net::Header::parameter, "split", default_split);
    }
}

void SolverProcess::solve() {
    const char *msg;
    SMTConfig config;
    config.setRandomSeed(atoi(this->header.get(net::Header::parameter, "seed").c_str()));
    auto lemma_push = [&](const std::vector<net::Lemma> &lemmas) {
        this->lemma_push(lemmas);
    };
    auto lemma_pull = [&](std::vector<net::Lemma> &lemmas) {
        this->lemma_pull(lemmas);
    };
    interpret = new OpenSMTInterpret(this->header, lemma_push, lemma_pull, config);
    std::string smtlib = this->instance;

    while (true) {

        opensmt::stop = false;
        mtx_solve.unlock();
#ifdef ENABLE_DEBUGING
        std::thread first (Logger::writeIntoFile,false,"SolverProcess: Start to Solve...","Recieved command: "+header["command"],getpid());
            first.join();
#endif
        interpret->interpFile((char *) (smtlib + this->header["query"]).c_str());
        mtx_solve.lock();
        sstat status = interpret->main_solver->getStatus();
        interpret->f_exit = false;
        opensmt::stop = false;
#ifdef ENABLE_DEBUGING
        std::thread log (Logger::writeIntoFile,false,"SolverProcess: Finished solving...","Recieved command: "+header["command"],getpid());
            log.join();
#endif
        if (status == s_True) {
            this->report(Status::sat);
        }
        else if (status == s_False) {

            this->report(Status::unsat);
        }
    }
}

void SolverProcess::interrupt() {
    std::lock_guard<std::mutex> _l(mtx_solve);
    opensmt::stop= true;
}

void SolverProcess::partition(uint8_t n) {
    pid_t pid = getpid();
    //fork() returns -1 if it fails, and if it succeeds, it returns the forked child's pid in the parent, and 0 in the child.
    // So if (fork() != 0) tests whether it's the parent process.
#ifdef ENABLE_DEBUGING
    std::thread log (Logger::writeIntoFile,false,"SolverProcess - Main Thread: Start to fork partition process",
                     "Recieved command: "+header["command"],getpid());
        log.join();
#endif
    if (fork() != 0) {
        return;
    }
    std::thread _t([&] {
        while (getppid() == pid)
            sleep(1);
        exit(0);
    });
    //FILE *file = fopen("/dev/null", "w");
    //dup2(fileno(file), fileno(stdout));
    //dup2(fileno(file), fileno(stderr));
    //fclose(file);
#ifdef ENABLE_DEBUGING
    std::thread logger (Logger::writeIntoFile,false,"PartitionProcess - Main Thread: Start to set SMTConfig to do partiotion",
                     "Recieved command: "+header["command"],getpid());
        logger.join();
#endif
    std::vector<std::string> partitions;
    const char *msg;
    if (!(
            interpret->main_solver->getConfig().setOption(SMTConfig::o_sat_split_num,
                                                          SMTOption(int(n)),
                                                          msg) &&
            interpret->main_solver->getConfig().setOption(SMTConfig::o_sat_split_type,
                                                          SMTOption(this->header.get(net::Header::parameter, "split")
                                                                            .c_str()),
                                                          msg) &&
            interpret->main_solver->getConfig().setOption(SMTConfig::o_smt_split_format_length,
                                                                  SMTOption("brief"),
                                                                  msg) &&
            interpret->main_solver->getConfig().setOption(SMTConfig::o_sat_split_units, SMTOption(spts_time), msg) &&
            interpret->main_solver->getConfig().setOption(SMTConfig::o_sat_split_inittune, SMTOption(double(2)), msg) &&
            interpret->main_solver->getConfig().setOption(SMTConfig::o_sat_split_midtune, SMTOption(double(2)), msg) &&
            interpret->main_solver->getConfig().setOption(SMTConfig::o_sat_split_asap, SMTOption(1), msg))) {
        this->report(partitions, msg);
    }
    else {
#ifdef ENABLE_DEBUGING
        std::thread log (Logger::writeIntoFile,false,"PartitionProcess - Main Thread: Start to partitioning",
                     "Recieved command: "+header["command"],getpid());
        log.join();
#endif
        sstat status = interpret->main_solver->solve();
        if (status == s_Undef) {
            std::vector<SplitData>& splits=static_cast<ScatterSplitter&>(interpret->main_solver->getSMTSolver()).splits;
            for (int i = 0; i < splits.size(); i++) {
                std::vector<vec<PtAsgn> > constraints;
                splits[i].constraintsToPTRefs(constraints, interpret->main_solver->getTHandler());
                vec<PTRef> clauses;
                for (int j = 0; j < constraints.size(); j++) {
                    vec<PTRef> clause;
                    for (int k = 0; k < constraints[j].size(); k++) {
                        PTRef pt =
                                constraints[j][k].sgn == l_True ?
                                constraints[j][k].tr :
                                interpret->main_solver->getLogic().mkNot(constraints[j][k].tr);
                        clause.push(pt);
                    }
                    clauses.push(interpret->main_solver->getLogic().mkOr(clause));
                }
                char *str = interpret->main_solver->getTHandler().getLogic().
                        printTerm(interpret->main_solver->getLogic().mkAnd(clauses), false, true);
                partitions.push_back(str);
#ifdef ENABLE_DEBUGING
                std::thread log (Logger::writeIntoFile,false,"PartitionProcess - Main Thread: Finished partitioning",
                     str,getpid());
        log.join();
#endif
                free(str);
            }
            this->report(partitions);
        } else if (status == s_True) {
            this->report(Status::sat);
        }
        else if (status == s_False) {
            this->report(Status::unsat);
        }
        else {
            this->report(partitions, "error during partitioning");
        }
    }
    exit(0);
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
        interpret = new OpenSMTInterpret(header, nullptr, nullptr, config);
        interpret->interpFile((char *) (payload + header["query"]).c_str());

        char *cnf = interpret->main_solver->getSMTSolver().printCnfClauses();
        this->report(header, header["command"], cnf);
        free(cnf);
        exit(0);
    } else {
        char *cnf = interpret->main_solver->getSMTSolver().printCnfClauses();
        this->report(header, header["command"], cnf);
        free(cnf);
    }
}

void SolverProcess::getCnfLearnts(net::Header &header) {
    char *cnf = interpret->main_solver->getSMTSolver().printCnfLearnts();
    this->report(header, header["command"], cnf);
    free(cnf);
}
