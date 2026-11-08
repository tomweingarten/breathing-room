# Builds breathing-room.nds with BlocksDS (https://blocksds.skylyrac.net).
# With the BlocksDS Docker image:
#   docker run --rm -v "$PWD":/work -w /work/nds skylyrac/blocksds:slim-latest make
override NAME := breathing-room
override GAME_TITLE := Breathing Room
override GAME_SUBTITLE := Slow breathing pacer
override GAME_AUTHOR := Tom Weingarten
override GAME_ICON := icon.bmp

BLOCKSDS ?= /opt/blocksds/core
include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile
