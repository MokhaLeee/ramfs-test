.PHONY: all run binary clean submit git gdb

INC_PATH := include/

all: compile

flags :=
# flags += -DTRACE_EN
flags += -DINFO_EN
flags += -DERROR_EN

dflags := $(flags)
dext_src := test1.c test2.c test3.c test4.c test5.c

compile: # git
	@gcc -g -std=c17 -O2 -I$(INC_PATH) $(dflags) $(dext_src) main.c fs/ramfs.c sh/shell.c -o ramfs-shell

run: compile
	@./ramfs-shell

gdb: compile
	gdb ramfs-shell

git:
	@git add -A
	@git commit --allow-empty -m "compile"

clean:
	@rm test

submit:
	$(eval TEMP := $(shell mktemp -d))
	$(eval BASE := $(shell basename $(CURDIR)))
	$(eval FILE := ${TEMP}/${TOKEN}.zip)
	@cd .. && zip -qr ${FILE} ${BASE}/.git
	@echo "Created submission archive ${FILE}"
	@curl -m 5 -w "\n" -X POST -F "TOKEN=${TOKEN}" -F "FILE=@${FILE}" \
		https://oj.cpl.icu/api/v2/submission/lab
	@rm -r ${TEMP}
