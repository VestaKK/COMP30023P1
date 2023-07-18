CC 		  := gcc
CCFLAGS   := -Wall

BUILD	  := build
TEST_DIR  := tests
SRC_DIR	  := src

R_DIR     := $(BUILD)/release
D_DIR	  := $(BUILD)/debug

INCFLAGS  := -Iinclude
LFLAGS	  := -lm
SRC 	  := $(wildcard $(SRC_DIR)/*.c)
OBJ 	  := $(SRC:$(SRC_DIR)/%.c=%.o)

EXE		  := ./allocate
DEBUG 	  := ./allocate_debug

default: release

all: release debug process

process: process.c
	$(CC) $(CCFLAGS) -o $@ $^

release: dirs $(EXE)

debug: dirs $(DEBUG)

dirs:
	@mkdir -p $(BUILD)
	@mkdir -p $(R_DIR)
	@mkdir -p $(D_DIR)

$(R_DIR)/%.o:$(SRC_DIR)/%.c
	$(CC) $(CCFLAGS) -c $^ -o $@ $(INCFLAGS) $(LFLAGS)

$(EXE): $(OBJ:%=$(R_DIR)/%)
	$(CC) $(CCFLAGS) -o $@ $^ $(INCFLAGS) $(LFLAGS)

$(D_DIR)/%.o:$(SRC_DIR)/%.c
	@mkdir -p $(D_DIR)
	$(CC) $(CCFLAGS) -D DEBUG -c $^ -o $@ $(INCFLAGS) $(LFLAGS)

$(DEBUG): $(OBJ:%=$(D_DIR)/%)
	$(CC) $(CCFLAGS) -D DEBUG -o $@ $^ $(INCFLAGS) $(LFLAGS)

clean:
	@rm -rf $(BUILD)
	@rm -f ./allocate
	@rm -f ./allocate_debug
	@rm -f ./process

test: 
	$(EXE) -f cases/task1/simple.txt -s SJF -m infinite -q 1
	$(EXE) -f cases/task1/more-processes.txt -s SJF -m infinite -q 3

	$(EXE) -f cases/task2/simple.txt -s RR -m infinite -q 3
	$(EXE) -f cases/task2/two-processes.txt -s RR -m infinite -q 1
	$(EXE) -f cases/task2/two-processes.txt -s RR -m infinite -q 3

	$(EXE) -f cases/task3/simple.txt -s SJF -m best-fit -q 3
	$(EXE) -f cases/task3/non-fit.txt -s SJF -m best-fit -q 3
	$(EXE) -f cases/task3/non-fit.txt -s RR -m best-fit -q 3

	$(EXE) -f cases/task4/spec.txt -s SJF -m infinite -q 3
	$(EXE) -f cases/task1/more-processes.txt -s SJF -m infinite -q 3
	$(EXE) -f cases/task2/simple.txt -s RR -m infinite -q 3

	$(EXE) -f cases/task1/simple.txt -s SJF -m infinite -q 1
	$(EXE) -f cases/task2/two-processes.txt -s RR -m infinite -q 3

test_debug: 
	$(DEBUG) -f cases/task1/simple.txt -s SJF -m infinite -q 1
	$(DEBUG) -f cases/task1/more-processes.txt -s SJF -m infinite -q 3

	$(DEBUG) -f cases/task2/simple.txt -s RR -m infinite -q 3
	$(DEBUG) -f cases/task2/two-processes.txt -s RR -m infinite -q 1
	$(DEBUG) -f cases/task2/two-processes.txt -s RR -m infinite -q 3

	$(DEBUG) -f cases/task3/simple.txt -s SJF -m best-fit -q 3
	$(DEBUG) -f cases/task3/non-fit.txt -s SJF -m best-fit -q 3
	$(DEBUG) -f cases/task3/non-fit.txt -s RR -m best-fit -q 3

	$(DEBUG) -f cases/task4/spec.txt -s SJF -m infinite -q 3
	$(DEBUG) -f cases/task1/more-processes.txt -s SJF -m infinite -q 3
	$(DEBUG) -f cases/task2/simple.txt -s RR -m infinite -q 3

	$(DEBUG) -f cases/task1/simple.txt -s SJF -m infinite -q 1
	$(DEBUG) -f cases/task2/two-processes.txt -s RR -m infinite -q 3

test_diff:
	$(EXE) -f cases/task1/simple.txt -s SJF -m infinite -q 1 | diff - cases/task1/simple-sjf.out
	$(EXE) -f cases/task1/more-processes.txt -s SJF -m infinite -q 3 | diff - cases/task1/more-processes.out

	$(EXE) -f cases/task2/simple.txt -s RR -m infinite -q 3 | diff - cases/task2/simple-rr.out
	$(EXE) -f cases/task2/two-processes.txt -s RR -m infinite -q 1 | diff - cases/task2/two-processes-1.out
	$(EXE) -f cases/task2/two-processes.txt -s RR -m infinite -q 3 | diff - cases/task2/two-processes-3.out

	$(EXE) -f cases/task3/simple.txt -s SJF -m best-fit -q 3 | diff - cases/task3/simple-bestfit.out
	$(EXE) -f cases/task3/non-fit.txt -s SJF -m best-fit -q 3 | diff - cases/task3/non-fit-sjf.out
	$(EXE) -f cases/task3/non-fit.txt -s RR -m best-fit -q 3 | diff - cases/task3/non-fit-rr.out

	$(EXE) -f cases/task4/spec.txt -s SJF -m infinite -q 3 | diff - cases/task4/spec.out
	$(EXE) -f cases/task1/more-processes.txt -s SJF -m infinite -q 3 | diff - cases/task1/more-processes.out
	$(EXE) -f cases/task2/simple.txt -s RR -m infinite -q 3 | diff - cases/task2/simple-rr.out

	$(EXE) -f cases/task1/simple.txt -s SJF -m infinite -q 1 | diff - cases/task1/simple-sjf.out
	$(EXE) -f cases/task2/two-processes.txt -s RR -m infinite -q 3 | diff - cases/task2/two-processes-3.out

.PHONY: default all release debug dirs test test_debug test_diff clean