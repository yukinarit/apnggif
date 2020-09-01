# apnggif

Python interface to [apng2gif](https://sourceforge.net/projects/apng2gif/).

Both APNG (Animated PNG) are GIF are major animation formats nowadays, however there is no good python library to convert from APNG to GIF. `apnggif` is modern, easy-to-use python conversion library (and CLI) built on top of [apng2gif](https://sourceforge.net/projects/apng2gif/) which is written in C++ and also used in [EZGIF.com](https://ezgif.com/).

## Usage

`apnggif` requires python>=3.6.

```
pip install apnggif
```

### CLI

If you pip install apnggif, a command line client `apnggif` is already installed in your system. Try the following command to convert APNG image to GIF.

```
apnggif <GIF> ...
```

For more information about options, check
```
apnggif --help
```

### In a Python program

```python
from apnggif import apnggif

apnggif("ball.png")
```

## License

[zlib/libpng License](https://opensource.org/licenses/Zlib).