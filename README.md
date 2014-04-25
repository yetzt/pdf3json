# pdf3json

I was in need of [pdf2json](https://code.google.com/p/pdf2json/) writing to stdout and there was no way to do that properly. Also the code seems to be a mess. So i forked it.

I'm not familiar with C++ and this is, especialy after mer fiddling around, still a mess. But it delivers JSON at stdout.

## Install

Pretty straightforward.

```
make && make install
```

## Usage

```
pdf3json <pdf-file>
```

## License

This software is based on [pdf2json](https://code.google.com/p/pdf2json/) v0.86 which is based on [XPDF](http://www.foolabs.com/xpdf/) 3.02 and therefore licenced under [GNU GPL v2](http://www.gnu.org/licenses/old-licenses/gpl-2.0.html)
