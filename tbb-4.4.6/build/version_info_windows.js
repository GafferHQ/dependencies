// Copyright 2005-2016 Intel Corporation.  All Rights Reserved.
//
// This file is part of Threading Building Blocks. Threading Building Blocks is free software;
// you can redistribute it and/or modify it under the terms of the GNU General Public License
// version 2  as  published  by  the  Free Software Foundation.  Threading Building Blocks is
// distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the
// implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
// See  the GNU General Public License for more details.   You should have received a copy of
// the  GNU General Public License along with Threading Building Blocks; if not, write to the
// Free Software Foundation, Inc.,  51 Franklin St,  Fifth Floor,  Boston,  MA 02110-1301 USA
//
// As a special exception,  you may use this file  as part of a free software library without
// restriction.  Specifically,  if other files instantiate templates  or use macros or inline
// functions from this file, or you compile this file and link it with other files to produce
// an executable,  this file does not by itself cause the resulting executable to be covered
// by the GNU General Public License. This exception does not however invalidate any other
// reasons why the executable file might be covered by the GNU General Public License.

var WshShell = WScript.CreateObject("WScript.Shell");

var tmpExec;

WScript.Echo("#define __TBB_VERSION_STRINGS(N) \\");

//Getting BUILD_HOST
WScript.echo( "#N \": BUILD_HOST\\t\\t" + 
              WshShell.ExpandEnvironmentStrings("%COMPUTERNAME%") +
              "\" ENDL \\" );

//Getting BUILD_OS
tmpExec = WshShell.Exec("cmd /c ver");
while ( tmpExec.Status == 0 ) {
    WScript.Sleep(100);
}
tmpExec.StdOut.ReadLine();

WScript.echo( "#N \": BUILD_OS\\t\\t" + 
              tmpExec.StdOut.ReadLine() +
              "\" ENDL \\" );

if ( WScript.Arguments(0).toLowerCase().match("gcc") ) {
    tmpExec = WshShell.Exec(WScript.Arguments(0) + " --version");
    WScript.echo( "#N \": BUILD_GCC\\t" + 
                  tmpExec.StdOut.ReadLine() + 
                  "\" ENDL \\" );

} else { // MS / Intel compilers
    //Getting BUILD_CL
    tmpExec = WshShell.Exec("cmd /c echo #define 0 0>empty.cpp");
    tmpExec = WshShell.Exec("cl -c empty.cpp ");
    while ( tmpExec.Status == 0 ) {
        WScript.Sleep(100);
    }
    var clVersion = tmpExec.StdErr.ReadLine();
    WScript.echo( "#N \": BUILD_CL\\t\\t" + 
                  clVersion +
                  "\" ENDL \\" );

    //Getting BUILD_COMPILER
    if ( WScript.Arguments(0).toLowerCase().match("icl") ) {
        tmpExec = WshShell.Exec("icl -c empty.cpp ");
        while ( tmpExec.Status == 0 ) {
            WScript.Sleep(100);
        }
        WScript.echo( "#N \": BUILD_COMPILER\\t" + 
                      tmpExec.StdErr.ReadLine() + 
                      "\" ENDL \\" );
    } else {
        WScript.echo( "#N \": BUILD_COMPILER\\t\\t" + 
                      clVersion +
                      "\" ENDL \\" );
    }
    tmpExec = WshShell.Exec("cmd /c del /F /Q empty.obj empty.cpp");
}

//Getting BUILD_TARGET
WScript.echo( "#N \": BUILD_TARGET\\t" + 
              WScript.Arguments(1) + 
              "\" ENDL \\" );

//Getting BUILD_COMMAND
WScript.echo( "#N \": BUILD_COMMAND\\t" + WScript.Arguments(2) + "\" ENDL" );

//Getting __TBB_DATETIME and __TBB_VERSION_YMD
var date = new Date();
WScript.echo( "#define __TBB_DATETIME \"" + date.toUTCString() + "\"" );
WScript.echo( "#define __TBB_VERSION_YMD " + date.getUTCFullYear() + ", " + 
              (date.getUTCMonth() > 8 ? (date.getUTCMonth()+1):("0"+(date.getUTCMonth()+1))) + 
              (date.getUTCDate() > 9 ? date.getUTCDate():("0"+date.getUTCDate())) );



