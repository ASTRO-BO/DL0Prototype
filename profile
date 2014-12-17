#!/usr/bin/env bash

CTATOOLSSRC=$HOME/CTATools/trunk
RTAPROTO2=$HOME/local.rtaproto2

export CPLUS_INCLUDE_PATH=$CPLUS_INCLUDE_PATH:$CTATOOLSSRC:$CTATOOLSSRC/Core:$CTATOOLSSRC/Build/Core:$RTAPROTO2/include
export LIBRARY_PATH=$LIBRARY_PATH:$CTATOOLSSRC/Build/Core:$RTAPROTO2/lib
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$CTATOOLSSRC/Build/Core:$RTAPROTO2/lib
