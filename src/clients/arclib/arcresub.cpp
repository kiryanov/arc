// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <list>
#include <string>
#include <sys/types.h>
#include <sys/stat.h>

#include <arc/ArcLocation.h>
#include <arc/IString.h>
#include <arc/Logger.h>
#include <arc/StringConv.h>
#include <arc/URL.h>
#include <arc/UserConfig.h>
#include <arc/client/Job.h>
#include <arc/client/JobSupervisor.h>

#include "utils.h"

#ifdef TEST
#define RUNRESUB(X) test_arcresub_##X
#else
#define RUNRESUB(X) X
#endif
int RUNRESUB(main)(int argc, char **argv) {

  setlocale(LC_ALL, "");

  Arc::Logger logger(Arc::Logger::getRootLogger(), "arcresub");
  Arc::LogStream logcerr(std::cerr);
  logcerr.setFormat(Arc::ShortFormat);
  Arc::Logger::getRootLogger().addDestination(logcerr);
  Arc::Logger::getRootLogger().setThreshold(Arc::WARNING);

  Arc::ArcLocation::Init(argv[0]);

  ClientOptions opt(ClientOptions::CO_RESUB, istring("[job ...]"));

  std::list<std::string> jobIDsOrNames = opt.Parse(argc, argv);

  if (opt.showversion) {
    std::cout << Arc::IString("%s version %s", "arcresub", VERSION)
              << std::endl;
    return 0;
  }

  // If debug is specified as argument, it should be set before loading the configuration.
  if (!opt.debug.empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(opt.debug));

  Arc::UserConfig usercfg(opt.conffile, opt.joblist);
  if (!usercfg) {
    logger.msg(Arc::ERROR, "Failed configuration initialization");
    return 1;
  }

  if (opt.show_plugins) {
    std::list<std::string> types;
    types.push_back("HED:Submitter");
    types.push_back("HED:TargetRetriever");
    types.push_back("HED:Broker");
    showplugins("arcresub", types, logger, usercfg.Broker().first);
    return 0;
  }

  if (!checkproxy(usercfg)) {
    return 1;
  }

  if (opt.debug.empty() && !usercfg.Verbosity().empty())
    Arc::Logger::getRootLogger().setThreshold(Arc::string_to_level(usercfg.Verbosity()));

  if (opt.same && opt.notsame) {
    logger.msg(Arc::ERROR, "--same and --not-same cannot be specified together.");
    return 1;
  }

  for (std::list<std::string>::const_iterator it = opt.jobidinfiles.begin(); it != opt.jobidinfiles.end(); it++) {
    if (!Arc::Job::ReadJobIDsFromFile(*it, jobIDsOrNames)) {
      logger.msg(Arc::WARNING, "Cannot read specified jobid file: %s", *it);
    }
  }

  if (opt.timeout > 0)
    usercfg.Timeout(opt.timeout);

  if (!opt.broker.empty())
    usercfg.Broker(opt.broker);

  if ((!opt.joblist.empty() || !opt.status.empty()) && jobIDsOrNames.empty() && opt.clusters.empty())
    opt.all = true;

  if (jobIDsOrNames.empty() && opt.clusters.empty() && !opt.all) {
    logger.msg(Arc::ERROR, "No jobs given");
    return 1;
  }

  std::list<std::string> rejectClusters;
  splitendpoints(opt.clusters, rejectClusters);
  if (!usercfg.ResolveAliases(opt.clusters, Arc::COMPUTING) || !usercfg.ResolveAliases(rejectClusters, Arc::COMPUTING)) {
    return 1;
  }

  std::list<Arc::Job> jobs;
  if (!Arc::Job::ReadJobsFromFile(usercfg.JobListFile(), jobs, jobIDsOrNames, opt.all, opt.clusters, rejectClusters)) {
    logger.msg(Arc::ERROR, "Unable to read job information from file (%s)", usercfg.JobListFile());
    return 1;
  }

  for (std::list<std::string>::const_iterator itJIDOrName = jobIDsOrNames.begin();
       itJIDOrName != jobIDsOrNames.end(); ++itJIDOrName) {
    std::cout << Arc::IString("Warning: Job not found in job list: %s", *itJIDOrName) << std::endl;
  }

  if (!opt.qlusters.empty() || !opt.indexurls.empty()) {
    usercfg.ClearSelectedServices();
    if (!opt.qlusters.empty()) {
      usercfg.AddServices(opt.qlusters, Arc::COMPUTING);
    }
    if (!opt.indexurls.empty()) {
      usercfg.AddServices(opt.indexurls, Arc::INDEX);
    }
  }

  Arc::JobSupervisor jobmaster(usercfg, jobs);
  if (!jobmaster.JobsFound()) {
    std::cout << Arc::IString("No jobs selected for resubmission") << std::endl;
    return 0;
  }

  std::list<Arc::Job> resubmittedJobs;
  std::list<Arc::URL> notresubmitted;
  // same + 2*notsame in {0,1,2}. same and notsame cannot both be true, see above.
  int retval = (int)!jobmaster.Resubmit(opt.status, (int)opt.same + 2*(int)opt.notsame, resubmittedJobs, notresubmitted);
  if (retval == 0 && resubmittedJobs.empty()) {
    std::cout << Arc::IString("No jobs to resubmit with the specified status") << std::endl;
    return 0;
  }

  for (std::list<Arc::Job>::const_iterator it = resubmittedJobs.begin();
       it != resubmittedJobs.end(); ++it) {
    std::cout << Arc::IString("Job submitted with jobid: %s", it->IDFromEndpoint.str()) << std::endl;
  }

  if (!resubmittedJobs.empty() && !Arc::Job::WriteJobsToFile(usercfg.JobListFile(), resubmittedJobs)) {
    std::cout << Arc::IString("Warning: Failed to lock job list file %s", usercfg.JobListFile()) << std::endl;
    std::cout << Arc::IString("         To recover missing jobs, run arcsync") << std::endl;
    retval = 1;
  }

  if (!opt.jobidoutfile.empty() && !Arc::Job::WriteJobIDsToFile(resubmittedJobs, opt.jobidoutfile)) {
    logger.msg(Arc::WARNING, "Cannot write jobids to file (%s)", opt.jobidoutfile);
    retval = 1;
  }

  // Get job IDs of jobs to kill.
  std::list<Arc::URL> jobstobekilled;
  for (std::list<Arc::Job>::const_iterator it = resubmittedJobs.begin();
       it != resubmittedJobs.end(); ++it) {
    if (!it->ActivityOldID.empty()) {
      jobstobekilled.push_back(it->ActivityOldID.back());
    }
  }

  std::list<Arc::URL> notkilled;
  std::list<Arc::URL> killedJobs = jobmaster.Cancel(jobstobekilled, notkilled);
  for (std::list<Arc::URL>::const_iterator it = notkilled.begin();
       it != notkilled.end(); ++it) {
    logger.msg(Arc::WARNING, "Resubmission of job (%s) succeeded, but killing the job failed - it will still appear in the job list", it->str());
    retval = 1;
  }

  if (!opt.keep) {
    std::list<Arc::URL> notcleaned, cleanedJobs;
    if (!jobmaster.CleanByIDs(killedJobs, cleanedJobs, notcleaned)) {
      retval = 1;
    }
    for (std::list<Arc::URL>::const_iterator it = notcleaned.begin();
         it != notcleaned.end(); ++it) {
      logger.msg(Arc::WARNING, "Resubmission of job (%s) succeeded, but cleaning the job failed - it will still appear in the job list", it->str());
    }

    if (!Arc::Job::RemoveJobsFromFile(usercfg.JobListFile(), cleanedJobs)) {
      std::cout << Arc::IString("Warning: Failed to lock job list file %s", usercfg.JobListFile()) << std::endl;
      std::cout << Arc::IString("         Use arcclean to remove non-existing jobs") << std::endl;
      retval = 1;
    }
  }

  if ((resubmittedJobs.size() + notresubmitted.size()) > 1) {
    std::cout << std::endl << Arc::IString("Job resubmission summary:") << std::endl;
    std::cout << "-----------------------" << std::endl;
    std::cout << Arc::IString("%d of %d jobs were resubmitted", resubmittedJobs.size(), resubmittedJobs.size() + notresubmitted.size()) << std::endl;
    if (!notresubmitted.empty()) {
      std::cout << Arc::IString("The following %d were not resubmitted", notresubmitted.size()) << std::endl;
      for (std::list<Arc::URL>::const_iterator it = notresubmitted.begin();
           it != notresubmitted.end(); ++it) {
        std::cout << it->str() << std::endl;
      }
    }
  }

  return retval;
}
