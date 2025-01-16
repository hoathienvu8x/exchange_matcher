CC = gcc
LDFLAGS = -I./includes -ldl -lpthread -lm
ifeq ($(build),release)
	CFLAGS = -O3
	LDFLAGS += -DNDEBUG=1
else
	CFLAGS = -Og -g
endif
CFLAGS += -std=gnu99 -Wall -Wextra -Werror -pedantic
RM = rm -rf

OBJECTS = list_utils.o math_funcs.o orders_matcher.o
OBJECTS := $(addprefix objects/,$(OBJECTS))
EXECUTABLE = exchange_matcher

all: objects $(EXECUTABLE)

objects:
	@echo "Create 'objects' folder ..."
	@mkdir -p objects

$(EXECUTABLE): objects/main.o $(OBJECTS)
ifeq ($(build),release)
	@echo "Build release '$@' executable ..."
else
	@echo "Build '$@' executable ..."
endif
	@$(CC) objects/main.o $(OBJECTS) -o $@ $(LDFLAGS)
	@$(RM) objects/main.o

objects/%.o: src/%.c
	@echo "Build '$@' object ..."
	@$(CC) -c $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	@echo "Cleanup ..."
	@$(RM) $(OBJECTS) $(EXECUTABLE)
