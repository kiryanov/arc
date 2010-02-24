import urlparse
import httplib
import time
import traceback
import threading
import random

import arc
from arcom import get_child_nodes, get_child_values_by_name
from arcom import import_class_from_string
from arcom.security import parse_ssl_config
from arcom.service import shepherd_uri, librarian_servicetype, bartender_servicetype, true, parse_node, create_response, shepherd_servicetype

from arcom.xmltree import XMLTree
from storage.common import common_supported_protocols, serialize_ids
from storage.client import LibrarianClient, BartenderClient, ISISClient

from arcom.logger import Logger
log = Logger(arc.Logger(arc.Logger_getRootLogger(), 'Storage.Shepherd'))

from storage.common import ALIVE, CREATING, STALLED, INVALID, DELETED, THIRDWHEEL, OFFLINE

class Shepherd:

    def __init__(self, cfg):
        self.service_is_running = True
        self.ssl_config = parse_ssl_config(cfg)
        
        try:
            backendclass = str(cfg.Get('BackendClass'))
            backendcfg = cfg.Get('BackendCfg')
            self.backend = import_class_from_string(backendclass)(backendcfg, shepherd_uri, self._file_arrived, self.ssl_config)
        except Exception, e:
            log.msg(arc.ERROR, 'Cannot import backend class %(c)s (reason: %(r)s)' % {'c':backendclass, 'r':e})
            raise
        
        try:
            storeclass = str(cfg.Get('StoreClass'))
            storecfg = cfg.Get('StoreCfg')
            self.store = import_class_from_string(storeclass)(storecfg)
        except:
            log.msg(arc.ERROR, 'Cannot import store class', storeclass)
            raise
            
        try:
            self.serviceID = str(cfg.Get('ServiceID'))
        except:
            log.msg(arc.ERROR, 'Cannot get serviceID')
            raise
            
        try:
            self.period = float(str(cfg.Get('CheckPeriod')))
            self.min_interval = float(str(cfg.Get('MinCheckInterval')))
        except:
            log.msg(arc.ERROR, 'Cannot set CheckPeriod, MinCheckInterval')
            raise
                    
        try:
            self.creating_timeout = float(str(cfg.Get('CreatingTimeout')))
        except:
            self.creating_timeout = 0
            
        try:
            self.checksum_lifetime = float(str(cfg.Get('ChecksumLifetime')))
        except:
            self.checksum_lifetime = 3600

        librarian_urls =  get_child_values_by_name(cfg, 'LibrarianURL')
        self.librarian = LibrarianClient(librarian_urls, ssl_config = self.ssl_config)
        if librarian_urls:
            log.msg(arc.INFO,'Got Librarian URLs from the config:', librarian_urls)
        else:
            isis_urls = get_child_values_by_name(cfg, 'ISISURL')
            if not isis_urls:
                log.msg(arc.ERROR, "No Librarian URLs and no ISIS URLs found in the configuration: no self-healing!")
            else:
                log.msg(arc.INFO,'Got ISIS URL, starting isisLibrarianThread')
                threading.Thread(target = self.isisLibrarianThread, args = [isis_urls]).start()

        bartender_urls =  get_child_values_by_name(cfg, 'BartenderURL')
        self.bartender = BartenderClient(bartender_urls, ssl_config = self.ssl_config)
        if bartender_urls:
            log.msg(arc.INFO,'Got Bartender URLs from the config:', bartender_urls)
        else:
            isis_urls = get_child_values_by_name(cfg, 'ISISURL')
            if not isis_urls:
                log.msg(arc.ERROR, "No Bartender URLs and no ISIS URLs found in the configuration: no self-healing!")
            else:
                log.msg(arc.INFO,'Got ISIS URL, starting isisBartenderThread')
                threading.Thread(target = self.isisBartenderThread, args = [isis_urls]).start()

        self.changed_states = self.store.list()
        threading.Thread(target = self.checkingThread, args = [self.period]).start()
        self.doReporting = True
        threading.Thread(target = self.reportingThread, args = []).start()

    def __del__(self):
        print "Shepherd's destructor called"
        self.service_is_running = False
        
    def getSpaceInformation(self):
        free_size = self.backend.getAvailableSpace()
        used_size = 0
        referenceIDs = self.store.list()
        for referenceID in referenceIDs:
            try:
                localData = self.store.get(referenceID)
                size = int(localData['size'])
                used_size += size
            except:
                pass
        total_size = free_size + used_size
        return free_size, used_size, total_size
    
    def isisLibrarianThread(self, isis_urls):
        while self.service_is_running:
            try:
                if self.librarian.urls:
                    time.sleep(30)
                else:
                    time.sleep(3)
                log.msg(arc.INFO,'Getting Librarians from ISISes')
                for isis_url in isis_urls:
                    if not self.service_is_running:
                        return
                    log.msg(arc.INFO,'Trying to get Librarian from', isis_url)
                    isis = ISISClient(isis_url, ssl_config = self.ssl_config)        
                    librarian_urls = isis.getServiceURLs(librarian_servicetype)
                    log.msg(arc.INFO, 'Got Librarian from ISIS:', librarian_urls)
                    if librarian_urls:
                        self.librarian = LibrarianClient(librarian_urls, ssl_config = self.ssl_config)
                        break
            except Exception, e:
                log.msg(arc.WARNING, 'Error in isisLibrarianThread: %s' % e)
                
    
    def isisBartenderThread(self, isis_urls):
        while self.service_is_running:
            try:
                if self.bartender.urls:
                    time.sleep(30)
                else:
                    time.sleep(3)
                log.msg(arc.INFO,'Getting Bartenders from ISISes')
                for isis_url in isis_urls:
                    if not self.service_is_running:
                        return
                    log.msg(arc.INFO,'Trying to get Bartender from', isis_url)
                    isis = ISISClient(isis_url, ssl_config = self.ssl_config)        
                    bartender_urls = isis.getServiceURLs(bartender_servicetype)
                    log.msg(arc.INFO, 'Got Bartender from ISIS:', bartender_urls)
                    if bartender_urls:
                        self.bartender = BartenderClient(bartender_urls, ssl_config = self.ssl_config)
                        break
            except Exception, e:
                log.msg(arc.WARNING, 'Error in isisBartenderThread: %s' % e)
                

    def reportingThread(self):
        # at the first start just wait for a few seconds
        time.sleep(5)
        while self.service_is_running:
            # do this forever
            try:
                # if reporting is on
                if self.doReporting:
                    # this list will hold the list of changed files we want to report
                    filelist = []
                    # when the state of a file changed somewhere the file is appended to the global 'changed_states' list
                    # so this list contains the files which state is changed between the last and the current cycle
                    while len(self.changed_states) > 0: # while there is changed file
                        # get the first one. the changed_states list contains referenceIDs
                        changed = self.changed_states.pop()
                        # get its local data (GUID, size, state, etc.)
                        localData = self.store.get(changed)
                        if not localData.has_key('GUID'):
                            log.msg(arc.VERBOSE, 'Error in shepherd.reportingThread()\n\treferenceID is in changed_states, but not in store')
                        else:
                            # add to the filelist the GUID, the referenceID and the state of the file
                            filelist.append((localData.get('GUID'), changed, localData.get('state')))
                    #print 'reporting', self.serviceID, filelist
                    # call the report method of the librarian with the collected filelist and with our serviceID
                    try:
                        next_report = self.librarian.report(self.serviceID, filelist)
                    except:
                        log.msg(arc.VERBOSE, 'Error sending report message to the Librarian, reason:', traceback.format_exc())
                        # if next_report is below zero, then we will send everything again
                        next_report = -1
                    # we should get the time of the next report
                    # if we don't get any time, do the next report 10 seconds later
                    if not next_report:
                        next_report = 10
                    last_report = time.time()
                    # if the next report time is below zero it means:
                    if next_report < 0: # 'please send all'
                        log.msg(arc.VERBOSE, 'reporting - asked to send all file data again')
                        # add the full list of stored files to the changed_state list - all the files will be reported next time (which is immediately, see below)
                        self.changed_states.extend(self.store.list())
                    # let's wait until there is any changed file or the reporting time is up - we need to do report even if no file changed (as a heartbeat)
                    time.sleep(10)
                    while len(self.changed_states) == 0 and last_report + next_report * 0.5 > time.time():
                        time.sleep(10)
                else:
                    time.sleep(10)
            except:
                log.msg()
                time.sleep(10)
        
    def toggleReport(self, doReporting):
        self.doReporting = doReporting
        return str(self.doReporting)
    
    def _checking_checksum(self, referenceID, localData):
        # get the current state (which is stored locally) or an empty string if it somehow has no state
        state = localData.get('state','')
        # hack: if the file's state is ALIVE then only check the checksum if the last check was a long time ago
        current_time = time.time()
        if state != ALIVE or current_time - localData.get('lastChecksumTime',-1) > self.checksum_lifetime:
            # ask the backend to create the checksum of the file 
            try:
                current_checksum = self.backend.checksum(localData['localID'], localData['checksumType'])
                log.msg(arc.DEBUG, 'self.backend.checksum was called on %(rID)s, the calculated checksum is %(cs)s' % {'rID':referenceID, 'cs':current_checksum})
                self.store.lock()
                try:
                    current_local_data = self.store.get(referenceID)
                    current_local_data['lastChecksumTime'] = current_time
                    current_local_data['lastChecksum'] = current_checksum
                    self.store.set(referenceID, current_local_data)
                    self.store.unlock()
                except:
                    log.msg()
                    self.store.unlock()
                    current_checksum = None
            except:
                current_checksum = None
        else:
            current_checksum = localData.get('lastChecksum', None)
        # get the original checksum
        checksum = localData['checksum']
        #print '-=-', referenceID, state, checksum, current_checksum
        if checksum == current_checksum:
            # if the original and the current checksum is the same, then the replica is valid
            if state in [INVALID, CREATING, STALLED]:
                # if it is currently INVALID or CREATING or STALLED, its state should be changed
                log.msg(arc.VERBOSE, '\nCHECKSUM OK', referenceID)
                self.changeState(referenceID, ALIVE)
                state = ALIVE
            return state
        else:
            # or if the checksum is not the same - we have a corrupt file, or a not-fully-uploaded one
            if state == CREATING: 
                # if the file's local state is CREATING, that's OK, the file is still being uploaded
                if self.creating_timeout:
                    # but if the file is in CREATED state for a long time, then maybe something was wrong, we should change its state
                    now = time.time()
                    if now - float(localData.get('created', now)) > self.creating_timeout:
                        self.changeState(referenceID, STALLED)
                        return STALLED
                return CREATING
            if state in [DELETED, STALLED]:
                # if the file is DELETED or STALLED we don't care if the checksum is wrong
                return state
            if state != INVALID:
                # but if it is not CREATING, not DELETED or STALLED and not INVALID - then it was ALIVE: now its state should be changed to INVALID
                log.msg(arc.VERBOSE, '\nCHECKSUM MISMATCH', referenceID, 'original:', checksum, 'current:', current_checksum)
                self.changeState(referenceID, INVALID)
            return INVALID
        
    def _file_arrived(self, referenceID):
        # this is called usually by the backend when a file arrived (gets fully uploaded)
        # call the checksum checker which will change the state to ALIVE if its checksum is OK, but leave it as CREATING if the checksum is wrong
        # TODO: either this checking should be in seperate thread, or the backend's should call this in a seperate thread?
        localData = self.store.get(referenceID)
        GUID = localData['GUID']
        trials = 3 # if arccp hasn't set the right checksum yet, try to wait for it
        while localData['checksum'] == '' and trials > 0:
            trials = trials - 1
            # first sleep 1 sec, then 2, then 3
            time.sleep(3 - trials)
            # check if the cheksum changed in the Librarian
            metadata = self.librarian.get([GUID])[GUID]
            localData = self._refresh_checksum(referenceID, localData, metadata)
        state = self._checking_checksum(referenceID, localData)
        # if _checking_checksum haven't change the state to ALIVE: the file is corrupt
        # except if the checksum is '', then the arccp hasn't set the right checksum yet
        if state == CREATING and localData['checksum'] != '':
            self.changeState(referenceID, INVALID)

    def _refresh_checksum(self, referenceID, localData, metadata):
        checksum = localData['checksum']
        checksumType = localData['checksumType']
        librarian_checksum = metadata.get(('states','checksum'), checksum)
        librarian_checksumType = metadata.get(('states','checksumType'), checksumType)
        if checksum != librarian_checksum or checksumType != librarian_checksumType:
            # refresh the checksum
            self.store.lock()
            current_local_data = self.store.get(referenceID)
            try:
                if not current_local_data: # what?
                    self.store.unlock()
                else:
                    current_local_data['checksum'] = librarian_checksum
                    current_local_data['checksumType'] = librarian_checksumType
                    log.msg(arc.VERBOSE, 'checksum refreshed', current_local_data)
                    self.store.set(referenceID, current_local_data)
                    self.store.unlock()
                    return current_local_data
            except:
                log.msg()
                self.store.unlock()
                return current_local_data
        return localData

    def checkingThread(self, period):
        # first just wait a few seconds
        time.sleep(10)
        while self.service_is_running:
            # do this forever
            try:
                # get the referenceIDs of all the stored files
                referenceIDs = self.store.list()
                # count them
                number = len(referenceIDs)
                alive_GUIDs = []
                # if there are stored files at all
                if number > 0:
                    # we should check all the files periodically, with a given period, which determines the checking interval for one file
                    interval = period / number
                    # but we don't want to run constantly, after a file is checked we should wait at least a specified amount of time
                    if interval < self.min_interval:
                        interval = self.min_interval
                    log.msg(arc.VERBOSE,'\n', self.serviceID, 'is checking', number, 'files with interval', interval)
                    # randomize the list of files to be checked
                    random.shuffle(referenceIDs)
                    # start checking the first one
                    for referenceID in referenceIDs:
                        try:
                            localData = self.store.get(referenceID)
                            #print localData
                            GUID, localID = localData['GUID'], localData['localID']
                            # first we get the file's metadata from the librarian
                            metadata = self.librarian.get([GUID])[GUID]
                            # check if the cheksum changed in the Librarian
                            localData = self._refresh_checksum(referenceID, localData, metadata)
                            # check the real checksum of the file: if the checksum is OK or not, it changes the state of the replica as well
                            # and it returns the state and the GUID and the localID of the file
                            #   if _checking_checksum changed the state then the new state is returned here:
                            state = self._checking_checksum(referenceID, localData)
                            # now the file's state is according to its checksum
                            # checksum takes time, so refreshing metadata...
                            metadata = self.librarian.get([GUID])[GUID]
                            # if it is CREATING or ALIVE:
                            if state == CREATING or state == ALIVE:
                                # if this metadata is not a valid file then the file must be already removed
                                if metadata.get(('entry', 'type'), '') != 'file':
                                    # it seems this is not a real file anymore
                                    # we should remove it
                                    bsuccess = self.backend.remove(localID)
                                    self.store.set(referenceID, None)
                                # if the file is ALIVE (which means it is not CREATING or DELETED)
                                if state == ALIVE:
                                    if GUID in alive_GUIDs:
                                        # this means that we already have an other alive replica of this file
                                        log.msg(arc.VERBOSE, '\n\nFile', GUID, 'has more than one replicas on this storage element.')
                                        self.changeState(referenceID, DELETED)
                                    else:    
                                        # check the number of needed replicas
                                        needed_replicas = int(metadata.get(('states','neededReplicas'),1))
                                        #print metadata.items()
                                        # find myself among the locations
                                        mylocation = serialize_ids([self.serviceID, referenceID])
                                        myself = [v for (s, p), v in metadata.items() if s == 'locations' and p == mylocation]
                                        #print myself
                                        if not myself or myself[0] != ALIVE:
                                            # if the state of this replica is not proper in the Librarian, fix it
                                            metadata[('locations', mylocation)] = ALIVE
                                            self.changeState(referenceID, ALIVE)
                                        # get the number of shepherds with alive (or creating) replicas
                                        alive_replicas = len(dict([(property.split(' ')[0], value) 
                                                                   for (section, property), value in metadata.items()
                                                                   if section == 'locations' and value in [ALIVE, CREATING]]))
                                        if alive_replicas < needed_replicas:
                                            # if the file has fewer replicas than needed
                                            log.msg(arc.VERBOSE, '\n\nFile', GUID, 'has fewer replicas than needed.')
                                            # we offer our copy to replication
                                            try:
                                                response = self.bartender.addReplica({'checkingThread' : GUID}, common_supported_protocols)
                                                success, turl, protocol = response['checkingThread']
                                            except:
                                                success = ''
                                            #print 'addReplica response', success, turl, protocol
                                            if success == 'done':
                                                # if it's OK, we asks the backend to upload our copy to the TURL we got from the bartender
                                                self.backend.copyTo(localID, turl, protocol)
                                                # TODO: this should be done in some other thread
                                            else:
                                                log.msg(arc.VERBOSE, 'checkingThread error, bartender responded', success)
                                            # so this GUID has an alive replica here
                                            alive_GUIDs.append(GUID)
                                        elif alive_replicas > needed_replicas:
                                            log.msg(arc.VERBOSE, '\n\nFile', GUID, 'has %d more replicas than needed.' % (alive_replicas-needed_replicas))
                                            thirdwheels = len([property for (section, property), value in metadata.items()
                                                               if section == 'locations' and value == THIRDWHEEL])
                                            if thirdwheels == 0:
                                                self.changeState(referenceID, THIRDWHEEL)
                                        else:
                                            # so this GUID has an alive replica here
                                            alive_GUIDs.append(GUID)
                            # or if this replica is not needed
                            elif state == THIRDWHEEL:
                                # check the number of needed replicasa
                                needed_replicas = int(metadata.get(('states','neededReplicas'),1))
                                # and the number of alive replicas
                                alive_replicas = len([property for (section, property), value in metadata.items()
                                                          if section == 'locations' and value == ALIVE])
                                # get the number of THIRDWHEELS not on this Shepherd (self.serviceID)
                                thirdwheels = len([property for (section, property), value in metadata.items()
                                                   if section == 'locations' and value == THIRDWHEEL and not property.startswith(self.serviceID)])
                                my_replicas = len([property for (section, property), value in metadata.items()
                                                   if section == 'locations' and value == ALIVE 
                                                   and property.startswith(self.serviceID)])
                                # if shephered has other alive replicas or no-one else have a thirdwheel replica, 
                                # and the file still has enough replicas, we delete this replica
                                if my_replicas != 0 or (thirdwheels == 0 and alive_replicas >= needed_replicas):
                                    #bsuccess = self.backend.remove(localID)
                                    #self.store.set(referenceID, None)
                                    self.changeState(referenceID, DELETED)
                                # else we sheepishly set the state back to ALIVE
                                else:
                                    self.changeState(referenceID, ALIVE)
                                    state = ALIVE
                            # or if this replica is INVALID
                            elif state == INVALID:
                                log.msg(arc.VERBOSE, '\n\nI have an invalid replica of file', GUID, '- now I remove it.')
                                self.changeState(referenceID, DELETED)      
                                #
                                # # disabling pull-method of self-healing # #
                                #                          
                                #my_replicas = len([property for (section, property), value in metadata.items()
                                #                   if section == 'locations' and value in [ALIVE,CREATING] 
                                #                   and property.startswith(self.serviceID)])
                                # we try to get a valid one by simply downloading this file
                                #try:
                                #    response = self.bartender.getFile({'checkingThread' : (GUID, common_supported_protocols)})
                                #    success, turl, protocol = response['checkingThread']
                                #except:
                                #    success = traceback.format_exc()
                                #if success == 'done':
                                #    # if it's OK, then we change the state of our replica to CREATING
                                #    self.changeState(referenceID, CREATING)
                                #    # then asks the backend to get the file from the TURL we got from the bartender
                                #    self.backend.copyFrom(localID, turl, protocol)
                                #    # and after this copying is done, we indicate that it's arrived
                                #    self._file_arrived(referenceID)
                                #    # TODO: this should be done in some other thread
                                #else:
                                #    log.msg(arc.VERBOSE, 'checkingThread error, bartender responded', success)
                            elif state == OFFLINE:
                                # online now
                                state = ALIVE
                                self.changeState(referenceID, ALIVE)
                            if state == DELETED:
                                # remove replica if marked it as deleted
                                bsuccess = self.backend.remove(localID)
                                self.store.set(referenceID, None)
                        except:
                            log.msg(arc.VERBOSE, 'ERROR checking checksum of %(rID)s, reason: %(r)s' % {'rID':referenceID, 'r':traceback.format_exc()})
                        # sleep for interval +/- 0.5*interval seconds to avoid race condition
                        time.sleep(interval+((random.random()-0.5)*interval))
                else:
                    time.sleep(period)
            except:
                log.msg()

    def changeState(self, referenceID, newState, onlyIf = None):
        # change the file's local state and add it to the list of changed files
        self.store.lock()
        try:
            localData = self.store.get(referenceID)
            if not localData: # this file is already removed
                self.store.unlock()
                return False
            oldState = localData['state']
            log.msg(arc.VERBOSE, 'changeState', referenceID, oldState, '->', newState)
            # if a previous state is given, change only if the current state is the given state
            if onlyIf and oldState != onlyIf:
                self.store.unlock()
                return False
            localData['state'] = newState
            self.store.set(referenceID, localData)
            self.store.unlock()
            # append it to the list of changed files (these will be reported)
            self.changed_states.append(referenceID)
        except:
            log.msg()
            self.store.unlock()
            return False

    def get(self, request):
        response = {}
        for requestID, getRequestData in request.items():
            log.msg(arc.VERBOSE, '\n\n', getRequestData)
            referenceID = dict(getRequestData)['referenceID']
            protocols = [value for property, value in getRequestData if property == 'protocol']
            #print 'Shepherd.get:', referenceID, protocols
            localData = self.store.get(referenceID)
            #print 'localData:', localData
            if localData.get('state', INVALID) == ALIVE:
                if localData.has_key('localID'):
                    localID = localData['localID']
                    checksum = localData['checksum']
                    checksumType = localData['checksumType']
                    protocol_match = self.backend.matchProtocols(protocols)
                    if protocol_match:
                        protocol = protocol_match[0]
                        try:
                            turl = self.backend.prepareToGet(referenceID, localID, protocol)
                            if turl:
                                response[requestID] = [('TURL', turl), ('protocol', protocol),
                                    ('checksum', localData['checksum']), ('checksumType', localData['checksumType'])]
                            else:
                                response[requestID] = [('error', 'internal error (empty TURL)')]
                        except:
                            log.msg()
                            response[requestID] = [('error', 'internal error (prepareToGet exception)')]
                    else:
                        response[requestID] = [('error', 'no supported protocol found')]
                else:
                    response[requestID] = [('error', 'no such referenceID')]
            else:
                response[requestID] = [('error', 'file is not alive')]
        return response

    def put(self, request):
        #print request
        response = {}
        for requestID, putRequestData in request.items():
            protocols = [value for property, value in putRequestData if property == 'protocol']
            protocol_match = self.backend.matchProtocols(protocols)
            if protocol_match:
                # just the first protocol
                protocol = protocol_match[0]
                acl = [value for property, value in putRequestData if property == 'acl']
                # create a dictionary from the putRequestData which contains e.g. 'size', 'GUID', 'checksum', 'checksumType'
                requestData = dict(putRequestData)
                size = int(requestData.get('size'))
                # ask the backend if there is enough space 
                availableSpace = self.backend.getAvailableSpace()
                if availableSpace and availableSpace < size:
                    response[requestID] = [('error', 'not enough space')]
                else:
                    GUID = requestData.get('GUID', None)
                    already_have_this_file = False
                    if GUID:
                        referenceIDs = self.store.list()
                        for referenceID in referenceIDs:
                            try:
                                localData = self.store.get(referenceID)
                                if localData['GUID'] == GUID and localData['state'] == ALIVE:
                                    already_have_this_file = True;
                                    break
                            except:
                                log.msg()
                                pass
                    if already_have_this_file:
                        response[requestID] = [('error', 'already have this file')]
                    else:
                        # create a new referenceIDs
                        referenceID = arc.UUID()
                        # ask the backend to create a local ID
                        localID = self.backend.generateLocalID()
                        # create the local data of the new file
                        file_data = {'localID' : localID,
                            'GUID' : requestData.get('GUID', None),
                            'checksum' : requestData.get('checksum', None),
                            'checksumType' : requestData.get('checksumType', None),
                            'lastChecksumTime' : -1,
                            'lastChecksum' : '',
                            'size' : size,
                            'acl': acl,
                            'created': str(time.time()),
                            'state' : CREATING} # first it has the state: CREATING
                        try:
                            # ask the backend to initiate the transfer
                            turl = self.backend.prepareToPut(referenceID, localID, protocol)
                            if turl:
                                # add the returnable data to the response dict
                                response[requestID] = [('TURL', turl), ('protocol', protocol), ('referenceID', referenceID)]
                                # store the local data
                                self.store.set(referenceID, file_data)
                                # indicate that this file is 'changed': it should be reported in the next reporting cycle (in reportingThread)
                                self.changed_states.append(referenceID)
                            else:
                                response[requestID] = [('error', 'internal error (empty TURL)')]
                        except Exception, e:
                            log.msg()
                            response[requestID] = [('error', 'internal error (prepareToPut exception: %s)' % e)]
            else:
                response[requestID] = [('error', 'no supported protocol found')]
        return response

    def delete(self,request):
        response = {}
        for requestID, referenceID in request.items():
            localData = self.store.get(referenceID)
            try:
                # note that actual deletion is done in self.reportingThread
                self.changeState(referenceID, DELETED)
                response[requestID] = 'deleted'
            except:
                response[requestID] = 'nosuchfile'
        return response


    def stat(self, request):
        properties = ['state', 'checksumType', 'checksum', 'acl', 'size', 'GUID', 'localID']
        response = {}
        for requestID, referenceID in request.items():
            localData = self.store.get(referenceID)
            response[requestID] = [referenceID]
            for p in properties:
                response[requestID].append(localData.get(p, None))
        return response

