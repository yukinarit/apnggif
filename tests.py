import pytest
from apnggif import read_input, read_output
from pathlib import Path


def test_read_input():
    assert ["foo"] == read_input("foo")
    assert ["foo", "bar"] == read_input(["foo", "bar"])
    assert ["foo", "bar"] == read_input(("foo", "bar"))
    assert ["/foo"] == read_input(Path("/foo"))
    assert ["/foo", "bar"] == read_input([Path("/foo"), "bar"])
    assert ["/foo", "bar"] == read_input((Path("/foo"), "bar"))


def test_read_output():
    assert ["foo"] == read_output("foo")
    assert ["foo", "bar"] == read_output(["foo", "bar"])
    assert ["foo", "bar"] == read_output(("foo", "bar"))
    assert ["/foo"] == read_output(Path("/foo"))
    assert ["/foo", "bar"] == read_output([Path("/foo"), "bar"])
    assert ["/foo", "bar"] == read_output((Path("/foo"), "bar"))

    assert ["foo", "bar"] == read_output(None, ("foo", "bar"))
    assert ["/foo", "bar"] == read_output(None, (Path("/foo"), "bar"))
    assert ["/foo.gif", "bar.gif"] == read_output(None, (Path("/foo.png"), "bar.png"))
