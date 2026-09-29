Flexilink Controller (Legacy MFC Build)

Build Instructions
------------------

1. Install Visual Studio 2022 or later.
   During installation, make sure the following components are included:
   - Desktop development with C++
   - MFC for x86/x64

2. Open the solution file:

   Controllerwithpasswords_src/Controller.sln

3. Build the solution (Debug or Release).

Important Notes
---------------

This is a legacy MFC project.

The solution is configured to use relative include paths to the following
folders located in the project root:

    ../Common
    ../Compiler
    ../VM4 compiler

These paths are already defined inside the project settings
(C/C++ → Additional Include Directories).

As long as the folder structure is preserved, no manual configuration
should be required.

Folder Structure
----------------

ProjectRoot/
│
├── Controllerwithpasswords_src/
│   └── Controller.sln
│
├── Common/
├── Compiler/
└── VM4 compiler/

If the directory layout is changed, include paths must be updated accordingly.

