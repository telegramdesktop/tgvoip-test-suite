#  Daniil Gentili's submission to the VoIP contest.
#  Copyright (C) 2019 Daniil Gentili <daniil@daniil.it>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <https://www.gnu.org/licenses/>.
#

include $(CLEAR_VARS)

LIBS += -lstdc++fs

CXXFLAGS		=	-O3 -std=c++17 -Wall -c -fpic -g
CFLAGS			=	-O3 -fexceptions -fpic -g

CPPFLAGS_CALL	=	$(shell pkg-config --cflags opus opusfile)
LFLAGS_CALL		=	-lopus -lopusfile -lopusenc -pthread -ltgvoip $(LIBS)

CPPFLAGS_RATE	=	$(shell pkg-config --cflags pocketsphinx sphinxbase opus opusfile)
LFLAGS_RATE		=	$(shell pkg-config --libs pocketsphinx sphinxbase opus opusfile) $(LIBS)

CPPFLAGS_RATE	+=	-DRANDOM_PREFIX=tgvoiprate -DOUTSIDE_SPEEX -DRESAMPLE_FULL_SINC_TABLE

#
#	Compiler
#
#	By default, the GNU C++ compiler is used. If you want to use a different
#	compiler, you can change that here. You can change this for both the 
#	compiler (the program that turns the c++ files into object files) and for
#	the linker (the program that links all object files into the single .so
#	library file. By default, g++ (the GNU C++ compiler) is used for both.
#

# For debian systems, actually build libopusenc library, too
LIBOPUSENC			=	/usr/include/opus/opusenc.h /usr/lib/libopusenc.so

CMUTGZ				=	cmuclmtk-0.7.tar.gz
CMUCLMTK			=	/usr/bin/wfreq2vocab
MODEL				=	assets/model/list.lm
TGVOIP				=	/usr/lib/libtgvoip.so
TGVOIPCALL			= 	../tgvoipcall
TGVOIPRATE			= 	../tgvoiprate
FIXTRANSCRIPT		=	assets/fixTranscripts

OBJECTS_RATE		=	rater.o tgvoiprate.o resampler/resample.o
OBJECTS_CALL		=	wrapper.o tgvoipcall.o
OBJECT_TRANSCRIPT	=	fixTranscripts.o

#
#	From here the build instructions start
#

all:					${CMUCLMTK} ${LIBOPUSENC} ${TGVOIPCALL} ${TGVOIPRATE} ${FIXTRANSCRIPT} ${MODEL}

${CMUTGZ}:
					wget http://downloads.sourceforge.net/cmusphinx/${CMUTGZ}

${CMUCLMTK}:				| ${CMUTGZ}
					tar -xzf cmuclmtk-0.7.tar.gz && cd cmuclmtk-0.7 && autoreconf -vfi && mkdir -p build && cd build && ../configure --prefix=/usr && make -j$(nproc) && sudo make install && cd ../..

${LIBOPUSENC}:
					cd libopusenc && autoreconf -vfi && mkdir -p build && cd build && ../configure --prefix=/usr && make -j$(nproc) && sudo make install && cd ../..

${TGVOIP}:
					cd libtgvoip && autoreconf -vfi && mkdir -p build && cd build && ../configure --enable-production-logging --enable-audio-callback --prefix=/usr && make -j$(shell nproc) && sudo make install && cd ../..

${TGVOIPCALL}:				${TGVOIP} ${OBJECTS_CALL}
					${CXX} -o $@ ${OBJECTS_CALL} ${LFLAGS_CALL}

${TGVOIPRATE}:				${OBJECTS_RATE}
					${CXX} -std=c++17 -o $@ ${OBJECTS_RATE} ${LFLAGS_RATE}

${FIXTRANSCRIPT}:				${OBJECT_TRANSCRIPT}
					${CXX} -o $@ ${OBJECT_TRANSCRIPT} $(LIBS)

${OBJECTS_RATE}:		CPPFLAGS += ${CPPFLAGS_RATE}
${OBJECTS_CALL}:		CPPFLAGS += ${CPPFLAGS_CALL}
${OBJECT_TRANSCRIPT}:	

${MODEL}:
					assets/genModel.sh

testclean:
					rm -rf tests-output/*

modelclean:
					rm -rf assets/model/*

simpleclean:
					rm -f ${TGVOIPCALL} ${TGVOIPRATE} ${FIXTRANSCRIPT} ${OBJECTS_RATE} ${OBJECTS_CALL} ${OBJECT_TRANSCRIPT}

clean:					simpleclean modelclean testclean
					cd libtgvoip/build && make clean && cd ../..
					cd libopusenc/build && make clean

test:
					php tests/test.php

rate:
					tests/rate-test.sh
