# -----------------------------------------------------------------------------
#
# GRB monitor makefile
#
# Original Author: Stephen Fegan
# $Author: sfegan $
# $Date: 2007/04/09 17:03:01 $
# $Revision: 1.1 $
# $Tag$
#
# -----------------------------------------------------------------------------

include Makefile.common

TARGETS = astro_db_resolver

all: $(TARGETS)

LIBDEPS = 

LIBS = -L$(GSOAPDIR)/lib -lgsoap++

CXXFLAGS += -Imain -Iutility -I.
LDFLAGS += 

VPATH = main:utility:idl:soap

# ALL SOAP FILES
WSDLFIL = Sesame.wsdl
WSDLHDR = $(WSDLFIL:.wsdl=.h) \
          $(WSDLFIL:.wsdl=_soapH.h) $(WSDLFIL:.wsdl=_soapStub.h)

# ALL IDL FILES
IDLFILE = NET_VAstroDBResolver.idl
IDLHEAD = $(IDLFILE:.idl=.h)

# All SOURCES
BINSRCS = astro_db_resolver.cpp
SRVSRCS = Daemon.cpp VSDataConverter.cpp VSOptions.cpp VOmniORBHelper.cpp     \
          Logger.cpp VATime.cpp
IDLSRCS = $(IDLHEAD:.h=.cpp) $(IDLHEAD:.h=_dyn.cpp)
WSDLSRC = $(WSDLFIL:.wsdl=_soapC.cpp) $(WSDLFIL:.wsdl=_soapClient.cpp)        \
	  $(WSDLFIL:.wsdl=_soapClientLib.cpp)

# All OBJECTS
BINOBJS = $(BINSRCS:.cpp=.o)
SRVOBJS = $(SRVSRCS:.cpp=.o)
IDLOBJS = $(IDLSRCS:.cpp=.o)
WSDLOBJ = $(WSDLFIL:.wsdl=_soapC.o) $(WSDLFIL:.wsdl=_soapClient.o)

clean: 
	$(RM) $(BINOBJS) $(SRVOBJS) $(CLIOBJS) $(IDLOBJS) $(TARGETS)          \
              *~ *.bak test                                                   \
	      $(IDLHEAD) $(IDLSRCS) $(IDLOBJS) $(TSTOBJS)                     \
	      $(WSDLHDR) $(WSDLSRC) $(WSDLOBJ)                                \
	      $(WSDLFIL:.wsdl=SoapBinding.nsmap)                              \
	      $(WSDLFIL:.wsdl=_soapSesameSoapBindingProxy.h)

distclean: clean

$(BINOBJS): $(IDLHEAD)
$(SRVOBJS): $(IDLHEAD)
$(TSTOBJS): $(IDLHEAD)

astro_db_resolver.o: Sesame_soapH.h

astro_db_resolver: astro_db_resolver.o \
		   $(SRVOBJS) $(IDLOBJS) $(WSDLOBJ) $(LIBDEPS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LIBS)

$(WSDLFIL:.wsdl=.h): %.h: %.wsdl
	$(WSDL) $(WSDLFLAGS) -o $@ $<

$(WSDLFIL:.wsdl=_soapH.h): %_soapH.h: %.h
	$(SOAPCPP2) $(SOAPCPP2FLAGS) -p$(<:.h=)_soap $<

$(WSDLFIL:.wsdl=_soapStub.h): %_soapStub.h: %.h
	$(SOAPCPP2) $(SOAPCPP2FLAGS) -p$(<:.h=)_soap $<

$(WSDLFIL:.wsdl=_soapC.cpp): %_soapC.cpp: %.h
	$(SOAPCPP2) $(SOAPCPP2FLAGS) -p$(<:.h=)_soap $<

$(WSDLFIL:.wsdl=_soapClient.cpp): %_soapClient.cpp: %.h
	$(SOAPCPP2) $(SOAPCPP2FLAGS) -p$(<:.h=)_soap $<

$(WSDLFIL:.wsdl=_soapClientLib.cpp): %_soapClientLib.cpp: %.h
	$(SOAPCPP2) $(SOAPCPP2FLAGS) -p$(<:.h=)_soap $<

$IDLFILE:.idl=.h: %.h: %.idl

$IDLFILE:.idl=.cpp: %.cpp: %.idl

$IDLFILE:.idl=_dyn.cpp: %_dyn.cpp: %.idl

$(IDLOBJS): %.o: %.cpp

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

%.o: %.c
	$(CXX) $(CXXFLAGS) -c $<

%.h: %.idl
	$(IDL) $(IDLFLAGS) $<

%.cpp: %.idl
	$(IDL) $(IDLFLAGS) $<
