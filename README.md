# colart

_Stability: 1 - [Experimental](https://github.com/tristanls/stability-index#stability-1---experimental)_

COmbined Lambda/object architecture Actor Run-Time.

**colart** is an initial implementation of an **asynchronous** version of [Open Reusable Object Models](http://www.vpri.org/pdf/tr2006003a_objmod.pdf) built on top of [tart (Tiny Actor Run-Time)](https://github.com/organix/tart) in the spirit of [COmbined Lambda/object Architecture (COLA) framework](http://piumarta.com/software/cola/).

## Usage

To enable/disable trace and memory trace edit the `TRACE` and `TRACE_MEMORY` macros in `tart.h`.

### Build

Take a look insicde `build.sh`. The build was ~~tested~~ run on _debian-7.0.0_ with _llvm 3.4_ installed.

    ./build.sh

### Run

    ./bootstrap.native > trace.log 2>&1