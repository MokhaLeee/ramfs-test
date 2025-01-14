.PHONY: all run binary clean submit git gdb

INC_PATH := include/

all: compile

compile: git
	@gcc -g -std=c17 -O2 -I$(INC_PATH) main.c fs/ramfs.c sh/shell.c -o ramfs-shell

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
