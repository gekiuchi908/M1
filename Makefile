#
# Makefile for M1 sound player
#

CC   = gcc
CXX  = g++

# C/C++ 共通のプリプロセッサフラグ
CPPFLAGS += -DSOUND_OUTPUT=1 -DHAS_YM2612=1 -DHAS_YM3438=1 -DHAS_YM2203=1 -DHAS_YM2610=1 -DHAS_YM2610B=1 -DINLINE="static __inline__"
CPPFLAGS += -DHAS_YM3812=1 -DHAS_YM3526=1 -DHAS_M65C02=1 -DLSB_FIRST=1 -DHAS_M6803=1 -DHAS_M6808=1 -DHAS_ADSP2105=1
CPPFLAGS += -DHAS_ES5505=1 -DHAS_ES5506=1 -DHAS_K005289=1 -DHAS_SN76496=1 -DHAS_K007232=1 -DHAS_NAMCO=1
CPPFLAGS += -DHAS_CEM3394=1 -DHAS_YMZ280B=1 -DHAS_AY8910=1 -DHAS_DAC=1 -DHAS_SEGAPCM=1 -DHAS_OKIM6295=1
CPPFLAGS += -DHAS_TMS5220=1 -DHAS_ADPCM=1 -DHAS_K051649=1 -DHAS_YM2151_ALT=1 -DHAS_RF5C68=1
CPPFLAGS += -DHAS_QSOUND=1 -DHAS_K054539=1 -DHAS_UPD7759=1 -DHAS_MULTIPCM=1 -DHAS_YMF278B=1 -DHAS_MSM5232=1
CPPFLAGS += -DHAS_K053260=1 -DHAS_POKEY=1 -DHAS_HC55516=1 -DHAS_IREMGA20=1 -DHAS_MSM5205=1 -DHAS_C140=1
CPPFLAGS += -DHAS_BSMT2000=1 -DHAS_HD63701=1 -DHAS_CUSTOM=1 -DHAS_ADSP2100=1 -DHAS_ADSP2101=1 -DHAS_ADSP2115=1
CPPFLAGS += -DHAS_YMF262=1 -DHAS_YM2413=1 -DHAS_YM2608=1 -DHAS_VLM5030=1 -DHAS_MPEG=1 -DHAS_N7751=1
CPPFLAGS += -DHAS_PIC16C54=1 -DHAS_PIC16C55=1 -DHAS_PIC16C56=1 -DHAS_PIC16C57=1 -DHAS_PIC16C58=1
CPPFLAGS += -DHAS_C352=1 -DHAS_YMF271=1 -DHAS_SCSP=1 -DHAS_Y8950=1 -DHAS_ADSP2104=1 -DPATHSEP="/"
#-DPTR64
# non-"core" defines
CPPFLAGS += -DPS2=0 -DM1=1 -DUNIX=1 -DNDEBUG=1
CPPFLAGS += -I. -I.. -Icpu -I../cpu -Isound -I../sound -Iboards -I../boards -Impeg -I../mpeg -Iexpat -I../expat -Izlib -I../zlib
CPPFLAGS += `pkg-config --cflags libpulse` `sdl-config --cflags`

# C/C++ 共通のコンパイラ警告・最適化フラグ
COMMON_FLAGS := -Wall -c -O3 -g3 -fno-strict-aliasing -fPIC -Wno-unused-but-set-variable

# C専用フラグ（ここに C専用警告を追加）
CFLAGS   += $(COMMON_FLAGS) -Wno-old-style-definition -std=c11

# C++専用フラグ
CXXFLAGS += $(COMMON_FLAGS) -std=c++17

EXE  = m1
LIBS = -lasound `pkg-config --libs libpulse` `sdl-config --libs`                                                                            

# m1 core objects
OBJS = m1snd.o unzip.o timer.o wavelog.o rom.o irem_cpu.o
OBJS += 6821pia.o cpuintrf.o sndintrf.o state.o taitosnd.o kabuki.o memory.o
OBJS += trklist.o m1queue.o m1filter.o xmlout.o 
OBJS += chd.o chdcd.o harddisk.o md5.o sha1.o gamelist.o
OBJS += oss.o

