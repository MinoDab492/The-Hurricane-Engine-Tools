Set objArgs = WScript.Arguments 
InputFolder = objArgs(0) 
ZipFile = objArgs(1) 
Set fso = WScript.CreateObject("Scripting.FileSystemObject") 
Set objZipFile = fso.CreateTextFile(ZipFile, True) 
objZipFile.Write "PK" & Chr(5) & Chr(6) & String(18, vbNullChar) 
objZipFile.Close 
Set objShell = WScript.CreateObject("Shell.Application") 
Set source = objShell.NameSpace(InputFolder).Items 
Set objZip = objShell.NameSpace(fso.GetAbsolutePathName(ZipFile)) 
if not (objZip is nothing) then  
   objZip.CopyHere(source) 
   wScript.Sleep 12000 
end if 
