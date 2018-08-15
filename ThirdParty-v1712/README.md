<!--
   |--------------------------------------------------------------------------|
   | =========                 |                                              |
   | \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox        |
   |  \\    /   O peration     |                                              |
   |   \\  /    A nd           | Copyright (C) 2016-2017 OpenCFD Ltd.         |
   |    \\/     M anipulation  |                                              |
   |--------------------------------------------------------------------------|
  -->

---

# OpenFOAM&reg; ThirdParty

OpenFOAM depends to a certain extent on third-party libraries
(*opensource only*). It also provides some interfaces to *opensource* or
*proprietary* libraries. This third-party package contains configurations and
scripts for building third-party packages. It should normally only be used in
conjunction with the corresponding OpenFOAM version.

## Configuration of Third-Party Versions

For most of the build scripts, the default software version
is provided by an appropriate OpenFOAM `etc/config.sh/...` entry.
This approach avoids duplicate entries for the default versions and
ensures the best overall consistency between the OpenFOAM installation
and its corresponding third-party installation.

Nonethess, the distributed make scripts can generally be used for a
variety of versions of the third-party libraries, with the software
version specified on the command-line.

---

## Before Starting

0. Review the [system requirements](http://www.openfoam.com/documentation/system-requirements.php)
   and decide on the following:
   * compiler type/version (you may need a third-party compiler installation).
   * MPI type/version.
   * ParaView type/version.
   * CMake type/version, ...
1. Adjust the OpenFOAM `etc/bashrc`, `etc/config.sh/...` or equivalent
   `prefs.sh` files to reflect your preferred configuration.
2. Source the updated OpenFOAM environment

---

## Building

Many components of ThirdParty are *optional* or are invoked
automatically as part of the top-level OpenFOAM `Allwmake`.
Nonetheless it may be necessary or useful to build particular
ThirdParty components prior to building OpenFOAM itself.

### Build Sequence

1. `makeGcc` _or_ `makeLLVM` *(optional)*
2. `makeCmake`  *(optional)*
3. `Allwmake`
   - This will be automatically invoked by the top-level OpenFOAM `Allwmake`.
4. `makeParaView`  *(optional but highly recommended)*
5. Any other additional optional components


### Build Details

More details can be found the ThirdParty BUILD.md information.

<!-- OpenFOAM -->

[link AddOns]: https://develop.openfoam.com/Community/OpenFOAM-addOns
[link community-projects]: http://www.openfoam.com/community/projects.php

---

<!-- Standard Footer -->
## Additional OpenFOAM Links

- [Community AddOns][link AddOns] repository
- [Collaborative and Community-based Developments][link community-projects]
- [Download](http://www.openfoam.com/download) and
  [installation instructions](http://www.openfoam.com/code/build-guide.php)
- [Documentation](http://www.openfoam.com/documentation)
- [Reporting bugs/issues](http://www.openfoam.com/code/bug-reporting.php) (including bugs/suggestions/feature requests) in OpenFOAM
- [Contacting OpenCFD](http://www.openfoam.com/contact)

---

Copyright 2016-2017 OpenCFD Ltd
