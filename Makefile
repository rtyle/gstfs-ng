PRODUCT=gstfs-ng
VERSION=1.0
PACKAGE=$(PRODUCT)-$(VERSION)

INCS=\
	Cwd.h\
	Exception.h\
	FileIndex.h\
	FileReader.h\
	FindFile.h\
	GstFs.h\
	ImageCache.h\
	Image.h\
	ImageReader.h\
	ReaderFactory.h\
	Reader.h\
	Synchronizable.h\
	TranscodeFileReader.h\
	Transcode.h\
	Utility.h\
	readlink.h\

SRCS=\
	Cwd.cpp\
	FileIndex.cpp\
	FileReader.cpp\
	GstFs.cpp\
	ImageCache.cpp\
	ImageReader.cpp\
	main.cpp\
	Reader.cpp\
	ReaderFactory.cpp\
	Transcode.cpp\
	TranscodeFileReader.cpp\
	Utility.cpp\
	readlink.cpp\

OBJS=$(SRCS:.cpp=.o)

FILES=$(INCS) $(SRCS) Makefile COPYING gstfs-ng.8 .project .cproject ChangeLog gstfs-ng.monitor

PKGS=fuse glib-2.0 gstreamer-1.0

LIBS=-lboost_thread -lpthread $$(pkg-config --libs $(PKGS))

CXXFLAGS+=-g -Wall -D_FILE_OFFSET_BITS=64 -DFUSE_USE_VERSION=26 $$(pkg-config --cflags $(PKGS))	--std=c++11 -Wno-deprecated

all: $(PRODUCT)

$(PRODUCT): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LIBS)

clean:
	$(RM) $(PRODUCT) $(OBJS) $(PACKAGE).tgz
	$(RM) -r $(PACKAGE)

$(PACKAGE).tgz: $(FILES)
	mkdir $(PACKAGE)
	cp $(FILES) $(PACKAGE)
	tar czf $(PACKAGE).tgz $(PACKAGE)
	$(RM) -r $(PACKAGE)

build: $(PACKAGE).tgz
	tar xzf $(PACKAGE).tgz
	cd $(PACKAGE); $(MAKE)
