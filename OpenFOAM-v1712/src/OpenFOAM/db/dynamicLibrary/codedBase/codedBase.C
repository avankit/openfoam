/*---------------------------------------------------------------------------*\
  =========                 |
  \\      /  F ield         | OpenFOAM: The Open Source CFD Toolbox
   \\    /   O peration     |
    \\  /    A nd           | Copyright (C) 2011-2016 OpenFOAM Foundation
     \\/     M anipulation  | Copyright (C) 2016 OpenCFD Ltd.
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software: you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM.  If not, see <http://www.gnu.org/licenses/>.

\*---------------------------------------------------------------------------*/

#include "codedBase.H"
#include "SHA1Digest.H"
#include "dynamicCode.H"
#include "dynamicCodeContext.H"
#include "dlLibraryTable.H"
#include "Pstream.H"
#include "PstreamReduceOps.H"
#include "OSspecific.H"
#include "Ostream.H"
#include "regIOobject.H"

// * * * * * * * * * * * * * * Static Data Members * * * * * * * * * * * * * //

namespace Foam
{
    defineTypeNameAndDebug(codedBase, 0);
}


// * * * * * * * * * * * * * Static Member Functions * * * * * * * * * * * * //

namespace Foam
{
//! \cond fileScope
static inline void writeEntryIfPresent
(
    Ostream& os,
    const dictionary& dict,
    const word& key
)
{
    // non-recursive like dictionary::found, but no pattern-match either
    const entry* ptr = dict.lookupEntryPtr(key, false, false);

    if (ptr)
    {
        os.writeKeyword(key)
            << token::HASH << token::BEGIN_BLOCK;

        os.writeQuoted(string(ptr->stream()), false)
            << token::HASH << token::END_BLOCK
            << token::END_STATEMENT << nl;
    }
}
//! \endcond
}


void Foam::codedBase::writeCodeDict(Ostream& os, const dictionary& dict)
{
    writeEntryIfPresent(os, dict, "codeInclude");
    writeEntryIfPresent(os, dict, "localCode");
    writeEntryIfPresent(os, dict, "code");
    writeEntryIfPresent(os, dict, "codeOptions");
    writeEntryIfPresent(os, dict, "codeLibs");
}


// * * * * * * * * * * * * * Private Member Functions  * * * * * * * * * * * //

void* Foam::codedBase::loadLibrary
(
    const fileName& libPath,
    const string& globalFuncName,
    const dictionary& contextDict
) const
{
    void* lib = 0;

    // avoid compilation by loading an existing library
    if (!libPath.empty())
    {
        if (libs().open(libPath, false))
        {
            lib = libs().findLibrary(libPath);

            // verify the loaded version and unload if needed
            if (lib)
            {
                // provision for manual execution of code after loading
                if (dlSymFound(lib, globalFuncName))
                {
                    loaderFunctionType function =
                        reinterpret_cast<loaderFunctionType>
                        (
                            dlSym(lib, globalFuncName)
                        );

                    if (function)
                    {
                        (*function)(true);    // force load
                    }
                    else
                    {
                        FatalIOErrorInFunction
                        (
                            contextDict
                        )   << "Failed looking up symbol " << globalFuncName
                            << nl << "from " << libPath << exit(FatalIOError);
                    }
                }
                else
                {
                    FatalIOErrorInFunction
                    (
                        contextDict
                    )   << "Failed looking up symbol " << globalFuncName << nl
                        << "from " << libPath << exit(FatalIOError);

                    lib = 0;
                    if (!libs().close(libPath, false))
                    {
                        FatalIOErrorInFunction
                        (
                            contextDict
                        )   << "Failed unloading library "
                            << libPath
                            << exit(FatalIOError);
                    }
                }
            }
        }
    }

    return lib;
}


void Foam::codedBase::unloadLibrary
(
    const fileName& libPath,
    const string& globalFuncName,
    const dictionary& contextDict
) const
{
    void* lib = 0;

    if (libPath.empty())
    {
        return;
    }

    lib = libs().findLibrary(libPath);

    if (!lib)
    {
        return;
    }

    // provision for manual execution of code before unloading
    if (dlSymFound(lib, globalFuncName))
    {
        loaderFunctionType function =
            reinterpret_cast<loaderFunctionType>
            (
                dlSym(lib, globalFuncName)
            );

        if (function)
        {
            (*function)(false);    // force unload
        }
        else
        {
            FatalIOErrorInFunction
            (
                contextDict
            )   << "Failed looking up symbol " << globalFuncName << nl
                << "from " << libPath << exit(FatalIOError);
        }
    }

    if (!libs().close(libPath, false))
    {
        FatalIOErrorInFunction
        (
            contextDict
        )   << "Failed unloading library " << libPath
            << exit(FatalIOError);
    }
}


