//-*-mode:idl; mode:font-lock;-*-

/**
 * \file astro_db_resolver.cpp
 * \brief CORBA gateway to web-based astronomy databases (SIMBAD)
 *
 * Original Author: Stephen Fegan
 * $Author: sfegan $
 * $Date: 2007/08/16 16:46:21 $
 * $Revision: 1.3 $
 * $Tag$
 *
 **/

#include<iostream>
#include<sstream>

#include<zthread/Thread.h>

#include<VSOptions.hpp>
#include<VOmniORBHelper.h>
#include<Logger.hpp>
#include<Daemon.h>

#include<NET_VAstroDBResolver.h>

#include"Sesame_soapH.h" // get proxy
#include"SesameSoapBinding.nsmap" // get namespace bindings

static const char*const VERSION =
  "$Id: astro_db_resolver.cpp,v 1.3 2007/08/16 16:46:21 sfegan Exp $";

using namespace VCorba;
using namespace VERITAS;

// ============================================================================
//
// AstroDBResolverServant
//
// ============================================================================

class AstroDBResolverServant:
  public POA_VAstroDBResolver::Command,
  public PortableServer::RefCountServantBase
{
public:
  AstroDBResolverServant(const std::vector<std::string>& sesame_endpoints,
			 Logger* logger);
  ~AstroDBResolverServant();

  void nAlive();
  VAstroDBResolver::ObjectInfoSeq* resolve(const char* object_name);

private:
  std::vector<std::string> m_sesame_endpoints;
  Logger*                  m_logger;
};

AstroDBResolverServant::
AstroDBResolverServant(const std::vector<std::string>& sesame_endpoints,
		       Logger* logger):
  POA_VAstroDBResolver::Command(), 
  PortableServer::RefCountServantBase(),
  m_sesame_endpoints(sesame_endpoints), m_logger(logger)
{ 
  if(m_sesame_endpoints.empty())m_sesame_endpoints.push_back("");
}

AstroDBResolverServant::~AstroDBResolverServant()
{
  // nothing to see here
}

void AstroDBResolverServant::nAlive()
{
  // nothing to see here
}

