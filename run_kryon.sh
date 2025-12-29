#!/bin/bash
# Helper script to run kryon with the latest build libraries
export LD_LIBRARY_PATH=/mnt/storage/Projects/kryon/build:$LD_LIBRARY_PATH
exec kryon "$@"
