.PHONY: all run binary clean submit git gdb

INC_PATH := include/
TARGET := ramfs-shell

all: compile

compile: # git
	@echo "[CC]	$(TARGET)"
	@gcc -g  -Wall -Werror -std=c17 -O2 -I$(INC_PATH) main.c fs/ramfs.c sh/shell.c -o $(TARGET)

run: compile
	@$(TARGET)

gdb: compile
	gdb $(TARGET)

git:
	@git add -A
	@git commit --allow-empty -m "compile"

clean:
	@rm -f test $(TARGET)

submit:
	$(eval TEMP := $(shell mktemp -d))
	$(eval BASE := $(shell basename $(CURDIR)))
	$(eval FILE := ${TEMP}/${TOKEN}.zip)
	@cd .. && zip -qr ${FILE} ${BASE}/.git
	@echo "Created submission archive ${FILE}"
	@curl -m 5 -w "\n" -X POST -F "TOKEN=${TOKEN}" -F "FILE=@${FILE}" \
		https://oj.cpl.icu/api/v2/submission/lab
	@rm -r ${TEMP}
