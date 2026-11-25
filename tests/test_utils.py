#!/usr/bin/env python3
"""Compare utilities with system commands using isolated temporary files."""
import os
from pathlib import Path
import pty
import re
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
PROGRAM_DIRS = {"mycat": "lab_1", "mygrep": "lab_1",
                "myls": "lab_1", "mychmod": "lab_1"}


def executable_path(tool):
    return ROOT / PROGRAM_DIRS[tool] / tool


ENV = dict(os.environ, LC_ALL="C", TZ="UTC")
for key in ("BLOCK_SIZE", "BLOCKSIZE", "LS_BLOCK_SIZE", "POSIXLY_CORRECT", "CLICOLOR", "CLICOLOR_FORCE"):
    ENV.pop(key, None)


def run(tool, args=(), data=None, cwd=None, mask=None):
    executable = str(executable_path(tool)) if tool.startswith("my") else tool
    return subprocess.run([executable, *map(str, args)], input=data,
                          stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                          env=ENV, cwd=cwd,
                          preexec_fn=(lambda: os.umask(mask)) if mask is not None else None)


class Utilities(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="unix-lab-")
        self.dir = Path(self.temp.name)
        self.a = self.dir / "a.txt"
        self.a.write_bytes(b"alpha\n\nBeta\nalpha beta\nlast")
        self.b = self.dir / "b.txt"
        self.b.write_bytes(b"line\n\n\nend\n")

    def tearDown(self):
        self.temp.cleanup()

    def equal_command(self, ours, system, args, data=None, cwd=None):
        actual, expected = run(ours, args, data, cwd), run(system, args, data, cwd)
        self.assertEqual(actual.returncode, expected.returncode,
                         (args, actual.stderr, expected.stderr))
        self.assertEqual(actual.stdout, expected.stdout, args)

    def test_cat_numbering(self):
        for flags in ([], ["-n"], ["-b"], ["-nb"], ["-bn"]):
            for files in ([self.a], [self.a, self.b], [self.b, self.a]):
                with self.subTest(flags=flags, files=files):
                    # GNU cat maintains one stream across file boundaries.
                    expected = run("cat", flags, b"".join(f.read_bytes() for f in files))
                    actual = run("mycat", [*flags, *files])
                    self.assertEqual(actual.stdout, expected.stdout)
                    self.assertEqual(actual.returncode, 0)

    def test_cat_ends(self):
        for flag in ("-E", "-nE", "-bE", "-nbE"):
            actual = run("mycat", [flag, self.a]).stdout
            if sys.platform.startswith("linux"):
                expected = run("cat", [flag, self.a]).stdout
            else:
                base = flag.replace("E", "")
                expected = run("cat", ([base] if base != "-" else []) + [self.a]).stdout
                expected = expected.replace(b"\n", b"$\n")
            self.assertEqual(actual, expected)

    def test_cat_stdin_binary_and_errors(self):
        data = b"a\x00b\xff\n\nlast"
        self.equal_command("mycat", "cat", [], data)
        self.assertEqual(run("mycat", ["-n", "-", self.b], data).stdout,
                         run("cat", ["-n"], data + self.b.read_bytes()).stdout)
        result = run("mycat", [self.dir / "missing", self.b])
        self.assertEqual(result.returncode, 1)
        self.assertEqual(result.stdout, self.b.read_bytes())
        self.assertNotEqual(run("mycat", ["-z"]).returncode, 0)

    def test_cat_option_placement(self):
        self.assertEqual(run("mycat", [self.a, "-nE"]).stdout,
                         run("mycat", ["-nE", self.a]).stdout)
        (self.dir / "-n").write_text("literal\n")
        self.assertEqual(run("mycat", ["--", "-n"], cwd=self.dir).stdout, b"literal\n")

    def test_grep_patterns_and_options(self):
        for pattern in ("alpha", "^a", "a$", "^$", "[a-z]*", "", r"\(alpha\)", "missing"):
            for flags in ([], ["-i"], ["-v"], ["-n"], ["-c"], ["-ivn"], ["-q"]):
                with self.subTest(pattern=pattern, flags=flags):
                    self.equal_command("mygrep", "grep", [*flags, pattern, self.a])
        self.equal_command("mygrep", "grep", ["-En", "alpha|Beta", self.a, self.b])

    def test_grep_stdin_and_errors(self):
        for data in (b"", b"one\ntwo", b"\n\n", b"alpha\nno\nalpha"):
            self.equal_command("mygrep", "grep", ["-n", "a"], data)
        self.equal_command("mygrep", "grep", ["alpha", self.a, "-"], b"alpha\n")
        self.assertEqual(run("mygrep", ["[", self.a]).returncode, 2)
        self.assertEqual(run("mygrep", ["a", self.dir / "missing"]).returncode, 2)
        self.assertEqual(run("mygrep").returncode, 2)
        self.assertEqual(run("mygrep", ["a", self.dir / "missing", self.a]).returncode, 2)
        self.assertEqual(run("mygrep", ["-q", "a", self.dir / "missing", self.a]).returncode, 0)

    def test_real_pipeline(self):
        first = subprocess.Popen([str(executable_path("mycat")), str(self.a)], stdout=subprocess.PIPE)
        second = subprocess.run([str(executable_path("mygrep")), "alpha"], stdin=first.stdout,
                                stdout=subprocess.PIPE, env=ENV)
        first.stdout.close()
        self.assertEqual(first.wait(), 0)
        self.assertEqual(second.returncode, 0)
        self.assertEqual(second.stdout, b"alpha\nalpha beta\n")

    def populate_ls(self):
        (self.dir / ".hidden").write_text("secret\n")
        (self.dir / "folder").mkdir()
        (self.dir / "empty").touch()
        (self.dir / "run").write_text("#!/bin/sh\n")
        (self.dir / "run").chmod(0o755)
        (self.dir / "link").symlink_to("a.txt")
        (self.dir / "broken").symlink_to("missing")
        os.mkfifo(self.dir / "pipe")

    def test_ls_names_and_sorting(self):
        self.populate_ls()
        for args in ([], ["-a"], [self.a, self.b], ["-a", self.dir], ["-a", "folder", "."]):
            with self.subTest(args=args):
                self.equal_command("myls", "ls", args, cwd=self.dir)
        for args in (["-la", self.dir], ["-l", self.dir, "-a"], [self.dir, "-la"]):
            self.assertEqual(run("myls", args).stdout, run("myls", ["-la", self.dir]).stdout)

    def test_ls_long(self):
        self.populate_ls()
        for flags in ("-l", "-la"):
            actual, expected = run("myls", [flags, self.dir]), run("ls", [flags, self.dir])
            self.assertEqual(actual.returncode, 0)
            # Linux: compare exact bytes; BSD ls uses different column spacing.
            if sys.platform.startswith("linux"):
                self.assertEqual(actual.stdout, expected.stdout)
            else:
                normalized = re.sub(rb"(?m)^([bcdlps-][rwxstST-]{9})[@+]", rb"\1", expected.stdout)
                self.assertEqual(actual.stdout.split(), normalized.split())
        (self.dir / "empty_dir").mkdir()
        self.equal_command("myls", "ls", ["-l", self.dir / "empty_dir"])

    def test_ls_links_operands_and_errors(self):
        self.populate_ls()
        (self.dir / "dirlink").symlink_to("folder")
        self.equal_command("myls", "ls", [self.dir / "dirlink"])
        result = run("myls", [self.dir / "missing", self.a])
        self.assertNotEqual(result.returncode, 0)
        self.assertIn(str(self.a).encode(), result.stdout)
        self.assertNotEqual(run("myls", ["-z"]).returncode, 0)
        (self.dir / "-l").touch()
        self.equal_command("myls", "ls", ["--", "-l"], cwd=self.dir)

    def test_ls_terminal_colors(self):
        self.populate_ls()
        master, slave = pty.openpty()
        try:
            child = subprocess.Popen([str(executable_path("myls")), "-l", str(self.dir)],
                                     stdout=slave, stderr=subprocess.PIPE, env=ENV)
            os.close(slave)
            slave = None
            chunks = []
            while True:
                try:
                    data = os.read(master, 4096)
                except OSError:
                    break
                if not data:
                    break
                chunks.append(data)
            self.assertEqual(child.wait(), 0)
            child.stderr.close()
            output = b"".join(chunks)
            for code, name in ((b"34", b"folder"), (b"32", b"run"), (b"36", b"link")):
                self.assertIn(b"\x1b[" + code + b"m" + name + b"\x1b[0m", output)
            self.assertNotIn(b"\x1b[", run("myls", [self.dir]).stdout)
        finally:
            os.close(master)
            if slave is not None:
                os.close(slave)

    def test_chmod_examples_and_symbolic(self):
        modes = ["+x", "u-r", "g+rw", "ug+rw", "uga+rwx", "766", "0644", "0",
                 "u=rw,g=r,o=", "a-x", "u+x,g-w,o+r", "g=u", "o+g", "u+r-w+x",
                 "a+X", "a=", "u+", "u=rx", "u=rw,g=u,o=g"]
        starts = [0o000, 0o644, 0o751, 0o777]
        if sys.platform.startswith("linux"):
            modes += ["u+s", "g+s", "o+t"]
            starts += [0o4755, 0o2770]
        for start in starts:
            for mode in modes:
                with self.subTest(start=oct(start), mode=mode):
                    self.a.chmod(start)
                    self.b.chmod(start)
                    actual = run("mychmod", [mode, self.a], mask=0o022)
                    expected = run("chmod", [mode, self.b], mask=0o022)
                    self.assertEqual(actual.returncode, expected.returncode, (actual.stderr, expected.stderr))
                    self.assertEqual(self.a.stat().st_mode & 0o7777, self.b.stat().st_mode & 0o7777)

    def test_chmod_umask(self):
        for mask in (0, 0o022, 0o077, 0o777):
            for mode in ("+rwx", "-w", "=rw", "+X", "a+rw", "u+rw"):
                with self.subTest(mask=oct(mask), mode=mode):
                    self.a.chmod(0o764)
                    self.b.chmod(0o764)
                    actual = run("mychmod", [mode, self.a], mask=mask)
                    expected = run("chmod", ["--", mode, self.b], mask=mask)
                    self.assertEqual(actual.returncode, 0, actual.stderr)
                    # Some GNU chmod versions warn and return 1 for a leading
                    # minus mode when umask prevents the naive removal, even
                    # with --. The change still succeeds; verify its exact bits.
                    warning = (sys.platform.startswith("linux") and mode.startswith("-")
                               and expected.returncode == 1
                               and b"new permissions are " in expected.stderr
                               and b", not " in expected.stderr)
                    if not warning:
                        self.assertEqual(expected.returncode, 0, expected.stderr)
                    self.assertEqual(self.a.stat().st_mode & 0o7777, self.b.stat().st_mode & 0o7777)

    def test_chmod_errors_multiple_and_symlink(self):
        for mode in ("", "888", "10000", "u+z", "u", "u+r,", "u+gr", ",u+r"):
            before = self.a.stat().st_mode
            self.assertNotEqual(run("mychmod", [mode, self.a]).returncode, 0, mode)
            self.assertEqual(self.a.stat().st_mode, before)
        result = run("mychmod", ["600", self.a, self.dir / "missing", self.b])
        self.assertEqual(result.returncode, 1)
        self.assertEqual(self.a.stat().st_mode & 0o777, 0o600)
        self.assertEqual(self.b.stat().st_mode & 0o777, 0o600)
        link = self.dir / "link"
        link.symlink_to(self.a)
        self.assertEqual(run("mychmod", ["640", link]).returncode, 0)
        self.assertEqual(self.a.stat().st_mode & 0o777, 0o640)
        folder = self.dir / "folder"
        folder.mkdir()
        folder.chmod(0o600)
        self.assertEqual(run("mychmod", ["a+X", folder]).returncode, 0)
        self.assertEqual(folder.stat().st_mode & 0o777, 0o711)


if __name__ == "__main__":
    unittest.main(verbosity=2)
