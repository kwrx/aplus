@echo off

cd usr\apps
call build.bat

cd ..\..
cd usr\lib
call build.bat

cd ..\..
cd usr\modules
call build.bat

