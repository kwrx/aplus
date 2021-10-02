# aplus `#os`
[![deploy](https://github.com/kwrx/aplus/actions/workflows/deploy.yml/badge.svg)](https://github.com/kwrx/aplus/actions/workflows/deploy.yml)
[![GitHub release (latest by date)](https://img.shields.io/github/v/release/kwrx/aplus)](https://github.com/kwrx/aplus/releases/latest)
[![License: GPL](https://img.shields.io/badge/License-GPL-blue.svg)](/LICENSE) 

*A hobbyist operating system built mostly from scratch with a unix-like, hybrid and cross-platform kernel.*

## :zap: Running
1. Open your terminal and type:

```bash
$ ./extra/utils/gen-grubcfg root
```

2. Generate .img file
```bash
$ ./extra/utils/gen-image root aplus.img
```

3. Run it
```bash
$ ./extra/utils/run-qemu TARGET
```

**NOTE:** replace `TARGET` with the target build: `i686`, `x86_64`, etc.

---

See [GitHub Repository](https://github.com/kwrx/aplus) for more information.