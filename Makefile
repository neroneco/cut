# determine compiler
ifeq ($(CC),clang)
  FLAGS += -Weverything
else ifeq ($(CC),gcc)
  FLAGS += -Wall -Wextra
else
Warning:
	@echo No compiler detected, set CC environment variable
endif

# flag macros
FLAGS += -std=c99 
LDFLAGS := -lpthread

# path macros
BIN_PATH := bin
OBJ_PATH := obj
SRC_PATH := src
DBG_PATH := debug

TARGET_NAME := cut

TARGET := $(BIN_PATH)/$(TARGET_NAME)
TARGET_DEBUG := $(DBG_PATH)/$(TARGET_NAME)

# src files & obj files
SRC := $(wildcard $(SRC_PATH)/*.c)
OBJ := $(addprefix $(OBJ_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))
OBJ_DEBUG := $(addprefix $(DBG_PATH)/, $(addsuffix .o, $(notdir $(basename $(SRC)))))


# default rule
default: dir all

# non-phony targets
$(TARGET): $(OBJ)
	$(CC) $(FLAGS) $(OBJ) -o $@ $(LDFLAGS)

$(OBJ_PATH)/%.o: $(SRC_PATH)/%.c
	$(CC) $(FLAGS) -c $< -o $@

$(DBG_PATH)/%.o: $(SRC_PATH)/%.c
	$(CC) $(FLAGS) -c -g $< -o $@

$(TARGET_DEBUG): $(OBJ_DEBUG)
	$(CC) $(FLAGS) -g $< -o $@

# phony rules
.PHONY: dir
dir:
	@mkdir -p $(BIN_PATH) $(OBJ_PATH) $(DBG_PATH) $(LIB_PATH)

.PHONY: all
all: $(TARGET)

.PHONY: debug
debug: $(TARGET_DEBUG)

# tests rules
TESTS_PATH := tests

SRC_TESTS := $(wildcard $(TESTS_PATH)/*.c)
TARGETS_TESTS := $(addprefix $(TESTS_PATH)/bin/, $(notdir $(basename $(SRC_TESTS))))

$(TESTS_PATH)/bin/%: $(TESTS_PATH)/%.c
	$(CC) $(FLAGS) $< -o $@

.PHONY: test
test: $(TESTS_PATH)/bin $(TARGETS_TESTS)
	@for test in $(TARGETS_TESTS) ; do ./$$test ; done

$(TESTS_PATH)/bin:
	mkdir -p $@


# clean files list
CLEAN_LIST := $(TARGET) \
			  $(TARGET_DEBUG) \
			  $(TARGETS_TESTS) \
			  $(OBJ) \
			  $(OBJ_DEBUG)

.PHONY: clean
clean:
	@echo CLEAN
	@rm -f $(CLEAN_LIST)
	@rm -d $(BIN_PATH) $(OBJ_PATH) $(DBG_PATH) $(TESTS_PATH)/bin