VAstroDBResolver::ObjectInfoSeq*
AstroDBResolverServant::resolve(const char* object_name)
{
  m_logger->logMessage(5,std::string("Query: ")+object_name);

  struct soap *soap = soap_new();
  for(std::vector<std::string>::const_iterator iendpoint = 
	m_sesame_endpoints.begin(); iendpoint != m_sesame_endpoints.end();
      iendpoint++)
    {
      const char* endpoint = iendpoint->empty()?NULL:iendpoint->c_str();
      std::string result;
      if(soap_call_ns1__sesame(soap, endpoint, NULL,
			       object_name, "ui", result) != SOAP_OK)
	{
	  std::ostringstream errstream;
	  errstream << "Endpoint failed: " << *iendpoint << ": ";
	  if(soap_check_state(soap))
	    errstream << "soap struct not initialized";
	  else if (soap->error)
	    { 
	      const char *c;
	      const char *v = NULL;
	      const char *s;
	      const char **d = soap_faultcode(soap);
	      if(!*d)soap_set_fault(soap);
	      c = *d;
	      if (soap->version == 2)
		v = *soap_faultsubcode(soap);
	      s = *soap_faultstring(soap);
	      d = soap_faultdetail(soap);
	      if(soap->version)errstream << "SOAP 1." << soap->version;
	      else errstream << "Error " << soap->error;
	      errstream << " fault: " << c;
	      if(v)errstream << " [" << v << ']';
	      if(s)errstream << " reason: " << s;
	      if(d)errstream << " detail: " << *d;
	    }

	  m_logger->logMessage(2,errstream.str());
	  continue;
	}

      std::istringstream rstream(result);
      
      std::string supplier;
      bool found_one    = false;
      double ra         = 0;
      double dec        = 0;
      std::string source_class;
      std::string source_mtype;
      std::vector<std::string> alias;

      unsigned nwritten = 0;

      std::string line;

      VAstroDBResolver::ObjectInfoSeq* results = 
	new VAstroDBResolver::ObjectInfoSeq;

      while(std::getline(rstream,line))
	{
	  m_logger->logMessage(10,std::string("Line: ")+line);

	  if(line.size() < 2)continue;

	  std::string line_copy = line;
	  std::istringstream lstream(line_copy);
	  //std::cerr << line << '\n';
	  if(line[0] == '#')
	    {
	      if(line[1] == '=')
		{
		  assert(!found_one);
		  std::string word;
		  lstream >> word;
		  supplier = word.substr(2);
		}
	      else if(line[1] == '!')
		{
		  std::ostringstream errstream;
		  errstream << "Query: " << object_name 
			    << " ERROR: " << line;
		  m_logger->logMessage(3,errstream.str());
		}
	      else if(line[1] == 'J')
		{
		  if(line.substr(3)=="(No coordinates!)")
		    found_one=false;
		}
	      else if(line[1] == 'B')
		{
		  if(found_one)
		    {
		      found_one=false;
		      nwritten++;
		      
		      results->length(nwritten);

		      (*results)[nwritten-1].resolver = supplier.c_str();
		      (*results)[nwritten-1].name     = object_name;
		      (*results)[nwritten-1].aliases.length(alias.size());
		      for(unsigned ialias=0;ialias<alias.size();ialias++)
			(*results)[nwritten-1].aliases[ialias] = 
			  alias[ialias].c_str();
		      std::ostringstream typestream;
		      typestream << source_class;
		      if(!source_mtype.empty())
			typestream << ' ' << source_mtype;
		      (*results)[nwritten-1].type     = 
			typestream.str().c_str();
		      (*results)[nwritten-1].ra_rad   = ra;
		      (*results)[nwritten-1].dec_rad  = dec;
		      (*results)[nwritten-1].epoch_J  = 2000.0;

		      std::ostringstream stream;
		      stream << "Result: " << object_name << ' '
			     << ra << ' ' << dec << ' ' << 2000 << ' '
			     << source_class << ' ' << source_mtype << ' '
			     << alias[0];
		      m_logger->logMessage(5,stream.str());
		    }

		  supplier     = "";
		  ra           = 0;
		  dec          = 0;
		  source_class = "";
		  source_mtype = "";
		  alias.clear();
		}
	    }
	  else if(line[0] == '%')
	    {
	      //std::cout << found_one << ' ' << line << std::endl;
	      std::string word;
	      lstream >> word;
	      if(word == "%C")lstream >> source_class;
	      if(word == "%T")lstream >> source_mtype;
	      else if(word == "%J")
		{
		  found_one = true;
		  double ra_deg;
		  double dec_deg;
		  lstream >> ra_deg >> dec_deg;
		  ra = ra_deg/180.0*M_PI;
		  dec = dec_deg/180.0*M_PI;
		}
	      else if(word == "%I")alias.push_back(line.substr(3));
	    }
	}

      if(found_one)
	{
	  found_one=false;
	  nwritten++;
		  
	  results->length(nwritten);

	  (*results)[nwritten-1].resolver = supplier.c_str();
	  (*results)[nwritten-1].name     = object_name;
	  (*results)[nwritten-1].aliases.length(alias.size());
	  for(unsigned ialias=0;ialias<alias.size();ialias++)
	    (*results)[nwritten-1].aliases[ialias] = 
	    alias[ialias].c_str();
	  std::ostringstream typestream;
	  typestream << source_class;
	  if(!source_mtype.empty())typestream << ' ' << source_mtype;
	  (*results)[nwritten-1].type     = typestream.str().c_str();
	  (*results)[nwritten-1].ra_rad   = ra;
	  (*results)[nwritten-1].dec_rad  = dec;
	  (*results)[nwritten-1].epoch_J  = 2000.0;

	  std::ostringstream stream;
	  stream << "Result: " << object_name << ' '
		 << ra << ' ' << dec << ' ' << 2000 << ' '
		 << source_class << ' ' << source_mtype;
	  if(!alias.empty()) stream << ' ' << alias[0];
	  m_logger->logMessage(5,stream.str());
	}

      return results;
    }

  throw VAstroDBResolver::ResolveFailed();
}

