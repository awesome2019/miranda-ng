set tp=%1
if  "%tp%"=="" (echo "please specify target platform 32 or 64!"&&pause&&goto :EOF)
if /i '%tp%' == '64' set bt=64
if /i '%tp%' == '32' (
  set ptr=Win32
) else if /i '%tp%' == '64' (
  set ptr=X64
)

call "%ProgramFiles(x86)%\Microsoft Visual Studio\2017\Community\Common7\Tools\VsDevCmd.bat"


MsBuild.exe "fwin8_10.sln" /m /t:Rebuild /p:Configuration=Release;Platform="%ptr%" /fileLogger /fileLoggerParameters:LogFile=Logs\fwin8_10%tp%.log;errorsonly;warningsonly;summary

exit
