# Microsoft Developer Studio Project File - Name="spserver" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=spserver - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "spserver.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "spserver.mak" CFG="spserver - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "spserver - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "spserver - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "spserver - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "Release"
# PROP Intermediate_Dir "Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD CPP /nologo /MD /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_MBCS" /D "_LIB" /YX /FD /c
# ADD BASE RSC /l 0x804 /d "NDEBUG"
# ADD RSC /l 0x804 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "spserver - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "Debug"
# PROP Intermediate_Dir "Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /YX /FD /GZ /c
# ADD CPP /nologo /MDd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_MBCS" /D "_LIB" /FR /YX /FD /GZ /c
# ADD BASE RSC /l 0x804 /d "_DEBUG"
# ADD RSC /l 0x804 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "spserver - Win32 Release"
# Name "spserver - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\spserver\spbuffer.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spexecutor.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\sphandler.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\sphttp.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\sphttpmsg.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spiochannel.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spiocpdispatcher.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spiocpevent.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spiocplfserver.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spiocpserver.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spioutils.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spmsgblock.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spmsgdecoder.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\sprequest.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spresponse.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spsession.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spsmtp.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spthread.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spthreadpool.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\sputils.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spwin32buffer.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spwin32iocp.cpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spwin32port.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\spserver\spbuffer.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spexecutor.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\sphandler.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\sphttp.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\sphttpmsg.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spiochannel.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spiocpdispatcher.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spiocpevent.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spiocplfserver.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spiocpserver.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spioutils.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spmsgblock.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spmsgdecoder.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spporting.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\sprequest.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spresponse.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spsession.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spsmtp.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spthread.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spthreadpool.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\sputils.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spwin32buffer.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spwin32iocp.hpp
# End Source File
# Begin Source File

SOURCE=..\spserver\spwin32port.hpp
# End Source File
# End Group
# End Target
# End Project
