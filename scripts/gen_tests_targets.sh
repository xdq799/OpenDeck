#!/bin/bash

if [ "$(uname)" == "Darwin" ]; then
    grep=ggrep
elif [ "$(expr substr $(uname -s) 1 5)" == "Linux" ]; then
    grep=grep
fi

tests=$($grep -Eo 'SOURCES_.+\b' Sources.mk | cut -c 9- | tr '\n' ' ')

printf "" > Objects.mk

for test in $tests
do
    printf '%s\n' 'OBJECTS_'${test}' := $(addprefix build/,$(SOURCES_'${test}'))' >> Objects.mk
    printf '%s\n' 'OBJECTS_'${test}' := $(OBJECTS_'${test}':.c=.o)' >> Objects.mk
    printf '%s\n' 'OBJECTS_'${test}' := $(OBJECTS_'${test}':.cpp=.o)' >> Objects.mk
    printf '%s\n\n' '-include $(OBJECTS_'${test}':%.o=%.d)' >> Objects.mk
done

for test in $tests
do
    printf '%s\n' 'build/'${test}'.out: $(OBJECTS_'${test}') build/gtest_main.a' >> Objects.mk
    printf '\t%s\n' '@echo Linking' >> Objects.mk
    printf '\t%s\n' '@$(CXX) $(CPPFLAGS) $(CXXFLAGS) -lpthread $^ -o $@' >> Objects.mk
    printf '\t%s\n' '@echo Created executable: $@' >> Objects.mk
    printf '\t%s\n\n' '@cp build/'${test}'.out build/test.out' >> Objects.mk
done