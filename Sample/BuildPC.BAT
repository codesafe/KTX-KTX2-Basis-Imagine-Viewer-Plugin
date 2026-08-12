@SET OUTPUT=IPS.plg

@CALL PellesC

@SET COMPILER_OPTION=/c /Gm /MT /Ox /Tx86-coff /Ze
@SET COMPILER_OPTION=%COMPILER_OPTION% /I ../..
@SET LINKER_OPTION=/def:Main.DEF /dll /entry:DllMain /out:%OUTPUT% /release /subsystem:windows

@FOR %%F IN (*.c) DO @CALL POCC %COMPILER_OPTION% %%F
@CALL POLink %LINKER_OPTION% *.obj