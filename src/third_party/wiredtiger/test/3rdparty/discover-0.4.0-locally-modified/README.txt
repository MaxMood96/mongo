WiredTiger Team Changes
=======================

* Replace the use of distutils with setuptools in setup.py as distutils
  is deprecated and removed in Python 3.12.

discover-0.4.0
==============

This is the test discovery mechanism and ``load_tests`` protocol for unittest
backported from Python 2.7 to work with Python 2.4 or more recent (including 
Python 3).

.. note::

    Test discovery is just part of what is new in unittest in Python 2.7. All
    of the new features have been backported to run on Python 2.4-2.6, including
    test discovery. This is the 
    `unittest2 package <http://pypi.python.org/pypi/unittest2>`_.

discover can be installed with pip or easy_install. After installing switch the
current directory to the top level directory of your project and run::

   python -m discover
   python discover.py
  
(If you have setuptools or `distribute <http://pypi.python.org/pypi/distribute>`_
installed you will also have a ``discover`` script available.)

This will discover all tests (with certain restrictions) from the current
directory. The discover module has several options to control its behavior (full
usage options are displayed with ``python -m discover -h``)::

    Usage: discover.py [options]

    Options:
      -v, --verbose    Verbose output
      -s directory     Directory to start discovery ('.' default)
      -p pattern       Pattern to match test files ('test*.py' default)
      -t directory     Top level directory of project (default to
                       start directory)

    For test discovery all test modules must be importable from the top
    level directory of the project.

For example to use a different pattern for matching test modules run::

    python -m discover -p '*test.py'

(For UNIX-like shells like Bash you need to put quotes around the test pattern
or the shell will attempt to expand the pattern instead of passing it through to
discover. On Windows you must *not* put quotes around the pattern as the
Windows shell will pass the quotes to discover as well.)

Test discovery is implemented in ``discover.DiscoveringTestLoader.discover``. As
well as using discover as a command line script you can import
``DiscoveringTestLoader``, which is a subclass of ``unittest.TestLoader``, and
use it in your test framework.

This method finds and returns all test modules from the specified start
directory, recursing into subdirectories to find them. Only test files that
match *pattern* will be loaded. (Using shell style pattern matching.)

All test modules must be importable from the top level of the project. If
the start directory is not the top level directory then the top level
directory must be specified separately.

The ``load_tests`` protocol allows test modules and packages to customize how
they are loaded. This is implemented in
``discover.DiscoveringTestLoader.loadTestsFromModule``. If a test module defines
a ``load_tests`` function then tests are loaded from the module by calling
``load_tests`` with three arguments: `loader`, `standard_tests`, `None`.

If a test package name (directory with `__init__.py`) matches the
pattern then the package will be checked for a ``load_tests``
function. If this exists then it will be called with *loader*, *tests*,
*pattern*.

.. note::

    The default pattern for matching tests is ``test*.py``. The '.py' means
    that it will match test files and *not* match package names. You can
    change this by changing the pattern using a command line option like
    ``-p 'test*'``.

If ``load_tests`` exists then discovery does  *not* recurse into the package,
``load_tests`` is responsible for loading all tests in the package.

The pattern is deliberately not stored as a loader attribute so that
packages can continue discovery themselves. *top_level_dir* is stored so
``load_tests`` does not need to pass this argument in to
``loader.discover()``.

discover.py is maintained in a google code project (where bugs and feature
requests should be posted): http://code.google.com/p/unittest-ext/

The latest development version of discover.py can be found at:
http://code.google.com/p/unittest-ext/source/browse/trunk/discover.py

CHANGELOG
=========

2010/06/11 0.4.0
----------------

* Addition of a setuptools compatible test collector. Set
  "test_suite = 'discover.collector'" in setup.py. "setup.py test" will start
  test discovery with default parameters from the same directory as the setup.py.
* Allow test discovery using dotted module names instead of a path.
* Addition of a setuptools compatible entrypoint for the discover script.
* A faulty load_tests function will not halt test discovery. A failing test
  is created to report the error.
* If test discovery imports a module from the wrong location (usually because
  the module is globally installed and the user is expecting to run tests
  against a development version in a different location) then discovery halts
  with an ImportError and the problem is reported.
* Matching files during test discovery is done in
  ``DiscoveringTestLoader._match_path``. This method can be overriden in
  subclasses to, for example, match on the full file path or use regular
  expressions for matching.
* Tests for discovery ported from unittest2. (The tests require unittest2 to
  run.)

Feature parity with the ``TestLoader`` in Python 2.7 RC 1.


2010/02/07 0.3.2
----------------

* If ``load_tests`` exists it is passed the standard tests as a ``TestSuite`` 
  rather than a list of tests.

2009/09/13 0.3.1
----------------

* Fixed a problem when a package directory matches the discovery pattern.

2009/08/20 0.3.0
----------------

* Failing to import a file (e.g. due to a syntax error) no longer halts
  discovery but is reported as a failure.
* Discovery will not attempt to import test files whose names are not valid Python
  identifiers, even if they match the pattern.