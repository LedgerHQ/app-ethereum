MAKEFLAGS	+=	--no-print-directory

RM			?=	rm -f
ECHO		=	`which echo`

ifneq (,$(findstring xterm,${TERM}))
GREEN       := $(shell tput -Txterm setaf 2)
RED         := $(shell tput -Txterm setaf 1)
CYAN         := $(shell tput -Txterm setaf 6)
RESET 		:= $(shell tput -Txterm sgr0)
else
GREEN       := ""
RED         := ""
RESET       := ""
endif

BUILD_DIRECTORY	=	$(realpath build/)

DIRECTORY_BUILD = build

all:
	@cmake -B ${DIRECTORY_BUILD} -H.
	@make -C ${DIRECTORY_BUILD}
	@CTEST_OUTPUT_ON_FAILURE=1 make -C ${DIRECTORY_BUILD} test

coverage: all
	@lcov --directory . -b "${BUILD_DIRECTORY}" --capture --initial -o coverage.base
	@lcov --rc lcov_branch_coverage=1 --directory . -b "${BUILD_DIRECTORY}" --capture -o coverage.capture
	@lcov --directory . -b "${BUILD_DIRECTORY}" --add-tracefile coverage.base --add-tracefile coverage.capture -o coverage.info
	@lcov --directory . -b "${BUILD_DIRECTORY}" --remove coverage.info '*/tests/unit/*' -o coverage.info --remove coverage.info '*/build/_deps/cmocka-src/src/*'
	@$(ECHO) -e "${GREEN}[ OK ]${RESET} Generated 'coverage.info'."
	@genhtml coverage.info -o coverage
	@if [ -f coverage.base ]; then $(ECHO) -e "${RED}[ RM ]${RESET}" coverage.base && $(RM) -r coverage.base ; fi;
	@if [ -f coverage.capture ]; then $(ECHO) -e "${RED}[ RM ]${RESET}" coverage.capture && $(RM) -r coverage.capture ; fi;
	@$(ECHO) -e "${CYAN}[ REDIRECT ]${RESET}" `realpath coverage/index.html` && xdg-open `realpath coverage/index.html`

clean:
	@if [ -d ${DIRECTORY_BUILD} ]; then $(ECHO) -e "${RED}[ RM ]${RESET}" ${DIRECTORY_BUILD} && $(RM) -r ${DIRECTORY_BUILD} ; fi;
	@if [ -d coverage ]; then $(ECHO) -e "${RED}[ RM ]${RESET}" coverage && $(RM) -r coverage ; fi;
	@if [ -f coverage.info ]; then $(ECHO) -e "${RED}[ RM ]${RESET}" coverage.info && $(RM) -r coverage.info ; fi;

.PHONY: all coverage clean