# MPEG decoder objects

OBJS += mpeg/dump.o mpeg/getbits.o mpeg/getdata.o mpeg/huffman.o mpeg/layer2.o 
OBJS += mpeg/layer3.o mpeg/misc2.o mpeg/position.o mpeg/transform.o mpeg/util.o mpeg/audio.o

# Zlib objects (avoids dynamic link, allowing use with .NET / Mono)

OBJS += zlib/adler32.o zlib/deflate.o zlib/gzwrite.o zlib/inflate.o zlib/trees.o zlib/compress.o
OBJS += zlib/gzlib.o zlib/infback.o zlib/inftrees.o zlib/uncompr.o zlib/crc32.o zlib/gzclose.o
OBJS += zlib/gzread.o zlib/inffast.o zlib/zutil.o

# Expat XML parser lib objects

OBJS += expat/xmlparse.o expat/xmlrole.o expat/xmltok.o

CUIOBJS = m1cui.o

# Boards (drivers)
OBJS += boards/brd_raiden2.o boards/brd_segapcm.o boards/brd_taifx1.o boards/brd_multi32.o 
OBJS += boards/brd_sys1832.o boards/brd_hcastle.o boards/brd_segamodel1.o boards/brd_cps1.o boards/brd_gradius3.o 
OBJS += boards/brd_twin16.o boards/brd_qsound.o boards/brd_xexex.o boards/brd_bubblebobble.o boards/brd_parodius.o
OBJS += boards/brd_namsys21.o boards/brd_overdrive.o boards/brd_contra.o boards/brd_gradius.o boards/brd_gx.o
OBJS += boards/brd_gyruss.o boards/brd_btime.o boards/brd_atarisy1.o boards/brd_atarisy2.o boards/brd_itech32.o
OBJS += boards/brd_f3.o boards/brd_gauntlet.o boards/brd_gng.o boards/brd_starwars.o boards/brd_mpatrol.o 
OBJS += boards/brd_macrossplus.o boards/brd_braveblade.o boards/brd_s1945.o
OBJS += boards/brd_dbz2.o boards/brd_null.o boards/brd_sharrier.o boards/brd_endurobl2.o
OBJS += boards/brd_neogeo.o boards/brd_megasys1.o boards/brd_ssio.o
OBJS += boards/brd_1942.o boards/brd_bjack.o boards/brd_88games.o boards/brd_sys16.o
OBJS += boards/brd_m72.o boards/brd_m92.o boards/brd_dcs.o boards/brd_chipsqueakdeluxe.o
OBJS += boards/brd_deco8.o boards/brd_scsp.o boards/brd_wmscvsd.o boards/brd_wmsadpcm.o 
OBJS += boards/brd_btoads.o boards/brd_lemmings.o boards/brd_sidepck.o
OBJS += boards/brd_segasys1.o boards/brd_atarijsa.o boards/brd_cavez80.o boards/brd_sf1.o
OBJS += boards/brd_darius.o boards/brd_namsys1.o boards/brd_ms32.o boards/brd_sun16.o
OBJS += boards/brd_frogger.o boards/brd_blzntrnd.o boards/brd_ddragon.o
OBJS += boards/brd_magiccat.o boards/brd_raizing.o boards/brd_ddragon3.o boards/brd_tatass.o
OBJS += boards/brd_aquarium.o boards/brd_djboy.o boards/brd_deco32.o boards/brd_skns.o
OBJS += boards/brd_fcombat.o boards/brd_legion.o boards/brd_dooyong.o boards/brd_afega.o
OBJS += boards/brd_nmk16.o boards/brd_namsys86.o boards/brd_sshang.o boards/brd_mappy.o
OBJS += boards/brd_galaga.o boards/brd_airbustr.o boards/brd_toaplan1.o boards/brd_segac2.o
OBJS += boards/brd_cischeat.o boards/brd_harddriv.o boards/brd_flower.o boards/brd_oneshot.o
OBJS += boards/brd_rastan.o boards/brd_tecmosys.o boards/brd_ssys22.o boards/brd_tail2nose.o
OBJS += boards/brd_ajax.o boards/brd_nslash.o boards/brd_njgaiden.o boards/brd_jedi.o
OBJS += boards/brd_dsb.o boards/brd_wecleman.o boards/brd_dsbz80.o boards/brd_bottom9.o
OBJS += boards/brd_tnzs.o boards/brd_rushcrash.o boards/brd_tecmo16.o boards/brd_combatsc.o
OBJS += boards/brd_circusc.o boards/brd_bladestl.o boards/brd_renegade.o boards/brd_rygar.o
OBJS += boards/brd_namh8.o  boards/brd_hotrock.o boards/brd_psychic5.o boards/brd_spi.o
OBJS += boards/brd_fuuki32.o boards/brd_slapfight.o boards/brd_douni.o 
OBJS += boards/brd_cage.o boards/brd_airgallet.o boards/brd_gott3.o boards/brd_hatch.o
OBJS += boards/brd_psycho.o boards/brd_mnight.o boards/brd_logicpro.o boards/brd_gladiator.o
OBJS += boards/brd_thunder.o boards/brd_taitosj.o boards/brd_beatmania.o boards/brd_pizza.o
OBJS += boards/brd_rallyx.o boards/brd_yunsun.o boards/brd_bbusters.o boards/brd_snk68k.o
OBJS += boards/brd_buggyboy.o boards/brd_spacegun.o boards/brd_hyperduel.o
OBJS += boards/brd_equites.o boards/brd_taito84.o boards/brd_tatsumi.o boards/brd_namcona.o
OBJS += boards/brd_genesis.o boards/brd_jaleco.o boards/brd_panicr.o boards/brd_mitchell.o
OBJS += boards/brd_arkanoid.o boards/brd_hexion.o

