import apnggif_sys
import click
import logging
import coloredlogs
from typing import Union, Tuple, Optional, List
from pathlib import Path

log = logging.getLogger("apnggif")

Input = Union[str, Path, List[str], List[Path]]

Output = Optional[Union[str, Path, List[str], List[Path]]]

Transparency = Optional[int]

Color = Optional[Union[str, Tuple[int, int, int]]]


class Error(Exception):
    """
    Exception class for apnggif.
    """


def apnggif(png: Input, gif: Output = None, tlevel: Transparency = None, bg: Color = None):
    """
    Python interface to apng2gif.
    """
    log.debug(f"apnggif(png={png}, gif={gif}, tlevel={tlevel}, bg={bg})")
    pngs = read_input(png)
    gifs = read_output(gif, pngs)
    if len(pngs) != len(gifs):
        raise Error("png input length != gif output length")

    if tlevel and bg:
        raise Error("You can't set both transparency and bg.")

    if tlevel is None:
        if bg is None:
            tlevel = 128
        else:
            tlevel = -1

    if bg is None:
        bg = ""

    for png, gif in zip(pngs, gifs):
        log.info(f'apnggif("{png}", "{gif}", {tlevel}, "{bg}"))')
        apnggif_sys.apnggif(png, gif, tlevel, bg)


def read_input(png: Input) -> List[str]:
    if not png:
        raise Error("png source is not specified")
    if isinstance(png, (list, tuple)):
        pass
    elif isinstance(png, str):
        png = [Path(png)]
    elif isinstance(png, Path):
        png = [png]
    png = [p if isinstance(p, Path) else Path(p) for p in png]
    return [str(p.resolve()) for p in png]


def read_output(inp: Output, png: Optional[List[Union[str, Path]]] = None) -> List[str]:
    gifs: List = []
    if isinstance(inp, str):
        gifs = [inp]
    elif isinstance(inp, Path):
        gifs = [inp]
    elif isinstance(inp, (list, tuple)):
        gifs = inp
    elif not inp and png:
        for p in png:
            p = str(p.resolve()) if isinstance(p, Path) else p
            if p[-4:].lower() == ".png":
                gifs.append(p[:-4] + ".gif")
            else:
                gifs.append(p)
    return [str(p.resolve()) if isinstance(p, Path) else p for p in gifs]


@click.command()
@click.argument("png", nargs=-1)
@click.option("-t", "--tlevel", type=int, help="Transparency level, defaults to 128")
@click.option("-b", "--bg", type=str, help="Background color")
@click.option("-v", "--verbose", is_flag=True, help="Enable verbose logging")
def main(png, tlevel, bg, verbose):
    """
    apnggif - Convert APNG to GIF.
    """
    coloredlogs.install(
        level="DEBUG" if verbose else "INFO", logger=log, fmt="%(levelname)s %(message)s"
    )
    try:
        apnggif(png, tlevel=tlevel, bg=bg)
    except Exception as e:
        log.error(e)


if __name__ == "__main__":
    main()