from arcom.service import Service

class ShepherdService(Service):

    def __init__(self, cfg):
        try:
            serviceID = str(cfg.Get('ServiceID')).split('/')[-1]
        except:
            serviceID = "Shepherd"
        self.service_name = serviceID
        # names of provided methods
        request_names = ['get', 'put', 'stat', 'delete', 'toggleReport']
        # create the business-logic class
        self.shepherd = Shepherd(cfg)
        # get the additional request names from the backend
        backend_request_names = self.shepherd.backend.public_request_names
        # bring the additional request methods into the namespace of this object
        for name in backend_request_names:
            if not hasattr(self, name):
                setattr(self, name, getattr(self.shepherd.backend, name))
                request_names.append(name)
        # call the Service's constructor
        Service.__init__(self, [{'request_names' : request_names, 'namespace_prefix': 'she', 'namespace_uri': shepherd_uri}], cfg)

    def stat(self, inpayload):
        request = parse_node(inpayload.Child().Child(), ['requestID', 'referenceID'], single = True)
        response = self.shepherd.stat(request)
        #print response
        return create_response('she:stat',
            ['she:requestID', 'she:referenceID', 'she:state', 'she:checksumType', 'she:checksum', 'she:acl', 'she:size', 'she:GUID', 'she:localID'], response, self._new_soap_payload())
    

    def delete(self, inpayload):
        request = parse_node(inpayload.Child().Child(), ['requestID', 'referenceID'], single = True)
        response = self.shepherd.delete(request)
        tree = XMLTree(from_tree = 
            ('she:deleteResponseList',[
                ('she:deleteResponseElement',[
                    ('she:requestID', requestID),
                    ('she:status', status)
                    ]) for requestID, status in response.items()
                ])
            )
        out = self._new_soap_payload()
        response_node = out.NewChild('deleteresponse')
        tree.add_to_node(response_node)
        return out

    def _putget_in(self, putget, inpayload):
        request = dict([
            (str(node.Get('requestID')), [
                (str(n.Get('property')), str(n.Get('value')))
                    for n in get_child_nodes(node.Get(putget + 'RequestDataList'))
            ]) for node in get_child_nodes(inpayload.Child().Child())])
        return request

    def _putget_out(self, putget, response):
        #print response
        tree = XMLTree(from_tree =
            ('she:' + putget + 'ResponseList', [
                ('she:' + putget + 'ResponseElement', [
                    ('she:requestID', requestID),
                    ('she:' + putget + 'ResponseDataList', [
                        ('she:' + putget + 'ResponseDataElement', [
                            ('she:property', property),
                            ('she:value', value)
                        ]) for property, value in responseData
                    ])
                ]) for requestID, responseData in response.items()
            ])
        )
        out = self._new_soap_payload()
        response_node = out.NewChild(putget + 'response')
        tree.add_to_node(response_node)
        return out

    def get(self, inpayload):
        # if inpayload.auth:
        #     print 'Shepherd auth "get": ', inpayload.auth
        request = self._putget_in('get', inpayload)
        response = self.shepherd.get(request)
        return self._putget_out('get', response)

    def put(self, inpayload):
        request = self._putget_in('put', inpayload)
        response = self.shepherd.put(request)
        return self._putget_out('put', response)

    def toggleReport(self, inpayload):
        doReporting = str(inpayload.Child().Get('doReporting'))
        response = self.shepherd.toggleReport(doReporting == true)
        out = self._new_soap_payload()
        response_node = out.NewChild('lbr:toggleReportResponse')
        response_node.Set(response)
        return out

    def RegistrationCollector(self, doc):
        regentry = arc.XMLNode('<RegEntry />')
        regentry.NewChild('SrcAdv').NewChild('Type').Set(shepherd_servicetype)
        #Place the document into the doc attribute
        doc.Replace(regentry)
        return True

    def GetAdditionalLocalInformation(self, service_node):
        service_node.Name('StorageService')
        service_node.NewChild('Type').Set(shepherd_servicetype)
        capacity_node = service_node.NewChild('StorageServiceCapacity')
        free_size, used_size, total_size = self.shepherd.getSpaceInformation()
        print free_size, used_size, total_size
        gigabyte = 1073741824.0
        capacity_node.NewChild('FreeSize').Set(str(free_size/gigabyte))
        capacity_node.NewChild('UsedSize').Set(str(used_size/gigabyte))
        capacity_node.NewChild('TotalSize').Set(str(total_size/gigabyte))
