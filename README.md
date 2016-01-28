**DISCLAIMER: This is not an official Google product.**

## What does this do?

This is a tool that traces the build commands used to compile source code, and
creates a graph of the dependencies between files and the compiler flags used to
produce outputs. It can then generate equivalent build files in another build
system such as [Bazel](http://bazel.io/) or [Ninja](https://ninja-build.org/).

## What could it be used for?

* Automatically convert a project from one build system to another.
  * Compile a library statically even if the original build system doesn't
    support it.
  * Easier cross-compilation.
* Verify all #include'd headers are coming from the place you expect.  Eg. the
  sysroot when doing hermetic builds.
* Easier integration with code indexers such as [Kythe](https://www.kythe.io/).

## Current status

This project is still in very early stages.  It's a proof of concept that works
on some packages, but don't expect it to work perfectly on everything.