# Sound cores
OBJS += sound/fm.o sound/multipcm.o sound/scsp.o sound/segapcm.o sound/scspdsp.o
OBJS += sound/ym2151.o sound/rf5c68.o sound/ay8910.o sound/ymdeltat.o sound/fmopl.o
OBJS += sound/k054539.o sound/k053260.o sound/ymf278b.o sound/c140.o sound/tms57002.o 
OBJS += sound/upd7759.o sound/samples.o sound/dac.o sound/pokey.o sound/es5506.o
OBJS += sound/adpcm.o sound/k007232.o sound/qsound.o sound/msm5205.o sound/tms5220.o
OBJS += sound/5220intf.o sound/iremga20.o sound/streams.o sound/hc55516.o
OBJS += sound/bsmt2000.o sound/k005289.o sound/sn76496.o sound/namco.o sound/cem3394.o
OBJS += sound/ymz280b.o sound/2203intf.o sound/2610intf.o sound/2612intf.o sound/3812intf.o
OBJS += sound/k051649.o sound/2151intf.o sound/flower.o sound/ym2413.o sound/2413intf.o
OBJS += sound/2608intf.o sound/vlm5030.o sound/262intf.o sound/ymf262.o sound/c352.o sound/ymf271.o
OBJS += sound/dmadac.o sound/rf5c400.o sound/msm5232.o sound/es5510.o

# CPU cores
OBJS += cpu/m68kcpu.o cpu/m68kops.o
OBJS += cpu/m6800.o cpu/m6809.o cpu/m6502.o cpu/h6280.o cpu/i8039.o cpu/nec.o
OBJS += cpu/adsp2100.o cpu/z80.o cpu/m37710.o cpu/m37710o0.o cpu/m37710o1.o
OBJS += cpu/m37710o2.o cpu/m37710o3.o cpu/hd6309.o cpu/tms32010.o cpu/pic16c5x.o
OBJS += cpu/h83002.o cpu/h8periph.o cpu/tms32031.o cpu/2100dasm.o
OBJS += cpu/i8085.o

