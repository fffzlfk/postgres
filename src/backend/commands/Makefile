#-------------------------------------------------------------------------
#
# Makefile--
#    Makefile for backend/commands
#
# IDENTIFICATION
#    src/backend/commands/Makefile
#
#-------------------------------------------------------------------------

subdir = src/backend/commands
top_builddir = ../../..
include $(top_builddir)/src/Makefile.global

OBJS = \
	aggregatecmds.o \
	alter.o \
	amcmds.o \
	analyze.o \
	async.o \
	cluster.o \
	collationcmds.o \
	comment.o \
	constraint.o \
	conversioncmds.o \
	copy.o \
	copyfrom.o \
	copyfromparse.o \
	copyto.o \
	createas.o \
	createmodel.o \
	dbcommands.o \
	define.o \
	discard.o \
	dropcmds.o \
	event_trigger.o \
	explain.o \
	extension.o \
	foreigncmds.o \
	functioncmds.o \
	hashmap.o \
	indexcmds.o \
	inferredby.o \
	lockcmds.o \
	matview.o \
	modelutils.o \
	opclasscmds.o \
	operatorcmds.o \
	policy.o \
	portalcmds.o \
	prepare.o \
	proclang.o \
	publicationcmds.o \
	ptensor.o \
	schemacmds.o \
	seclabel.o \
	sequence.o \
	statscmds.o \
	subscriptioncmds.o \
	tablecmds.o \
	tablespace.o \
	trigger.o \
	train.o \
	tsearchcmds.o \
	typecmds.o \
	user.o \
	vacuum.o \
	vacuumparallel.o \
	variable.o \
	view.o

CXX = g++
LIBTORCH_PATH = /home/fffzlfk/dev/train/libtorch
INCLUDE_DIRS = -I$(top_srcdir)/src/include -I$(LIBTORCH_PATH)/include -I$(LIBTORCH_PATH)/include/torch/csrc/api/include
PG_CXXFLAGS += -std=c++17 -g -O0 -fPIC -D_GLIBCXX_USE_CXX11_ABI=0

train.o: train.cpp
	$(CXX) $(INCLUDE_DIRS) $(PG_CXXFLAGS)  -c -o $@ $<

LIBS += PG_LDFLAGS

include $(top_srcdir)/src/backend/common.mk
