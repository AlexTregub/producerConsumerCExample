#!/bin/bash
# Compile programs to binaries (WILL NOT EXECUTE FILES)
gcc producer.c -pthread -lrt -o producer;
gcc consumer.c -pthread -lrt -o consumer;

# File execution should be done via:
# ./producer & ./consumer &