void Foam::codedBase::createLibrary
(
    dynamicCode& dynCode,
    const dynamicCodeContext& context
) const
{
    bool create =
        Pstream::master()
     || (regIOobject::fileModificationSkew <= 0);   // not NFS

    if (create)
    {
        // Write files for new library
        if (!dynCode.upToDate(context))
        {
            // filter with this context
            dynCode.reset(context);

            this->prepare(dynCode, context);

            if (!dynCode.copyOrCreateFiles(true))
            {
                FatalIOErrorInFunction
                (
                    context.dict()
                )   << "Failed writing files for" << nl
                    << dynCode.libRelPath() << nl
                    << exit(FatalIOError);
            }
        }

        if (!dynCode.wmakeLibso())
        {
            FatalIOErrorInFunction
            (
                context.dict()
            )   << "Failed wmake " << dynCode.libRelPath() << nl
                << exit(FatalIOError);
        }
    }


    // all processes must wait for compile to finish
    if (regIOobject::fileModificationSkew > 0)
    {
        //- Since the library has only been compiled on the master the
        //  other nodes need to pick this library up through NFS
        //  We do this by just polling a few times using the
        //  fileModificationSkew.

        const fileName libPath = dynCode.libPath();

        off_t mySize = Foam::fileSize(libPath);
        off_t masterSize = mySize;
        Pstream::scatter(masterSize);

        if (debug)
        {
            Pout<< endl<< "on processor " << Pstream::myProcNo()
                << " have masterSize:" << masterSize
                << " and localSize:" << mySize
                << endl;
        }


        if (mySize < masterSize)
        {
            if (debug)
            {
                Pout<< "Local file " << libPath
                    << " not of same size (" << mySize
                    << ") as master ("
                    << masterSize << "). Waiting for "
                    << regIOobject::fileModificationSkew
                    << " seconds." << endl;
            }
            Foam::sleep(regIOobject::fileModificationSkew);

            // Recheck local size
            mySize = Foam::fileSize(libPath);

            if (mySize < masterSize)
            {
                FatalIOErrorInFunction
                (
                    context.dict()
                )   << "Cannot read (NFS mounted) library " << nl
                    << libPath << nl
                    << "on processor " << Pstream::myProcNo()
                    << " detected size " << mySize
                    << " whereas master size is " << masterSize
                    << " bytes." << nl
                    << "If your case is not NFS mounted"
                    << " (so distributed) set fileModificationSkew"
                    << " to 0"
                    << exit(FatalIOError);
            }
        }

        if (debug)
        {
            Pout<< endl<< "on processor " << Pstream::myProcNo()
                << " after waiting: have masterSize:" << masterSize
                << " and localSize:" << mySize
                << endl;
        }
    }
    reduce(create, orOp<bool>());
}


// * * * * * * * * * * * * Protected Member Functions  * * * * * * * * * * * //

void Foam::codedBase::updateLibrary
(
    const word& name
) const
{
    const dictionary& dict = this->codeDict();

    dynamicCode::checkSecurity
    (
        "codedBase::updateLibrary()",
        dict
    );

    dynamicCodeContext context(dict);

    // codeName: name + _<sha1>
    // codeDir : name
    dynamicCode dynCode
    (
        name + context.sha1().str(true),
        name
    );
    const fileName libPath = dynCode.libPath();


    // the correct library was already loaded => we are done
    if (libs().findLibrary(libPath))
    {
        return;
    }

    Info<< "Using dynamicCode for " << this->description().c_str()
        << " at line " << dict.startLineNumber()
        << " in " << dict.name() << endl;


    // remove instantiation of fvPatchField provided by library
    this->clearRedirect();

    // may need to unload old library
    unloadLibrary
    (
        oldLibPath_,
        dynamicCode::libraryBaseName(oldLibPath_),
        context.dict()
    );

    // try loading an existing library (avoid compilation when possible)
    if (!loadLibrary(libPath, dynCode.codeName(), context.dict()))
    {
        createLibrary(dynCode, context);

        loadLibrary(libPath, dynCode.codeName(), context.dict());
    }

    // retain for future reference
    oldLibPath_ = libPath;
}


// * * * * * * * * * * * * * * * * Constructors  * * * * * * * * * * * * * * //

Foam::codedBase::codedBase()
{}


// * * * * * * * * * * * * * * * * Destructor  * * * * * * * * * * * * * * * //

Foam::codedBase::~codedBase()
{}


// ************************************************************************* //
