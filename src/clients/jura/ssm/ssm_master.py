#!/usr/bin/env python

import os
import sys
import platform
import time
import logging
import ConfigParser
import stomp

sys.path.insert(len(sys.path)+1, sys.path[len(sys.path)-1][:5]+"local/"+ sys.path[len(sys.path)-1][5:])

from SecureStompMessenger import SecureStompMessenger, \
    Config, ProducerConfig, ConsumerConfig, SSMLoggerID
from message_db import MessageDB

log = None


def get_basic_config(config):
    '''
    Read general SSM configuration info from a ConfigParser object,
    and return Config object to pass to the SSM.
    '''
    
    c = Config()
    # Start the logging
    c.logconf = config.get('logging', 'log-conf-file')
    c.logconf = os.path.normpath(os.path.expandvars(c.logconf))
    if not os.access(c.logconf, os.R_OK):
        raise Exception("Can't find or open logging config file " + c.logconf)
    logging.config.fileConfig(c.logconf)
    global log
    log = logging.getLogger(SSMLoggerID)  
    
    try:
        c.host = config.get('broker','host')
        c.port = config.getint('broker','port')
        
        # As well as fetching filepaths, expand any environment variables.
        c.capath = config.get('certificates','cadir')
        c.capath = os.path.normpath(os.path.expandvars(c.capath))
        c.certificate = config.get('certificates','certificate')
        c.certificate = os.path.normpath(os.path.expandvars(c.certificate))
        c.key = config.get('certificates','key')
        c.key = os.path.normpath(os.path.expandvars(c.key))
    except Exception, err:
        pass
        
    # Check CRLs unless specifically told not to.
    c.check_crls = not (config.get('certificates', 'check-crls').lower() == 'false')

    return c

    
def get_consumer_config(config):
    '''
    Read SSM consumer configuration info from a ConfigParser object,
    and return ConsumerConfig object to pass to the SSM.
    '''
    
    c = ConsumerConfig()
        
    c.listen_to = config.get('consumer','topic')
    c.valid_dn = config.get('consumer','valid-dns')
    c.valid_dn = os.path.normpath(os.path.expandvars(c.valid_dn))
    c.read_valid_dn_interval = config.getint('consumer', 'read-valid-dns')
        
    if not os.path.isfile(c.valid_dn):
        log.warn("Valid DN file doesn't exist: not starting the consumer")
        raise Exception(c.valid_dn + ' not a file.')
    
    return c
        
        
def get_producer_config(config):
    '''
    Read SSM producer configuration info from a ConfigParser object,
    and return ProducerConfig object to pass to the SSM.
    '''    
    c = ProducerConfig()
    
    c.consumerDN = config.get('producer','consumer-dn')
    try:
        c.send_to = config.get('producer','topic')
    except Exception, err:
        pass

    # How often we clear the consumer certificate, enforcing a re-request
    c.clear_cert_interval = config.getint('producer', 'clear-consumer-cert')
    # perform variable expansions
    ack = config.get('producer', 'ack')
    ack = ack.replace('$host', os.uname()[1])
    ack = ack.replace('$pid', str(os.getpid()))

    c.ack_queue = ack
    
    return c


def runprocess(config_file, host, port, topic, key, cert, cadir, message_path):
    '''
    Retrieve the configuration from the file and start the SSM.
    '''
    # Retrieve configuration from file
    config = ConfigParser.ConfigParser()
    config.read(config_file)
    
    # Required configuration
    try:
        # SSM configuration
        configuration = get_basic_config(config)
        configuration.host = host
        configuration.port = port
        configuration.key = key
        configuration.certificate = cert
        configuration.capath = cadir
        
        # MessageDB configuration
        messages = config.get('messagedb','path')
        messages = message_path
        messages = os.path.normpath(os.path.expandvars(messages))
        # Only turn on test mode if specified.
        test = (config.get('messagedb', 'test').lower() == 'true')

    except Exception, err:
        print "Error in configuration file: " + str(err)
        print "System will exit."
        sys.exit(1)

    # Optional consumer configuration
    try:
        consumer_config = get_consumer_config(config)
    except:
        consumer_config = None

    # Optional producer configuration
    try:
        producer_config = get_producer_config(config)
        producer_config.send_to = topic
    except:
        producer_config = None
       
    # Try to connect to the broker
    try:

        messagedb = MessageDB(messages, test)
        listener = SecureStompMessenger(messagedb, configuration, producer_config, consumer_config)

    except KeyboardInterrupt:
        pass
    except Exception, err:
        print "FATAL STARTUP ERROR: " + str(err)
        log.error("SSM failed to start: " + str(err))
        sys.exit(1)
        
    #    
    # Finally, as long as initial checks pass, set it going.
    #
    
    log.info("Starting the SSM...")
    
    try:
        listener.startup()
    except Exception, err:
        print "Failed to connect: " + str(err)
        print "SSM will exit."
        log.info("SSM failed to start: " + str(err))
        sys.exit(1)
    
    #
    # Successful connection, listen for incoming messages
    #
    
    log.info("The SSM started successfully.")
    log.info("python version: " + platform.python_version())
    log.info("stomppy version: " + str(stomp.__version__))
    log.debug('Ready for service')
#    while True:
    try:

        time.sleep(1.0)

        # Process all the messages one at a time before continuing
        while listener.process_outgoing():
            pass

    except KeyboardInterrupt:
        raise
    except Exception, err:
        log.error('Error in message processing: '+str(err))
        sys.exit(1)
    
    listener.stop()
    
    
################################################################################

if __name__ == "__main__":
    if (len(sys.argv) != 9):
        print "Usage: python ssm-master.py <path-to-config-file> <hostname> <port> <topic> <key-path> <cert-path> <cadir-path> <path-to-messages-files>"
        sys.exit(1)
    
    candidate = sys.argv[1]
    for dirname in sys.path:
        candidate = os.path.join(dirname, sys.argv[1])
        if os.path.isfile(candidate):
            os.environ["CONF_HOME"]=dirname
            break
    
    runprocess(candidate, sys.argv[2], sys.argv[3], sys.argv[4], sys.argv[5], sys.argv[6], sys.argv[7], sys.argv[8])
    sys.exit(0)

