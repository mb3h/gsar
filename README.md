# gsar

![](https://raw.githubusercontent.com/mb3h/gsar/master/gsar.png)

## Design Policy

maintenance model (code paradigm): objective [C99]


## Overview

This is simple cpu load monitor which prots 'system', 'iowait' and 'total' only.
This tool is lightweight, and never depends on other library except for 'libc6' and GTK+.


## Getting Started

```bash
# renew suitable './configure'
aclocal ; autoheader ; automake -a -c ; autoconf

# create Makefile
./configure

# build
make

# startup
gsar [-m <magnify-scale>]
```


## TODO priority / progress

TODO
- [ ] display CPU tag label per graph.
- [x] don't forget window position on each boot.
- [ ] append option which set monitor interval.
- [ ] GTK+3.0 support.
- [ ] GTK+4.0 support.

PENDING


## Notes

README Updated on:2022-10-30
