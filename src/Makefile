 #
 #   Makefile
 #
 #   Copyright (C) 2014-2020 Programmist11180 <programmer11180@programist.ru>
 #
 #   This file is part of PBXMMS2client.
 #
 #   PBXMMS2client is free software: you can redistribute it and/or modify
 #   it under the terms of the GNU General Public License as published by
 #   the Free Software Foundation, either version 3 of the License, or
 #   (at your option) any later version.
 #
 #   PBXMMS2client is distributed in the hope that it will be useful,
 #   but WITHOUT ANY WARRANTY; without even the implied warranty of
 #   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 #   GNU General Public License for more details.
 #
 #   You should have received a copy of the GNU General Public License
 #   along with PBXMMS2client.  If not, see <http://www.gnu.org/licenses/>.
 #

OUT = pbxmms2client
include /usr/local/pocketbook/common.mk

SOURCES += fork.cpp main.cpp client_screen.cpp settings.cpp log.cpp xmms.cpp

CXXFLAGS+= -Wall -Wextra -I$(HOME)/pb_programming/pbtk-0.2.1 \
`pkg-config --cflags xmms2-client freetype2 glib-2.0`

ifeq (${BUILD},emu)
CXXFLAGS+= -std=c++11 -Wno-write-strings `pkg-config --cflags sigc++-2.0`
LDFLAGS+= -L$(HOME)/pb_programming/pblibs_emulator
endif

ifeq (${BUILD},arm_gnueabi)
CXXFLAGS+= -std=c++98 -I$(HOME)/pb_programming/sigc++-arm-bin/include/sigc++-2.0
LDFLAGS+= -L$(HOME)/pb_programming/pblibs_arm_gnueabi
endif

LIBS+= -linkview -lpbtk -lsigc-2.0 -lxmmsclient -lglib-2.0 -lxmmsclient-glib -lpthread -lpng12


PIXMAPS=

PIXMAPS_C=$(PIXMAPS:.xpm=.c)
PIXMAPS_OBJS=$(addprefix $(OBJDIR)/,$(PIXMAPS_C:.c=.o))

all: $(PROJECT)

$(PROJECT): $(PIXMAPS_C) $(OBJDIR) $(OBJS) $(PIXMAPS_OBJS)
	$(CXX) -o $@ $(OBJS) $(PIXMAPS_OBJS) $(LDFLAGS) $(LIBS)

# $(PROJECT) : $(OBJDIR) $(SYSTEM_LINK) $(OBJS) $(BITMAP_OBJS)
# $(LD) -o $@ $(OBJS) $(BITMAP_OBJS) $(LDFLAGS) $(LIBS)

#$(OBJDIR):
#	mkdir -p $(OBJDIR)
#	mkdir -p $(OBJDIR)

$(OBJDIR)/%.cxx.o: %.cxx
	$(CXX) -c -o $@ $(CXXFLAGS) $(INCLUDES) $(CDEPS) $<
$(OBJDIR)/%.cpp.o: %.cpp
	$(CXX) -c -o $@ $(CXXFLAGS) $(INCLUDES) $(CDEPS) $<

$(OBJDIR)/images/%.o: images/%.c
	$(CC) -c -o $@ $(CFLAGS) $(INCLUDES) $(CDEPS) $<

$(PIXMAPS_C): $(PIXMAPS)

images/%.c: images/%.xpm
	./xpbres -c $@ $<

-include $(OBJDIR)/*.d
