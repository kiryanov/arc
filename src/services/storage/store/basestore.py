import os, threading, traceback


def logprint(*args):
    if args:
        print ' '.join([str(arg) for arg in args])
    else:
        traceback.print_exc()

class BaseStore:
    
    def __init__(self, storecfg, non_existent_object = {}, log = None):
        """ Constructor of BaseStore.

        BaseStore(storecfg)

        'storecfg' is an XMLNode with a 'DataDir'
        'non_existent_object' will be returned if an object not found
        """
        if log:
            self.log = log
        else:
            self.log = logprint
        # get the datadir from the storecfg XMLNode
        self.datadir = str(storecfg.Get('DataDir'))
        # set the value which we should return if there an object does not exist
        self.non_existent_object = non_existent_object
        # if the given data directory does not exist, try to create it
        if not os.path.exists(self.datadir):
            os.mkdir(self.datadir)
        # initialize a lock which can be used to avoid race conditions
        self.llock = threading.Lock()

    def lock(self, blocking = True):
        """ Acquire the lock.

        lock(blocking = True)

        'blocking': if blocking is True, then this only returns when the lock is acquired.
        If it is False, then it returns immediately with False if the lock is not available,
        or with True if it could be acquired.
        """
        return self.llock.acquire(blocking)

    def unlock(self):
        """ Release the lock.

        unlock()
        """
        self.llock.release()
        