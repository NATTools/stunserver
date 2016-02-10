[![Build Status](https://travis-ci.org/NATTools/stunserver.svg?branch=master)](https://travis-ci.org/NATTools/stunserver)
[![GitHub version](https://badge.fury.io/gh/NATTools%2Fstunserver.svg)](https://badge.fury.io/gh/NATTools%2Fstunserver)
# stunserver
A simple stunserver implementation. Only suitable to test the NATTool libraries.

## Compiling

`build.sh` does the following

    mkdir build     (hold all of the build files in a separate directory)
    cd build; cmake (create the makefiles)
    make            (To build the code)

You will end up with a an application you can execute in build/dist/bin/stunserver.


## Development

You need to have [cmake](http://www.cmake.org/) to build.
Note that version 3.2 or newer is needed. (Some linux distributions are a bit slow to update, so manuall install may be needed.)




### Coding Standard

Using uncrustify to clean up code. There is a uncrustify.cfg file that describes
the format. Run uncrustify before posting pull requests.

[Semantic versioning](http://semver.org/) is used.

## Contributing

Fork and send pull requests.  Please add your name to the AUTHORS list in your
first patch.

# License

BSD 2-clause:

Copyright (c) 2015, NATTools
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
