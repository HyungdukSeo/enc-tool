# target & paths
OUTPUT_FILE	= DfmChanger.exe
# source
SRCS 		= 	sample.c
HDRS 		= 	$(SRCS:.c=.h)

# IPG libraries
G_LIBS		=	-lgobject-2.0 -lglib-2.0 -lgio-2.0 -lgobject-2.0 -lglib-2.0
STK_LIBS 	= 	-lPs -lEmime -lGsb -lGs
BASE_LIBS	= 	-lSec
#AS_LIBS 	= 	-lAsBcc -lASsm -lASync -lAsU -lAsI -lAsM2Sdp -lAXSrtp -lSec
MPP_LIBS	= 	-lMfunc -lMpObj -lMPkt -lMpSkUdpRsc -lMpSk -lMpUtil -lPA
CORE_LIBS 	= 	-lMms -lAp -lHa \
				-lCe$(FAST_OPT) -lIf$(FAST_OPT) -lSk$(FAST_OPT) -lU$(FAST_OPT) -lGen$(FAST_OPT)
OS_LIBS 	= 	$(SYSLIB) 
LIBS 		= 	$(AS_LIBS) $(STK_LIBS) $(MPP_LIBS) $(BASE_LIBS) $(CORE_LIBS) $(OS_LIBS) \
				$(G_LIBS)

CC 			= 	$(COMPILER)

DFLAGS 		= 	$(XWARN) -D$(PLATFORM) -DERRDEBUG -D_REENTRANT $(DFLAGS_OPT) \
				-DBLOCK_$(BLKNAME) -DSCU_INC -g

IFLAGS 		= 	-I. \
				-I../include \
				-I$(XN_HOME) \
				-I$(XN_CORE)/include \
				-I$(XN_HOME)/STK/CORE/include \
				-I$(XN_HOME)/STK/ABNF/include \
				-I$(XN_HOME)/STK/SIP/include \
				-I$(XN_HOME)/STK/MGCP/include \
				-I$(XN_HOME)/MPP2.0/include \
				-I$(XN_HOME)/BASE/include/COM/msg\
				-I/usr/include/glib-2.0 \
				-I/usr/lib64/glib-2.0/include \

LDFLAGS 	= 	-L$(XN_CORE)/lib/$(ARCH) \
				-L../lib/$(ARCH) \
				-L$(XN_HOME)/STK/CORE/lib/$(ARCH) \
				-L$(XN_HOME)/STK/SIP/lib/$(ARCH) \
				-L$(XN_HOME)/STK/ABNF/lib/$(ARCH) \
				-L$(XN_HOME)/STK/MGCP/lib/$(ARCH) \
				-L$(XN_HOME)/MPP2.0/lib/$(ARCH) \
				-L$(XN_HOME)/MPP2.0/lib/$(ARCH)/MPbase/ \
				-L$(XN_HOME)/MPP2.0/lib/$(ARCH)/MPfunc/ \
				-L$(XN_HOME)/BASE/lib/$(ARCH) \
                
CFLAGS 		= 	$(DFLAGS) $(IFLAGS)

OBJS 		= 	$(SRCS:%.c=%.o)

CLEAR_TYPES	= o exe

# Default target
.PHONY: all
all: $(OUTPUT_FILE)

.PHONY: clean 
clean:
	rm -rf *.o  *.exe

$(OUTPUT_FILE): $(OBJS)
	g++ -g -o $@ $^ $(LDFLAGS) $(LIBS);

$(ARCH)/%.o	: 	%.c
	g++ $(OUTPUT_OPTION) $(CFLAGS) -c $< -o $@
