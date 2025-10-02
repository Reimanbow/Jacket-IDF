.PHONY: all build flash monitor run menuconfig clean erase fullclean \
        add-dep rm-dep update-dep create-comp reconf

PROJECT_NAME := Jacket-IDF
PORT ?= /dev/ttyACM0
BAUD ?= 115200

all: build

build:
	idf.py build

flash:
	idf.py -p $(PORT) -b $(BAUD) flash

monitor:
	idf.py -p $(PORT) -b $(BAUD) monitor

run: build flash monitor

menuconfig:
	idf.py menuconfig

clean:
	idf.py clean

erase:
	idf.py -p $(PORT) erase-flash

fullclean:
	idf.py fullclean