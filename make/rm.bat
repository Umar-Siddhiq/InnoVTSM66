@ECHO OFF

:next
IF "%~1" == "" EXIT /B 0

IF /I "%~1" == "-f" (
    SHIFT
    GOTO next
)

SET "RM_TARGET=%~1"
SET "RM_TARGET=%RM_TARGET:/=\%"

IF EXIST "%RM_TARGET%" (
    DEL /F /Q "%RM_TARGET%" >NUL 2>NUL
)

SHIFT
GOTO next
