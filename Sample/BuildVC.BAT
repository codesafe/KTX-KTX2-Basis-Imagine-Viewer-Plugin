@SET OUTPUT=IPS.plg

@CALL MSVS60

@SET COMPILER_OPTION=/c /GF /MT /Ox
@SET COMPILER_OPTION=%COMPILER_OPTION% /I ../..
@SET LINKER_OPTION=/def:Main.DEF /dll /entry:DllMain /opt:nowin98 /out:%OUTPUT% /release /subsystem:windows

@CALL CL %COMPILER_OPTION% *.c
@CALL Link %LINKER_OPTION% *.obj