SRCS = $(foreach obj,$(OBJS) $(CUIOBJS),$(firstword $(wildcard $(obj:.o=.cpp) $(obj:.o=.c))))

%.o: %.c
	@echo Compiling $<
	@$(CC) $(CPPFLAGS) $(CFLAGS) $< -o $@

%.o: %.cpp
	@echo Compiling $<
	@$(CXX) $(CPPFLAGS) $(CXXFLAGS) $< -o $@
	
all: $(EXE) launcher

# make the libraries
lib$(EXE).a: $(OBJS)
	ar r lib$(EXE).a $(OBJS)

lib$(EXE).so: $(OBJS)
	@echo Linking library
	@$(CXX) -shared -o lib$(EXE).so $(OBJS)

# link the commandline exe
$(EXE): $(CUIOBJS) $(OBJS)
	@echo Linking $(EXE)
	@$(CXX) -g -o $(EXE) $(CUIOBJS) $(OBJS) -L. $(LIBS)

# M1 GUI Launcher
LAUNCHER_EXE = m1-launcher
QT_DIR ?= /home/hkiuchi/Qt/6.11.1/gcc_64
QT_MOC = $(QT_DIR)/libexec/moc
QT_PKG_PATH = $(QT_DIR)/lib/pkgconfig
QT_CFLAGS = $(shell PKG_CONFIG_PATH=$(QT_PKG_PATH):$$PKG_CONFIG_PATH pkg-config --cflags Qt6Widgets)
QT_LIBS   = $(shell PKG_CONFIG_PATH=$(QT_PKG_PATH):$$PKG_CONFIG_PATH pkg-config --libs Qt6Widgets)

LAUNCHER_SRCS = launcher/main.cpp launcher/xml_parser.cpp launcher/game_table_model.cpp \
                launcher/rom_verifier.cpp launcher/m1_process.cpp launcher/settings_dialog.cpp \
                launcher/game_cache.cpp launcher/track_list.cpp launcher/rom_browser_dialog.cpp \
                launcher/mainwindow.cpp launcher/i18n.cpp
LAUNCHER_MOC_HEADERS = launcher/game_table_model.h launcher/rom_verifier.h launcher/m1_process.h \
                       launcher/settings_dialog.h launcher/rom_browser_dialog.h launcher/mainwindow.h
LAUNCHER_MOC_SRCS = $(LAUNCHER_MOC_HEADERS:launcher/%.h=launcher/moc_%.cpp)
LAUNCHER_OBJS = $(LAUNCHER_SRCS:.cpp=.o) $(LAUNCHER_MOC_SRCS:.cpp=.o) \
                unzip.o $(filter zlib/%,$(OBJS))

launcher/moc_%.cpp: launcher/%.h
	@echo Running MOC for $<
	@$(QT_MOC) $< -o $@

launcher/%.o: launcher/%.cpp
	@echo Compiling Launcher $<
	@$(CXX) -std=c++17 -Wall -g -O2 -fPIC -MMD -MP -I. -Izlib $(QT_CFLAGS) -c $< -o $@

-include launcher/*.d

launcher: $(LAUNCHER_EXE)

$(LAUNCHER_EXE): $(LAUNCHER_OBJS)
	@echo Linking $(LAUNCHER_EXE)
	@$(CXX) -g -o $(LAUNCHER_EXE) $(LAUNCHER_OBJS) $(QT_LIBS)

depend:
	makedepend -- $(CFLAGS) -- $(SRCS)

clean:
	rm -f lib$(EXE).a $(OBJS) $(CUIOBJS) $(EXE) *.bak cpu/*.bak boards/*.bak sound/*.bak \
	      $(LAUNCHER_EXE) launcher/*.o launcher/*.d launcher/moc_*.cpp

# DO NOT DELETE

check:
	@mkdir -p check-results
	@for src in $(SRCS); do \
		filename=$$(basename "$$src"); \
		echo "--- Checking $$filename" > check-results/"$$filename.txt"; \
		clang-tidy "$$src" >> check-results/"$$filename.txt"; \
	done

