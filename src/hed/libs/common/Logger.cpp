#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// Logger.cpp

#include <sstream>
#include <glib.h>
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif
#include "Logger.h"
#include "DateTime.h"
#ifdef WIN32
#include <process.h>
#endif

#undef rootLogger

namespace Arc {

  std::ostream& operator<<(std::ostream& os, LogLevel level) {
    if(level == VERBOSE)
      os << "VERBOSE";
    else if(level == DEBUG)
      os << "DEBUG";
    else if(level == INFO)
      os << "INFO";
    else if(level == WARNING)
      os << "WARNING";
    else if(level == ERROR)
      os << "ERROR";
    else if(level == FATAL)
      os << "FATAL";
    else  // There should be no more alternative!
      ;
    return os;
  }

  LogLevel string_to_level(const std::string& str) {
    if(str == "VERBOSE")
      return VERBOSE;
    else if(str == "DEBUG")
      return DEBUG;
    else if(str == "INFO")
      return INFO;
    else if(str == "WARNING")
      return WARNING;
    else if(str == "ERROR")
      return ERROR;
    else if(str == "FATAL")
      return FATAL;
    else  // should not happen...
      return FATAL;
  }

  LogMessage::LogMessage(LogLevel level,
                         const std::string& message,
                         va_list *v) :
    time(TimeStamp()),
    level(level),
    domain("---"),
    identifier(getDefaultIdentifier()),
    message(message) {
    char buf[2048];
#ifdef HAVE_LIBINTL_H
    vsnprintf(buf, 2048, dgettext("Arc", LogMessage::message.c_str()), *v);
#else
    vsnprintf(buf, 2048, LogMessage::message.c_str(), *v);
#endif
    LogMessage::message=buf;
  }

  LogMessage::LogMessage(LogLevel level,
                         const std::string& message,
                         const std::string& identifier,
                         va_list *v) :
    time(TimeStamp()),
    level(level),
    domain("---"),
    identifier(identifier),
    message(message) {
    char buf[2048];
#ifdef HAVE_LIBINTL_H
    vsnprintf(buf, 2048, dgettext("Arc", LogMessage::message.c_str()), *v);
#else
    vsnprintf(buf, 2048, LogMessage::message.c_str(), *v);
#endif
    LogMessage::message=buf;
  }

  LogLevel LogMessage::getLevel() const {
    return level;
  }

  void LogMessage::setIdentifier(std::string identifier) {
    this->identifier = identifier;
  }

  std::string LogMessage::getDefaultIdentifier() {
    std::ostringstream sout;
#ifdef HAVE_GETPID
    sout << getpid() << "/"
         << (unsigned long int)(void*)Glib::Thread::self();
#else
    sout << (unsigned long int)(void*)Glib::Thread::self();
#endif
    return sout.str();
  }

  void LogMessage::setDomain(std::string domain) {
    this->domain = domain;
  }

  std::ostream& operator<<(std::ostream& os, const LogMessage& message) {
    os << "[" << message.time << "] "
       << "[" << message.domain << "] "
       << "[" << message.level << "] "
       << "[" << message.identifier << "] "
       << message.message;
    return os;
  }

  LogDestination::LogDestination() {
    // Nothing needs to be done here.
  }

  LogDestination::LogDestination(const LogDestination&) {
    // Executing this code should be impossible!
    exit(EXIT_FAILURE);
  }

  void LogDestination::operator=(const LogDestination&) {
    // Executing this code should be impossible!
    exit(EXIT_FAILURE);
  }

  LogStream::LogStream(std::ostream& destination) : destination(destination) {
    // Nothing else needs to be done.
  }

  void LogStream::log(const LogMessage& message) {
    Glib::Mutex::Lock lock(mutex);
    destination << message << std::endl;
  }

  LogStream::LogStream(const LogStream&) : LogDestination(),
                                           destination(std::cerr) {
    // Executing this code should be impossible!
    exit(EXIT_FAILURE);
  }

  void LogStream::operator=(const LogStream&) {
    // Executing this code should be impossible!
    exit(EXIT_FAILURE);
  }

  Logger* Logger::rootLogger = NULL;
  unsigned int Logger::rootLoggerMark = ~rootLoggerMagic;

  Logger& Logger::getRootLogger(void) {
    if((rootLogger == NULL) || (rootLoggerMark != rootLoggerMagic)) {
      rootLogger = new Logger();
      rootLoggerMark = rootLoggerMagic;
    }
    return *rootLogger;
  }

  // LogStream Logger::cerr(std::cerr);

  Logger::Logger(Logger& parent,
                 const std::string& subdomain) :
    parent(&parent),
    domain(parent.getDomain() + "." + subdomain),
    threshold(parent.getThreshold()) {
    // Nothing else needs to be done.
  }

  Logger::Logger(Logger& parent,
                 const std::string& subdomain,
                 LogLevel threshold) :
    parent(&parent),
    domain(parent.getDomain() + "." + subdomain),
    threshold(threshold) {
    // Nothing else needs to be done.
  }

  void Logger::addDestination(LogDestination& destination) {
    destinations.push_back(&destination);
  }

  void Logger::setThreshold(LogLevel threshold) {
    this->threshold = threshold;
  }

  LogLevel Logger::getThreshold() const {
    return threshold;
  }

  void Logger::msg(LogMessage message) {
    message.setDomain(domain);
    log(message);
  }

  void Logger::msg(LogLevel level, const std::string& str, ...) {
    va_list v;
    va_start(v, str);
    msg(LogMessage(level, str, &v));
    va_end(v);
  }

  Logger::Logger() :
    parent(0),
    domain("Arc"),
    threshold(VERBOSE) {
    // addDestination(cerr);
  }

  Logger::Logger(const Logger&) {
    // Executing this code should be impossible!
    exit(EXIT_FAILURE);
  }

  void Logger::operator=(const Logger&) {
    // Executing this code should be impossible!
    exit(EXIT_FAILURE);
  }

  std::string Logger::getDomain() {
    return domain;
  }

  void Logger::log(const LogMessage& message) {
    if(message.getLevel() >= threshold) {
      std::list<LogDestination*>::iterator dest;
      std::list<LogDestination*>::iterator begin = destinations.begin();
      std::list<LogDestination*>::iterator end = destinations.end();
      for(dest = begin; dest != end; ++dest) {
        (*dest)->log(message);
      }
      if(parent != 0) {
        parent->log(message);
      }
    }
  }

}
