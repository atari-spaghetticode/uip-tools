# Copyright (c) 2001, Adam Dunkels.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. The name of the author may not be used to endorse or promote
#    products derived from this software without specific prior
#    written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
# OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
# NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# This file is part of the uIP TCP/IP stack.
#
# $Id: Makefile,v 1.13 2006/06/11 21:55:03 adam Exp $
#

all: uip

CC     = /opt/cross-mint/bin/m68k-atari-mint-gcc
LD     = /opt/cross-mint/bin/m68k-atari-mint-gcc
AR     = /opt/cross-mint/bin/m68k-atari-mint-ar
APPS   = atari dhcpc
CFLAGS = -std=gnu99 -Wall -Os -fomit-frame-pointer -m68000 \
		-I$(abspath ../uip) \
		-I$(abspath .) \
		-I$(abspath ../apps/atari) \
		-I$(abspath ../apps/dhcpc) \
		-I$(abspath ../dev) \
		-DUIP_CONF_BYTE_ORDER=BIG_ENDIAN 

#		\
#		-DUIP_ARCH_IPCHKSUM \
#		-DUIP_ARCH_ADD32

LD_FLAGS = -s -Wl,--traditional-format

-include ../uip/Makefile.include

uip: $(addprefix $(OBJECTDIR)/, main.o tapdev.o uip_arch.o clock-arch.o) apps.a uip.a
	$(LD) $(LD_FLAGS) $(addprefix $(OBJECTDIR)/, main.o tapdev.o uip_arch.o clock-arch.o) apps.a uip.a -o $@.tos

clean:
	rm -fr *.o *~ *core uip $(OBJECTDIR) *.a
