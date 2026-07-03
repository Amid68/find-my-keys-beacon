# Host-side test build. Uses the native compiler, not the ARM cross compiler:
# the point is to run src/siphash.c on *this* machine against known vectors.
CC     ?= cc
CFLAGS := -std=c11 -O2 -Wall -Wextra -Werror

BUILD := build

.PHONY: run
run: $(BUILD)/test_siphash
	./$(BUILD)/test_siphash

$(BUILD)/test_siphash: test_siphash.c ../src/siphash.c ../src/siphash.h
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) test_siphash.c ../src/siphash.c -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD)
