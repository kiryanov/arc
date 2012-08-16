from twisted.application import internet, service
from twisted.web import resource, server

from acix.core import ssl
from acix.cacheserver import pscan, cache, cacheresource


# -- constants
SSL_DEFAULT = True
CACHE_TCP_PORT = 5080
CACHE_SSL_PORT = 5443
DEFAULT_CAPACITY = 30000              # number of files in cache
DEFAULT_CACHE_REFRESH_INTERVAL = 600  # seconds between updating cache


def createCacheApplication(use_ssl=SSL_DEFAULT, port=None, cache_dir=None,
                           capacity=DEFAULT_CAPACITY, refresh_interval=DEFAULT_CACHE_REFRESH_INTERVAL):

    scanner = pscan.CacheScanner(cache_dir)
    cs = cache.Cache(scanner, capacity, refresh_interval)

    cr = cacheresource.CacheResource(cs)

    siteroot = resource.Resource()
    dataroot = resource.Resource()

    dataroot.putChild('cache', cr)
    siteroot.putChild('data', dataroot)

    site = server.Site(siteroot)

    # setup application
    application = service.Application("arc-cacheserver")

    cs.setServiceParent(application)

    if use_ssl:
        cf = ssl.ContextFactory()
        internet.SSLServer(port or CACHE_SSL_PORT, site, cf).setServiceParent(application)
    else:
        internet.TCPServer(port or CACHE_TCP_PORT, site).setServiceParent(application)

    return application
