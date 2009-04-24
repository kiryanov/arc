// -*- indent-tabs-mode: nil -*-

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <cstdlib>
#include <algorithm>

#include "RandomBroker.h"

namespace Arc {

  RandomBroker::RandomBroker(Config *cfg)
    : Broker(cfg) {}

  RandomBroker::~RandomBroker() {}

  Plugin* RandomBroker::Instance(PluginArgument *arg) {
    ACCPluginArgument *accarg =
      arg ? dynamic_cast<ACCPluginArgument*>(arg) : NULL;
    if (!accarg)
      return NULL;
    return new RandomBroker((Config*)(*accarg));
  }

  void RandomBroker::SortTargets() {

    std::vector<ExecutionTarget>::iterator iter = PossibleTargets.begin();

    logger.msg(DEBUG, "Matching against job description, following targets possible for RandomBroker: %d", PossibleTargets.size());

    iter = PossibleTargets.begin();

    for (int i = 1; iter != PossibleTargets.end(); iter++, i++)
      logger.msg(DEBUG, "%d. Cluster: %s", i, iter->DomainName);

    int i, j;
    std::srand(time(NULL));

    for (unsigned int k = 1; k < 2 * (std::rand() % PossibleTargets.size()) + 1; k++) {
      i = rand() % PossibleTargets.size();
      j = rand() % PossibleTargets.size();
      std::iter_swap(PossibleTargets.begin() + i, PossibleTargets.begin() + j);
    }

    logger.msg(DEBUG, "Best targets are: %d", PossibleTargets.size());

    iter = PossibleTargets.begin();

    for (int i = 1; iter != PossibleTargets.end(); iter++, i++)
      logger.msg(DEBUG, "%d. Cluster: %s", i, iter->DomainName);
    TargetSortingDone = true;
  }

} // namespace Arc