// ============================================================================
//
// Main
//
// ============================================================================

void usage(const std::string& progname, const VSOptions& options,
           std::ostream& stream)
{
  stream << "Usage: " << progname
         << " [options]"
         << std::endl
         << std::endl;
  stream << "Options:" << std::endl;
  options.printUsage(stream);
}

int main(int argc, char** argv)
{
  std::string program(*argv);

  // --------------------------------------------------------------------------
  // Command line options
  // --------------------------------------------------------------------------

  VSOptions options(argc,argv,true);

  // Set up variables ---------------------------------------------------------

  bool                      print_usage          = false;

  bool                      daemon               = true;
  std::string               daemon_lockfile      = "lock.dat";
  unsigned                  logger_level         = 5;
  std::string               logger_filename      = "log.dat";

  std::string               corba_nameserver     = "corbaname::db.vts";
  std::vector<std::string>  corba_args;
  int                       corba_port           = -1;

  std::vector<std::string>  sesame_endpoint;


  sesame_endpoint.push_back("http://cdsws.u-strasbg.fr/axis/services/Sesame");
  //DOA - SJF 2007-04-06 - sesame_endpoint.push_back("http://vizier.cfa.harvard.edu:8080/axis/servicesa/Sesame");
  sesame_endpoint.push_back("http://vizier.nao.ac.jp:8080/axis/services/Sesame");
  //DOA - SJF 2007-04-06 - sesame_endpoint.push_back("http://vizier.hia.nrc.ca:8080/axis/services/Sesame");
  
  // Fill variables from command line parameters ------------------------------

  if(options.find("h","Print this message.")!=VSOptions::FS_NOT_FOUND)
    print_usage=true;
  if(options.find("help","Print this message.")!=VSOptions::FS_NOT_FOUND)
    print_usage=true;

  if(options.find("no_daemon","Do not go into daemon mode.")
     !=VSOptions::FS_NOT_FOUND)daemon=false;
  options.findWithValue("daemon_lockfile",daemon_lockfile,
                        "Set the name of the lock file, if in daemon mode");
  options.findWithValue("logger_level",logger_level,
                        "Set the maximum message level to log. Higher values "
                        "produce more verbose logs.");
  options.findWithValue("logger_filename",logger_filename,
                        "Set the name of the log file, if in daemon mode");

  options.findWithValue("corba_nameserver",corba_nameserver,
                        "Set the IOR of the CORBA nameserver. The most "
                        "convenient form to give this in is as a CORBANAME, "
                        "e.g. \"corbaname::db.vts\".");
  options.findWithValue("corba_args",corba_args,
                        "Specifiy a comma seperated list of arguments to "
                        "send to OmniORB. See the OmniORB manual for the "
                        "list of command line arguements it accepts");
  options.findWithValue("corba_port",corba_port,
                        "Try to bind to a specific port for CORBA "
                        "communication.");

  options.findWithValue("sesame_endpoint",sesame_endpoint,
                        "Comma separated list of URLs for SIMBAD sesame "
			"service.");

  corba_args.insert(corba_args.begin(), program);

  // Ensure all options have been handled -------------------------------------

  if(!options.assertNoOptions())
    {
      std::cerr << program << ": unknown options: ";
      for(int i=1;i<argc;i++)
        if(*(argv[i])=='-') std::cerr << ' ' << argv[i];
      std::cerr << std::endl;
      usage(program, options, std::cerr);
      exit(EXIT_FAILURE);
    }

  argv++,argc--;

  // Print usage if requested -------------------------------------------------

  if(print_usage)
    {
      usage(program, options, std::cout);
      exit(EXIT_SUCCESS);
    }

  // --------------------------------------------------------------------------
  // Go daemon
  // --------------------------------------------------------------------------

  if(daemon)
    {
      // Get a temporary lock on the lockfile before daemon_init so we
      // can print out an error - the lock is lost at the fork in
      // daemon_init, so we get it again after the logger is started

      int fd;
      if(!VTaskNotification::Daemon::lock_file(daemon_lockfile,&fd))
        {
          std::cerr << "Could lock: " << daemon_lockfile
                    << ", terminating" << std::endl;
          exit(EXIT_FAILURE);
        }
      close(fd);

      // The daemon_init function will change directory to / so watch
      // out for the file names given on the command line. Put CWD at
      // the beginning if necessary.

      char cwd_buffer[1000];
      assert(getcwd(cwd_buffer,sizeof(cwd_buffer)) != NULL);
      std::string cwd = std::string(cwd_buffer)+std::string("/");
      if(daemon_lockfile[0]!='/')daemon_lockfile.insert(0,cwd);
      if(logger_filename[0]!='/')logger_filename.insert(0,cwd);

      // Go daemon...

      VTaskNotification::Daemon::daemon_init("/",true);
    }

  // --------------------------------------------------------------------------
  // Set up the debug logger
  // --------------------------------------------------------------------------

  SystemLogger* logger;
  if(daemon)logger = new SystemLogger(logger_filename,logger_level);
  else logger = new SystemLogger(std::cout,logger_level);

  logger->logSystemMessage(0,"Astro DB resolver starting");
  logger->logSystemMessage(0,VERSION);

  // --------------------------------------------------------------------------
  // Try to acquire lock on daemon_lockfile
  // --------------------------------------------------------------------------

  if(daemon)
    {
      if(VTaskNotification::Daemon::lock_file(daemon_lockfile))
        {
          logger->logSystemMessage(0,std::string("Locked: ")+daemon_lockfile);
        }
      else
        {
          logger->
            logSystemMessage(0,std::string("Could not lock: ")+
                             daemon_lockfile+std::string(", terminating"));
          exit(EXIT_FAILURE);
        }
    }

  // --------------------------------------------------------------------------
  // Fire up CORBA
  // --------------------------------------------------------------------------

  int omni_argc = corba_args.size();
  char** omni_argv = new char*[omni_argc+1];
  std::vector<char*> omni_args(omni_argc+1);

  for(int iarg=0;iarg<omni_argc;iarg++)
    {
      omni_args[iarg] = omni_argv[iarg] =
        new char[corba_args[iarg].length()+1];
      strcpy(omni_argv[iarg], corba_args[iarg].c_str());
    }

  omni_args[omni_argc] = omni_argv[omni_argc] = 0;

  VOmniORBHelper* orb = new VOmniORBHelper;
  orb->orbInit(omni_argc, omni_argv, corba_nameserver.c_str(), corba_port);

  // --------------------------------------------------------------------------
  // Create the CORBA servant, activate it and register it with the NS
  // --------------------------------------------------------------------------

  AstroDBResolverServant* resolver_servant = 
    new AstroDBResolverServant(sesame_endpoint, logger);
  CORBA::Object_var net_resolver;

  try
    {
      net_resolver =
        orb->poaActivateObject(resolver_servant,
                               VAstroDBResolver::progName,
                               VAstroDBResolver::Command::objName);
    }
  catch(const CORBA::Exception& x)
    {
      std::ostringstream stream;
      stream << "CORBA exception while trying to activate the server: "
             << x._name();
      logger->logMessage(1,stream.str());
      exit(EXIT_FAILURE);
    }

  try
    {
      orb->nsRegisterObject(net_resolver,
                            VAstroDBResolver::progName,
                            VAstroDBResolver::Command::objName);
    }
  catch(const CORBA::Exception& x)
    {
      std::ostringstream stream;
      stream << "CORBA exception while trying to register the nameserver: "
             << x._name();
      logger->logMessage(1,stream.str());
      exit(EXIT_FAILURE);
    }

  // --------------------------------------------------------------------------
  // Run the ORB - this call never ends
  // --------------------------------------------------------------------------

  orb->orbRun();

  // --------------------------------------------------------------------------
  // Clean everything up (this never happens)
  // --------------------------------------------------------------------------

  delete resolver_servant;
  delete orb;

  for(unsigned iarg=0;iarg<omni_args.size();iarg++)delete[] omni_args[iarg];
  delete[] omni_argv;

  delete logger;
}
