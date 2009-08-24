#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include <arc/win32.h>
#endif

#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>

#include <arc/ArcConfig.h>
#include <arc/ArcLocation.h>
#include <arc/Logger.h>
#include <arc/XMLNode.h>
#include <arc/message/MCCLoader.h>

#include "daemon.h"
#include "../options.h"

Arc::Daemon *main_daemon;
Arc::Config config;
Arc::MCCLoader *loader;
Arc::Logger& logger = Arc::Logger::rootLogger;

static void shutdown(int)
{
    logger.msg(Arc::DEBUG, "shutdown");
    delete loader;
    delete main_daemon;
    logger.msg(Arc::VERBOSE, "exit");
    _exit(0);
}

static void merge_options_and_config(Arc::Config& cfg, Arc::ServerOptions& opt)
{   
    Arc::XMLNode srv = cfg["Server"];
    if (!(bool)srv) {
      logger.msg(Arc::ERROR, "No server config part of config file");
      return;
    }

    if (opt.pid_file != "") {
        if (!(bool)srv["PidFile"]) {
           srv.NewChild("PidFile")=opt.pid_file;
        } else {
            srv["PidFile"] = opt.pid_file;
        }
    }

    if (opt.foreground == true) {
        if (!(bool)srv["Foreground"]) {
            srv.NewChild("Foreground");
        }
    }

    if (opt.user != "") {
        if (!(bool)srv["User"]) {
            srv.NewChild("User") = opt.user;
        } else {
            srv["User"] = opt.user;
        }
    }

    if (opt.group != "") {
        if (!(bool)srv["Group"]) {
            srv.NewChild("Group") = opt.group;
        } else {
            srv["Gser"] = opt.group;
        }
    }
}

static std::string init_logger(Arc::Config& cfg)
{   
    /* setup root logger */
    Arc::XMLNode log = cfg["Server"]["Logger"];
    Arc::LogStream* sd = NULL; 
    std::string log_file = (std::string)log;
    std::string str = (std::string)log.Attribute("level");
    if(!str.empty()) {
      Arc::LogLevel level = Arc::string_to_level(str);
      Arc::Logger::rootLogger.setThreshold(level); 
    }
    if(!log_file.empty()) {
      std::fstream *dest = new std::fstream(log_file.c_str(), std::fstream::out | std::fstream::app);
      if(!(*dest)) {
        logger.msg(Arc::ERROR,"Failed to open log file: %s",log_file);
        _exit(1);
      }
      sd = new Arc::LogStream(*dest); 
    }
    Arc::Logger::rootLogger.removeDestinations();
    if(sd) Arc::Logger::rootLogger.addDestination(*sd);
    if ((bool)cfg["Server"]["Foreground"]) {
      logger.msg(Arc::INFO, "Start foreground");
      Arc::LogStream *err = new Arc::LogStream(std::cerr);
      Arc::Logger::rootLogger.addDestination(*err);
    }
    return log_file;
}

static uid_t get_uid(const std::string &name)
{
    struct passwd *ent;    
    if (name[0] == '#') {
        return (atoi(&(name.c_str()[1])));
    }
    if (!(ent = getpwnam(name.c_str()))) {        
        std::cerr << "Bad user name" << std::endl;
        exit(1);
    }   
    return (ent->pw_uid);
}

static gid_t get_gid(const std::string &name)
{
    struct group *ent;    
    if (name[0] == '#') {
        return (atoi(&(name.c_str()[1])));
    }
    if (!(ent = getgrnam(name.c_str()))) {        
        std::cerr << "Bad user name" << std::endl;
        exit(1);
    }   
    return (ent->gr_gid);
}

int main(int argc, char **argv)
{
    // Ignore some signals
    signal(SIGTTOU,SIG_IGN);
    signal(SIGPIPE,SIG_IGN);
    // Temporary stderr destination for error messages
    Arc::LogStream logcerr(std::cerr);
    Arc::Logger::getRootLogger().addDestination(logcerr);
    /* Create options parser */
    Arc::ServerOptions options;

    if((argc>0) && (argv[0])) Arc::ArcLocation::Init(argv[0]);

    try {
        std::list<std::string> params = options.Parse(argc, argv);
        if (params.size() == 0) {
            /* Load and parse config file */
            config.parse(options.config_file.c_str());
            if(!config) {
                logger.msg(Arc::ERROR, "Failed to load service configuration from file %s", options.config_file);
                exit(1);
            }

            if(!MatchXMLName(config,"ArcConfig")) {
                logger.msg(Arc::ERROR, "Configuration root element is not <ArcConfig>");
                exit(1);
            }

            /* overwrite config variables by cmdline options */
            merge_options_and_config(config, options);
            std::string pid_file = (std::string)config["Server"]["PidFile"];
            /* initalize logger infrastucture */
            std::string root_log_file = init_logger(config);
            std::string user = (std::string)config["Server"]["User"];
            std::string group = (std::string)config["Server"]["Group"];
            // set signal handlers 
            signal(SIGTERM, shutdown);
            
            // switch user
            if (getuid() == 0) { // are we root?
                /* switch group it is specified */
                if (!group.empty()) {
                    gid_t g = get_gid(group);
                    if (setgid(g) != 0) {
                        logger.msg(Arc::ERROR, "Cannot switch to group (%s)", group);
                        exit(1);
                    }
                }
                /* switch user if it is specied */ 
                if (!user.empty()) {
                    uid_t u = get_uid(user);
                    if (setuid(u) != 0) {
                        logger.msg(Arc::ERROR, "Cannot switch to user (%s)", user);
                        exit(1);
                    }
                }
            }
            // demonize if the foreground options was not set
            if (!(bool)config["Server"]["Foreground"]) {
                main_daemon = new Arc::Daemon(pid_file, root_log_file);
            } else {
                signal(SIGINT, shutdown);
            }

            // bootstrap
            loader = new Arc::MCCLoader(config);
            logger.msg(Arc::INFO, "Service side MCCs are loaded");
            // sleep forever
            for (;;) {
                sleep(INT_MAX);
            }
        }
    } catch (const Glib::Error& error) {
      logger.msg(Arc::ERROR, error.what());
    }

    return 0;
}
